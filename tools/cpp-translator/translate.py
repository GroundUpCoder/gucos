#!/usr/bin/env python3
"""Pinned wasm32 LLVM → C backend and artifact resolver.

LLVM parses/verifies the module and supplies values, types, CFG and DataLayout.
The binding lacks a few instruction properties; those are extracted narrowly
from LLVM's canonical printer, never from unparsed input.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from storage import Storage
import runtime_abi

import llvmlite
from llvmlite import binding as llvm

PROFILE = 'wasm-posix-llvm-c-v3'
SHARED = {'linkonce_odr', 'weak_odr'}
LOCAL = {'internal', 'private'}
SCALARS = {1: '_Bool', 8: 'unsigned char', 16: 'unsigned short',
           32: 'unsigned int', 64: 'unsigned long long'}
SIGNED = {1: 'int', 8: 'signed char', 16: 'short', 32: 'int', 64: 'long long'}
RUNTIME_SIGNATURES = {
    '__cxa_call_terminate': 'void __cxa_call_terminate(void *)',
    '__wp_cxx_match': 'int __wp_cxx_match(void *, void *)',
    'memcpy': 'void * memcpy(void *, const void *, unsigned long)',
    'memmove': 'void * memmove(void *, const void *, unsigned long)',
    'memset': 'void * memset(void *, int, unsigned long)',
    'atan2f': 'float atan2f(float, float)',
    'atan2': 'double atan2(double, double)',
    'powf': 'float powf(float, float)',
    'pow': 'double pow(double, double)',
    'rintf': 'float rintf(float)',
    'rint': 'double rint(double)',
}


class Unsupported(Exception):
    pass


def digest(value):
    return hashlib.sha256(value.encode()).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'))


def require(test, message):
    if not test:
        raise Unsupported(message)


def validate_runtime_cycles(artifacts):
    rows = {row['symbol']: row for row in artifacts}
    for row in rows.values():
        require('runtimeDependencies' in row and
                set(row['runtimeDependencies']) <= set(row['dependencies']) & RUNTIME_SIGNATURES.keys(),
                f'{row["symbol"]}: invalid synthesized runtime dependencies')
    # Three fixed libc helpers: at most three linear graph walks, independent
    # of the number of generated functions. Include unselected definitions.
    for helper in RUNTIME_SIGNATURES.keys() & rows.keys():
        pending, visited = [helper], set()
        while pending:
            name = pending.pop()
            if name in visited or name not in rows:
                continue
            visited.add(name)
            row = rows[name]
            require(helper not in row['runtimeDependencies'],
                    f'{helper}: synthesized runtime dependency cycle through {name}')
            pending.extend(row['dependencies'])


def first_field(text, start=0, head_only=False):
    """Split one canonical LLVM type/value at its first unnested comma."""
    depth = 0
    quoted = False
    escaped = False
    for i in range(start, len(text)):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c in '[{<(':
            depth += 1
        elif c in ']}> )'.replace(' ', ''):
            depth -= 1
        elif c == ',' and depth == 0:
            return text[start:i].strip() if head_only else (text[start:i].strip(), text[i+1:].strip())
    return text[start:].strip() if head_only else (text[start:].strip(), '')



def byval_type(attributes):
    """Read a type attribute without treating quoted-name parentheses as syntax."""
    match = re.search(r'\bbyval\(', attributes)
    if not match:
        return None
    start = match.end()
    depth, quoted, escaped = 1, False, False
    for i in range(start, len(attributes)):
        c = attributes[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return attributes[start:i]
    require(False, 'unterminated byval type attribute')


def declarator(ty, name):
    if '(*)' in ty:
        return ty.replace('(*)', '(*' + name + ')', 1)
    return ty + ' ' + name


def api_types(header, clang, flags):
    if not header:
        return {}
    command = [clang, '--target=wasm32-unknown-unknown', '-x', 'c', '-std=c11',
               '-ffreestanding', *flags, '-fsyntax-only', '-Xclang', '-ast-dump=json', str(header)]
    tree = json.loads(subprocess.check_output(command, text=True))
    result = {}
    aliases = {n['name']: n['type']['qualType'] for n in tree.get('inner', []) if n.get('kind') == 'TypedefDecl'}
    resolved = {}
    def is_va_list(spelling, active=()):
        spelling = re.sub(r'\b(?:const|restrict|__restrict)\b', '', spelling).strip()
        if spelling in {'va_list', '__builtin_va_list', '__isoc_va_list'}:
            return True
        return spelling in aliases and spelling not in active and is_va_list(aliases[spelling], active+(spelling,))
    def expand(spelling, active=()):
        def token(match):
            name = match[0]
            # wasm-posix's C frontend erases enums to int in public signatures.
            # LLVM still verifies the actual 32-bit lowered ABI below.
            if name.startswith('enum '):
                return 'int'
            if name.startswith(('struct ', 'union ')) or name not in aliases:
                return name
            require(name not in active, f'cyclic public C typedef: {name}')
            if name not in resolved:
                resolved[name] = expand(aliases[name], active + (name,))
            return resolved[name]
        expanded = re.sub(r'\b(?:struct|union|enum)\s+\w+|\b\w+\b', token, spelling)
        return re.sub(r'\b(?:restrict|__restrict)\b', '', expanded).strip()
    for node in tree.get('inner', []):
        if node.get('kind') == 'FunctionDecl':
            signature = node['type']['qualType']
            ret = expand(signature.split('(', 1)[0].strip())
            originals = [x['type']['qualType'] for x in node.get('inner', []) if x.get('kind') == 'ParmVarDecl']
            va_params = [i for i,t in enumerate(originals) if is_va_list(t)]
            params = ['int **' if i in va_params else expand(t) for i,t in enumerate(originals)]
            row = {'return': ret, 'params': params, 'variadic': bool(node.get('variadic'))}
            if va_params:
                row['va_list_params'] = va_params
            require(node['name'] not in result or result[node['name']] == row, 'inconsistent public C declarations')
            result[node['name']] = row
    return result


def checked_api_type(spelling):
    scalar = r'(?:void|_Bool|char|short|int|long|float|double|signed|unsigned|const|volatile|restrict)'
    for match in re.finditer(r'\b(?:struct|union)\s+\w+', spelling):
        require(spelling[match.end():].lstrip().startswith('*'), 'public C aggregate ABI requires an adapter')
    residue = re.sub(r'\b(?:struct|union)\s+\w+|\b' + scalar + r'\b|[\s*(),]', '', spelling)
    require(not residue, f'unsupported public C type: {spelling}')
    # Pointer qualifiers are retained in emitted C signatures; LLVM lowers
    # qualified pointers to the same ptr ABI. Scalar qualification stays gated.
    require('volatile' not in spelling or '*' in spelling,
            'volatile scalar public ABI types unsupported')


def forward_decls(text):
    return [name + ';' for name in sorted(set(re.findall(r'\b(?:struct|union)\s+[A-Za-z_]\w*', text)))]


def without_ir_names(text):
    return re.sub(r'[%@](?:"(?:[^"\\]|\\.)*"|[-\w.$]+)', '', text)


class Backend(Storage):
    check = staticmethod(require)
    split_field = staticmethod(first_field)
    hash = staticmethod(digest)
    forwards = staticmethod(forward_decls)
    def __init__(self, text, unit_key, api=None, exceptions=False):
        require(llvmlite.__version__ == '0.50.0' and llvm.llvm_version_info == (22, 1, 0),
                'requires llvmlite 0.50.0 / LLVM 22.1.0')
        require(unit_key and not Path(unit_key).is_absolute() and '..' not in Path(unit_key).parts,
                'unit-key must be a stable relative identity without ..')
        self.context = llvm.create_context()
        # Clang's hosted Wasm main(void) adds an unused, hidden startup alias.
        # Our executable startup calls main directly. Do not generalize this to
        # user aliases or discard a referenced alias: their address identity is
        # observable and needs a real destination alias ABI.
        entry_alias = re.search(r'^@__main_void = hidden alias i32 \(\), ptr @main\s*$', text, re.M)
        self.omitted_entry_aliases = []
        if entry_alias:
            require(len(re.findall(r'@__main_void\b', text)) == 1,
                    'referenced Wasm startup alias requires an adapter')
            text = text[:entry_alias.start()] + text[entry_alias.end():]
            self.omitted_entry_aliases = ['__main_void']
        if exceptions:
            # O0 Clang can emit either call or invoke for an identical inline
            # body depending on when it discovers the callee is nounwind.
            # Canonicalize with LLVM's own CFG pass before content addressing;
            # duplicate definitions still must match exactly afterwards.
            def enable_cfg(match):
                return re.sub(r'"(?:[^"\\]|\\.)*"|\boptnone\b',
                              lambda token: '' if token[0] == 'optnone' else token[0], match[0])
            text = re.sub(r'^attributes #\d+ = \{[^\n]*\}$', enable_cfg, text, flags=re.M)
        self.module = llvm.parse_assembly(text, context=self.context)
        self.module.verify()
        require(self.module.triple.startswith('wasm32-'), 'only wasm32 targets are supported')
        if exceptions:
            llvm.initialize_all_targets()
            llvm.initialize_all_asmprinters()
            with llvm.Target.from_triple(self.module.triple).create_target_machine() as target:
                with llvm.create_pipeline_tuning_options(speed_level=0) as tuning:
                    with llvm.create_pass_builder(target, tuning) as builder:
                        with llvm.create_new_module_pass_manager() as passes:
                            passes.add_simplify_cfg_pass()
                            passes.run(self.module, builder)
            self.module.verify()
        self.unit_key = unit_key
        self.api = api or {}
        self.va8 = False
        self.wasi_libc = False
        self.storage_cache = {}
        self.checked_globals = set()
        self.layout_ids = {}
        self.aggregate_types = {}
        self.used_aggregates = set()
        require(self.module.triple.startswith('wasm32-'), 'only wasm32 targets are supported')
        self.layout = llvm.create_target_data(self.module.data_layout)
        self.globals = {g.name: g for g in self.module.global_variables}
        self.constructors = {}
        ctor = self.globals.pop('llvm.global_ctors', None)
        if ctor is not None:
            ctor_text = str(ctor)
            rows = re.findall(r'\{ i32 (\d+), ptr @([^,]+), ptr null \}', ctor_text)
            require(len(rows) == ctor.global_value_type.element_count, 'unsupported constructor association or encoding')
            for priority,name in rows:
                name=name.strip('"')
                require(name not in self.constructors, 'duplicate constructor registration')
                require(int(priority)<=65535, 'constructor priority exceeds destination range')
                self.constructors[name]=int(priority)
        require(not any(n.startswith('llvm.') for n in self.globals),
                'LLVM special globals (constructors, destructors, used lists) are unsupported')
        self.types_text = '\n'.join(str(t) for t in self.module.struct_types if not t.is_literal_struct)
        self.functions = {f.name: f for f in self.module.functions}
        if self.omitted_entry_aliases:
            require('main' in self.functions and not self.functions['main'].is_declaration and str(self.functions['main'].global_value_type)=='i32 ()',
                    'Wasm main(void) alias target has an unsupported signature')
            self.api.setdefault('main', {'return':'int','params':[],'variadic':False})
        for name in self.constructors:
            require(name in self.functions and not self.functions[name].is_declaration,
                    'external constructor requires an initialization adapter')
            ft=self.functions[name].global_value_type
            require(str(ft.get_function_return())=='void' and not list(ft.get_function_parameters()) and not ft.is_function_vararg,
                    'constructor must have void(void) signature')
        # Recover binding-missing type properties in ONE auxiliary parse. Parsing
        # the module's type definitions separately for each alloca/GEP type would
        # give quadratic work on large sets of unrelated record types.
        type_requests = {'ptr', 'i8', 'i16', 'i32', 'i64', 'float', 'double'}
        type_requests.update(self.type_spelling(g.global_value_type) for g in self.globals.values())
        for match in re.finditer(r'\bgetelementptr\s+(?:(?:inbounds|nuw|nusw|inrange\(-?\d+,\s*-?\d+\))\s+)*(?:\(\s*)?', text):
            type_requests.add(first_field(text, match.end(), head_only=True))
        for f in self.functions.values():
            for arg in f.arguments:
                type_text = byval_type(b' '.join(arg.attributes).decode())
                if type_text:
                    type_requests.add(type_text)
            for b in f.blocks:
                for i in b.instructions:
                    if i.opcode == 'alloca':
                        type_requests.add(first_field(str(i).split('alloca ', 1)[1])[0])
                    elif i.opcode == 'getelementptr':
                        rest = re.sub(r'^.*?getelementptr\s+(?:(?:inbounds|nuw|nusw|inrange\(-?\d+,\s*-?\d+\))\s+)*', '', str(i).strip())
                        type_requests.add(first_field(rest)[0])
        type_requests = sorted(type_requests)
        self.type_context = llvm.create_context()
        self.type_module = llvm.parse_assembly(self.types_text + '\n' + '\n'.join(
            f'@__type_{n} = external global {t}' for n, t in enumerate(type_requests)), context=self.type_context)
        self.type_cache = {t: g.global_value_type for t, g in zip(type_requests, self.type_module.global_variables)}
        self.struct_elements = {}
        self.symbols = dict(self.functions, **self.globals)
        self.names = {}
        self.names_used = {}
        self.byte_constants = {}
        self.used_runtime = set()
        self.profile = {'format': PROFILE, 'llvm': list(llvm.llvm_version_info),
                        'llvmlite': llvmlite.__version__, 'triple': self.module.triple,
                        'dataLayout': self.module.data_layout, 'exceptions': False,
                        'runtimeDependencies': 2, 'unnamedByteConstants': 1, 'globalArrayExtents': 1}
        require(self.layout.get_abi_size(self.parse_type('ptr')) == 4, 'requires 32-bit pointers')
        require(self.module.data_layout.startswith('e-'), 'requires little-endian layout')
        # Alias/ifunc/module-asm and named metadata cannot hide unhandled definitions.
        for line in str(self.module).splitlines():
            require(not line.startswith('module asm'), 'module assembly is unsupported')
            if line.startswith('@'):
                # Keywords inside symbol names or literal bytes are not IR syntax.
                syntax = re.sub(r'"(?:[^"\\]|\\.)*"', '""', without_ir_names(line))
                require(not re.search(r'\b(alias|ifunc)\b', syntax), 'aliases/ifuncs are unsupported')
            if line.startswith('$'):
                require(line.endswith('comdat any'), 'only COMDAT any is supported')
        for name, value in self.symbols.items():
            require(value.linkage.name in LOCAL | SHARED | {'external'} | ({'weak_any'} if exceptions else set()),
                    f'{name}: unsupported linkage {value.linkage.name}')
            require(value.storage_class.name == 'default', f'{name}: unsupported DLL storage class')
            if name.startswith('llvm.'):
                continue
            emitted = self.symbol_name(value)
            require(emitted not in self.names_used or self.names_used[emitted] == name or
                    (name in self.byte_constants and self.names_used[emitted] in self.byte_constants),
                    f'symbol encoding collision: {name}')
            self.names_used[emitted] = name
            self.names[name] = emitted

    def symbol_name(self, value):
        if value.is_global and not value.is_declaration and value.linkage.name == 'private':
            # Only relocation-free byte constants with globally insignificant
            # addresses may be coalesced. Named-address, mutable, local-only
            # unnamed_addr and pointer-bearing storage retain unit identity.
            payload = str(value).split(' = ', 1)[1]
            match = re.fullmatch(r'private unnamed_addr constant (\[\d+ x i8\]) (c"(?:[^"\\]|\\[0-9A-Fa-f]{2})*"|zeroinitializer)(?:, align (\d+))?', payload)
            if match:
                alignment = int(match[3]) if match[3] else self.layout.get_abi_alignment(value.global_value_type)
                identity = match[1] + '\0' + match[2] + '\0' + str(alignment)
                self.byte_constants[value.name] = identity
                return '__llc_literal_' + digest('unnamed-byte-v1\0' + identity)
        if value.linkage.name in LOCAL:
            return '__llc_local_' + digest(self.unit_key + '\0' + value.name)
        require(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', value.name) is not None,
                f'symbol cannot be represented in C yet: {value.name!r}')
        require(not value.name.startswith('__llc_'), 'reserved generated symbol prefix __llc_')
        return value.name

    def parse_type(self, text):
        require(text in self.type_cache, f'unregistered LLVM storage type: {text}')
        return self.type_cache[text]

    def ctype(self, t):
        k = t.type_kind.name
        if k == 'void':
            return 'void'
        if k == 'integer':
            require(t.type_width in SCALARS, f'unsupported integer width {t.type_width}')
            return SCALARS[t.type_width]
        if k == 'pointer':
            require(str(t) == 'ptr', 'nonzero pointer address space is unsupported')
            return 'void *'
        if k in {'float', 'double'}:
            return k
        if k == 'struct' and t.is_literal_struct and not t.is_packed_struct:
            elements = list(t.elements)
            require(all(e.type_kind.name in {'integer','pointer','float','double'} for e in elements),
                    'only scalar-field aggregate SSA values are supported')
            tag = '__llc_value_'+self.layout_identity(t)[:24]
            fields,cursor = [],0
            for i,e in enumerate(elements):
                offset = self.layout.get_element_offset(t,i)
                if offset>cursor:
                    fields.append(f'unsigned char pad{i}[{offset-cursor}];')
                fields.append(self.ctype(e)+f' f{i};')
                size = (max(1,(e.type_width+7)//8) if e.type_kind.name=='integer' else
                        8 if e.type_kind.name=='double' else 4)
                cursor = offset+size
            total = self.layout.get_abi_size(t)
            if total>cursor:
                fields.append(f'unsigned char tail[{total-cursor}];')
            self.aggregate_types[tag] = ('struct '+tag+' { '+ ' '.join(fields)+' } '
                +f'__attribute__((packed,aligned({self.layout.get_abi_alignment(t)})));')
            self.used_aggregates.add(tag)
            return 'struct '+tag
        raise Unsupported(f'unsupported SSA/ABI type: {t}')

    def signed(self, t):
        require(t.type_kind.name == 'integer' and t.type_width in SIGNED, 'expected supported integer')
        return SIGNED[t.type_width]

    def signed_operand(self, value):
        expr = self.operand(value)
        return f'-(int)({expr})' if str(value.type) == 'i1' else f'({self.signed(value.type)})({expr})'

    def prototype(self, f, name=None):
        ft = f.global_value_type
        if f.name in self.api:
            a = self.api[f.name]
            for ty in [a['return'], *a['params']]:
                checked_api_type(ty)
            require(a.get('variadic', False) == ft.is_function_vararg, f'{f.name}: inconsistent variadic ABI')
            params = list(ft.get_function_parameters())
            require(len(a['params']) == len(params),
                    f'{f.name}: public C ABI was lowered; adapter required')
            def lowered(ty):
                if '*' in ty:
                    return 'ptr'
                ty = re.sub(r'\b(const|restrict|signed|unsigned)\b', '', ty)
                ty = ' '.join(ty.split()) or 'int'
                return {'void': 'void', '_Bool': 'i1', 'char': 'i8', 'short': 'i16',
                        'short int': 'i16', 'int': 'i32', 'long': 'i32', 'long int': 'i32',
                        'long long': 'i64', 'long long int': 'i64', 'float': 'float', 'double': 'double'}.get(ty)
            require(lowered(a['return']) == str(ft.get_function_return()) and
                    all(lowered(t) == str(p) for t, p in zip(a['params'], params)),
                    f'{f.name}: public C header disagrees with the lowered ABI; adapter required')
            params_text = ', '.join(a['params'] + (['...'] if ft.is_function_vararg else [])) or 'void'
            return declarator(a['return'], (name or self.names[f.name]) + '(' + params_text + ')')
        require(f.linkage.name in LOCAL or f.name.startswith('_Z') or f.visibility.name == 'hidden',
                f'{f.name}: provide --api-header for the public C signature')
        args = [self.ctype(t) for t in ft.get_function_parameters()]
        if ft.is_function_vararg:
            args.append('...')
        return self.ctype(ft.get_function_return()) + ' ' + (name or self.names[f.name]) + '(' + (', '.join(args) or 'void') + ')'

    def operand(self, v):
        if v in self.locals:
            return self.locals[v]
        kind = v.value_kind.name
        if kind in {'function', 'global_variable'}:
            require(not self.api.get(v.name, {}).get('va_list_params'),
                    'address of public va_list function requires a callback ABI adapter')
            require(v.name in self.names, f'unsupported symbol operand {v.name}')
            self.dependencies.add(v.name)
            return '(void *)&' + self.names[v.name]
        if kind == 'constant_int':
            return str(v.get_constant_value()) + ('ULL' if v.type.type_width == 64 else 'U')
        if kind == 'constant_pointer_null':
            return '(void *)0'
        if kind == 'constant_fp':
            n = v.get_constant_value()
            if not math.isfinite(n):
                spelling = str(v).split()[-1]
                require(re.fullmatch(r'0x[0-9A-Fa-f]{16}',spelling), 'unsupported nonfinite constant encoding')
                bits = int(spelling[2:],16)
                single = v.type.type_kind.name == 'float'
                if single:
                    bits = ((bits >> 32) & 0x80000000) | 0x7f800000 | ((bits & ((1<<52)-1)) >> 29)
                ty,integer = ('float','unsigned int') if single else ('double','unsigned long long')
                return f'((union {{ {integer} bits; {ty} value; }}){{ .bits = {bits}ULL }}).value'
            return n.hex() + ('f' if v.type.type_kind.name == 'float' else '')
        if kind == 'constant_expr' and v.type.is_pointer:
            expr, deps = self.constant_pointer(str(v).removeprefix('ptr '))
            self.dependencies.update(deps)
            return expr
        if kind == 'constant_expr' and str(v.type) == 'i32':
            match = re.fullmatch(r'i32 ptrtoint \(ptr (.+) to i32\)', str(v))
            require(match is not None, 'unsupported integer constant expression')
            expr, deps = self.constant_pointer(match[1])
            self.dependencies.update(deps)
            return f'(unsigned int)({expr})'
        if kind in {'constant_struct', 'constant_aggregate_zero'}:
            ty = self.ctype(v.type)
            value = self.untype(str(v),v.type)
            if value == 'zeroinitializer':
                return '('+ty+'){0}'
            fields = list(self.fields(value[1:-1]))
            elems = list(v.type.elements)
            require(len(fields) == len(elems),'aggregate constant field count')
            values = [self.scalar_constant(t,self.untype(s,t)) for t,s in zip(elems,fields)]
            return '('+ty+'){'+', '.join(f'.f{i} = {v}' for i,v in enumerate(values))+'}'
        raise Unsupported(f'unsupported operand {kind}: {v}')

    def scalar_constant(self,t,value):
        if t.is_pointer:
            expr,deps = self.constant_pointer(value)
            self.dependencies.update(deps)
            return expr
        if t.type_kind.name == 'integer':
            if value.startswith('ptrtoint '):
                match = re.fullmatch(r'ptrtoint \(ptr (.+) to i32\)',value)
                require(t.type_width == 32 and match is not None,'unsupported constant pointer-to-integer cast')
                expr,deps = self.constant_pointer(match[1])
                self.dependencies.update(deps)
                return '(unsigned int)('+expr+')'
            require(re.fullmatch(r'-?\d+|true|false',value) is not None,'unsupported scalar constant')
            n = int({'true':'1','false':'0'}.get(value,value)) % (1<<t.type_width)
            return str(n)+('ULL' if t.type_width==64 else 'U')
        raise Unsupported('unsupported aggregate scalar constant '+value)

    def edge(self, source, target):
        assignments = [(phi, self.operand(value)) for phi, value in self.phi_inputs.get((source, target), [])]
        # Snapshot ALL inputs before writing any phi result (including cycles).
        return [f'{self.locals[p]}_edge = {v};' for p, v in assignments] + [
            f'{self.locals[p]} = {self.locals[p]}_edge;' for p, _ in assignments] + [
            f'goto {self.labels[target]};']

    def instruction(self, i, block):
        op = i.opcode
        vals = list(i.operands)
        text = str(i).strip()
        out = self.locals.get(i)
        val = self.operand
        if op == 'phi':
            return []
        if op == 'insertvalue':
            match = re.search(r',\s*(\d+)$', text)
            require(match is not None and i.type.is_literal_struct, 'unsupported insertvalue')
            index = int(match[1])
            elements = list(i.type.elements)
            require(index < len(elements) and elements[index] == vals[1].type, 'invalid insertvalue index/type')
            lines = ([f'{out} = ({self.ctype(i.type)}){{0}};'] if vals[0].value_kind.name in {'poison_value', 'undef_value'}
                     else [f'{out} = {val(vals[0])};'])
            return lines + [f'{out}.f{index} = {val(vals[1])};']
        if op == 'extractvalue':
            match = re.search(r'((?:,\s*\d+)+)$',text)
            require(match is not None,'missing extractvalue indices')
            indices = [int(n) for n in re.findall(r'\d+',match[1])]
            t,expr = vals[0].type,val(vals[0])
            for index in indices:
                require(t.is_literal_struct and 0 <= index < len(list(t.elements)), 'unsupported extractvalue index')
                expr += '.f'+str(index)
                t = list(t.elements)[index]
            return [f'{out} = {expr};']
        if op == 'alloca':
            require(block == self.blocks[0], 'non-entry alloca is unsupported')
            type_text, _ = first_field(text.split('alloca ', 1)[1])
            t = self.parse_type(type_text)
            require(vals[0].value_kind.name == 'constant_int', 'dynamic alloca is unsupported')
            count = vals[0].get_constant_value()
            size = self.layout.get_abi_size(t) * count
            require(0 < size <= 1 << 24, 'unsupported alloca size')
            align = re.search(r'\balign (\d+)', text)
            align = int(align[1]) if align else self.layout.get_abi_alignment(t)
            self.declarations.append(f'unsigned char {out}_storage[{size}] __attribute__((aligned({align})));')
            return [f'{out} = {out}_storage;']
        atomic = op in {'atomicrmw','cmpxchg','fence'} or (op in {'load','store'} and re.search(r'\batomic\b',without_ir_names(text)))
        if atomic:
            require('volatile' not in without_ir_names(text),'volatile atomic operations unsupported')
            require('syncscope' not in without_ir_names(text), 'explicit atomic syncscope unsupported')
            # WebAssembly atomics are seq_cst, a permitted strengthening of LLVM ordering.
            # Encodings: WebAssembly/threads proposals/threads/Overview.md.
            if op=='fence':
                return ['__builtin(atomic_fence);']
            t = i.type if op=='load' else vals[0].type if op=='store' else vals[1].type
            width = 32 if t.is_pointer else t.type_width if t.type_kind.name=='integer' else 0
            require(width in {8,16,32,64},'unsupported atomic scalar type')
            align = {8:0,16:1,32:2,64:3}[width]
            alignment = re.search(r'\balign (\d+)', text)
            require(alignment is not None and int(alignment[1]) >= width//8,
                    'atomic access must guarantee natural alignment')
            def raw(code,arguments,ret):
                return f'__wasm({ret}, ('+', '.join(arguments)+f'), op 254 {code} {align} 0)'
            ret = 'unsigned long long' if width==64 else 'unsigned int'
            if op=='load':
                code={8:0x12,16:0x13,32:0x10,64:0x11}[width]
                return [f'{out} = ({self.ctype(t)})'+raw(code,[val(vals[0])],ret)+';']
            if op=='store':
                code={8:0x19,16:0x1a,32:0x17,64:0x18}[width]
                return [raw(code,[val(vals[1]),f'({ret})({val(vals[0])})'],'void')+';']
            require(width==32,'only 32-bit atomic RMW/cmpxchg supported')
            if op=='cmpxchg':
                expr=raw(0x48,[val(vals[0]),f'(unsigned int)({val(vals[1])})',f'(unsigned int)({val(vals[2])})'],ret)
                return [f'{out}.f0 = ({self.ctype(t)}){expr};',f'{out}.f1 = {out}.f0 == {val(vals[1])};']
            operation=re.search(r'atomicrmw\s+(\w+)',text)[1]
            codes={'add':0x1e,'sub':0x25,'and':0x2c,'or':0x33,'xor':0x3a,'xchg':0x41}
            require(operation in codes,'unsupported atomic RMW operation '+operation)
            return [f'{out} = ({self.ctype(t)})'+raw(codes[operation],[val(vals[0]),f'(unsigned int)({val(vals[1])})'],ret)+';']
        if op in {'load', 'store'}:
            require(not re.search(r'\batomic\b', without_ir_names(text)), 'unsupported atomic memory operation')
            if re.search(r'\bvolatile\b', without_ir_names(text)):
                t=i.type if op=='load' else vals[0].type
                require(str(t) in {f'i{width}' for width in SCALARS}|{'float','double','ptr'},
                        'volatile aggregate memory operations unsupported')
                alignment=re.search(r'\balign (\d+)',text)
                require(not alignment or int(alignment[1])>=self.layout.get_abi_alignment(t),
                        'unaligned volatile memory operations unsupported')
                ty='unsigned char' if str(t)=='i1' else self.ctype(t)
                pointer=val(vals[0] if op=='load' else vals[1])
                access=f'*({ty} volatile *)({pointer})'
                if op=='load':return [f'{out} = ({access})'+(' & 1U' if str(t)=='i1' else '')+';']
                return [f'{access} = {val(vals[0])};']
            self.runtime_dependencies.add('memcpy')
            if op == 'load':
                if str(i.type) == 'i1':
                    return [f'{{ unsigned char __llc_byte; memcpy(&__llc_byte, {val(vals[0])}, 1); {out} = __llc_byte & 1U; }}']
                lines = [f'memcpy(&{out}, {val(vals[0])}, sizeof({out}));']
                if i.type.is_struct:
                    for index,t in enumerate(i.type.elements):
                        if str(t)=='i1':
                            offset = self.layout.get_element_offset(i.type,index)
                            lines.append(f'{{ unsigned char __llc_byte; memcpy(&__llc_byte, (unsigned char *)&{out}+{offset}, 1); {out}.f{index} = __llc_byte & 1U; }}')
                return lines
            ty = self.ctype(vals[0].type)
            return [f'{{ {ty} __llc_store = {val(vals[0])}; memcpy({val(vals[1])}, &__llc_store, sizeof(__llc_store)); }}']
        if op == 'getelementptr':
            rest = re.sub(r'^.*?getelementptr\s+(?:(?:inbounds|nuw|nusw|inrange\(-?\d+,\s*-?\d+\))\s+)*', '', text)
            type_text, _ = first_field(rest)
            t = self.parse_type(type_text)
            terms = []
            for pos, idx in enumerate(vals[1:]):
                if pos == 0:
                    stride = self.layout.get_abi_size(t)
                elif t.is_struct:
                    require(idx.value_kind.name == 'constant_int', 'nonconstant struct GEP index')
                    n = idx.get_constant_value()
                    terms.append(str(self.layout.get_element_offset(t, n)) + 'U')
                    if t not in self.struct_elements:
                        self.struct_elements[t] = list(t.elements)
                    t = self.struct_elements[t][n]
                    continue
                elif t.is_array:
                    t = next(t.elements)
                    stride = self.layout.get_abi_size(t)
                else:
                    raise Unsupported(f'unsupported GEP indexing type: {t}')
                # wasm32 pointer arithmetic wraps in 32 bits. Avoid C signed overflow.
                index_expr = self.signed_operand(idx) if idx.type.type_width < 32 else val(idx)
                terms.append(f'((unsigned int)({index_expr}) * {stride}U)')
            return [f'{out} = (void *)((unsigned int){val(vals[0])} + ' + ' + '.join(terms) + ');']
        binaries = {'add': '+', 'sub': '-', 'mul': '*', 'udiv': '/', 'urem': '%',
                    'and': '&', 'or': '|', 'xor': '^', 'shl': '<<', 'lshr': '>>',
                    'fadd': '+', 'fsub': '-', 'fmul': '*', 'fdiv': '/'}
        if op in binaries:
            require(not re.search(r'\b(fast|nnan|ninf|nsz|arcp|contract|afn|reassoc)\b', text),
                    'fast-math flags unsupported')
            a, b = map(val, vals)
            if i.type.type_kind.name == 'integer':
                # Prevent integral promotions from making i8/i16 arithmetic signed.
                wide = 'unsigned long long' if i.type.type_width == 64 else 'unsigned int'
                a, b = f'({wide})({a})', f'({wide})({b})'
            expr = f'({a} {binaries[op]} {b})'
            if str(i.type) == 'i1':
                expr = f'({expr} & 1U)'
            return [f'{out} = {expr};']
        if op in {'sdiv', 'srem', 'ashr'}:
            sym = {'sdiv': '/', 'srem': '%', 'ashr': '>>'}[op]
            lhs, rhs = self.signed_operand(vals[0]), self.signed_operand(vals[1])
            expr = f'{lhs} {sym} {rhs}'
            if op == 'srem':
                expr = f'({rhs} == -1 ? 0 : ({expr}))'
            return [f'{out} = {expr};']
        if op == 'icmp':
            pred = re.search(r'\bicmp (\w+)', text)[1]
            cmp = {'eq': '==', 'ne': '!=', 'ugt': '>', 'uge': '>=', 'ult': '<', 'ule': '<=',
                   'sgt': '>', 'sge': '>=', 'slt': '<', 'sle': '<='}
            require(pred in cmp, f'unsupported predicate {pred}')
            if pred.startswith('s'):
                return [f'{out} = {self.signed_operand(vals[0])} {cmp[pred]} {self.signed_operand(vals[1])};']
            ty = self.signed(vals[0].type) if pred.startswith('s') else (
                'unsigned int' if vals[0].type.is_pointer else self.ctype(vals[0].type))
            return [f'{out} = ({ty})({val(vals[0])}) {cmp[pred]} ({ty})({val(vals[1])});']
        if op == 'fcmp':
            match = re.search(r'\bfcmp (\w+)',text)
            pred = match[1]
            a,b = map(val,vals)
            nan = f'({a} != {a} || {b} != {b})'
            if pred in {'false','true','ord','uno'}:
                expr = {'false':'0','true':'1','ord':'!'+nan,'uno':nan}[pred]
            else:
                cmp = {'eq':'==','ne':'!=','gt':'>','ge':'>=','lt':'<','le':'<='}
                require(pred[:1] in {'o','u'} and pred[1:] in cmp, 'unsupported floating comparison/flags')
                expr = f'({"!"+nan if pred[0]=="o" else nan} {"&&" if pred[0]=="o" else "||"} ({a} {cmp[pred[1:]]} {b}))'
            return [f'{out} = {expr};']
        if op in {'zext', 'trunc', 'ptrtoint', 'inttoptr', 'fptrunc', 'fpext', 'uitofp', 'fptoui'}:
            expr = val(vals[0])
            if str(i.type) == 'i1':
                expr = f'({expr} & 1U)'
            return [f'{out} = ({self.ctype(i.type)})({expr});']
        if op in {'sext', 'sitofp'}:
            expr = val(vals[0])
            expr = f'-(int)({expr})' if str(vals[0].type) == 'i1' else f'({self.signed(vals[0].type)})({expr})'
            return [f'{out} = ({self.ctype(i.type)})({expr});']
        if op == 'fptosi':
            return [f'{out} = ({self.signed(i.type)})({val(vals[0])});']
        if op == 'bitcast':
            require(self.layout.get_abi_size(i.type) == self.layout.get_abi_size(vals[0].type), 'bitcast size mismatch')
            self.runtime_dependencies.add('memcpy')
            return [f'{{ {self.ctype(vals[0].type)} __llc_bits = {val(vals[0])}; memcpy(&{out}, &__llc_bits, sizeof({out})); }}']
        if op == 'select':
            return [f'{out} = {val(vals[0])} ? {val(vals[1])} : {val(vals[2])};']
        if op == 'landingpad':
            require(self.profile['exceptions'], 'landingpad requires exception profile')
            require(str(i.type) == '{ ptr, i32 }', 'unsupported landingpad ABI')
            require('filter ' not in text, 'exception filters are unsupported')
            self.runtime_dependencies.add('__wp_cxx_match')
            lines = [f'{out}.f0 = __llc_exception;', f'{out}.f1 = 0;']
            for clause in vals:
                require(str(clause.type) == 'ptr', 'unsupported catch clause')
                pointer = val(clause)
                selector = '0xffffffffU' if clause.value_kind.name == 'constant_pointer_null' else f'(unsigned int)({pointer})'
                lines += [f'if (!{out}.f1 && __wp_cxx_match(__llc_exception, {pointer})) {out}.f1 = {selector};']
            return lines
        if op == 'resume':
            require(self.profile['exceptions'], 'resume requires exception profile')
            return [f'__throw __wp_cxx_exception((unsigned int)({val(vals[0])}.f0));', '__builtin_trap();']
        if op in {'call', 'invoke'}:
            require(not re.search(r'\b(musttail|fastcc|coldcc|cc \d+)\b', without_ir_names(text)), 'unsupported call convention')
            callee, args = vals[-1], vals[:-3] if op == 'invoke' else vals[:-1]
            if op == 'invoke':
                require(self.profile['exceptions'], 'invoke requires exception profile')
            name = callee.name
            require(name != '__gxx_personality_v0', 'direct personality calls are unsupported')
            require(op != 'invoke' or not name.startswith('llvm.'), 'invoked intrinsics are unsupported')
            if name == 'llvm.assume':
                require(str(i.type)=='void' and args and str(args[0].type)=='i1', 'invalid assume intrinsic')
                if ' [ ' in text:
                    bundle=text[text.index(' [ ')+1:]
                    require(re.fullmatch(r'\[ \"align\"\([^)]*\) \](?:, ![^\n]+)?',bundle) is not None and
                            len(args) in {3,4} and str(args[1].type)=='ptr' and
                            all(str(a.type)=='i32' for a in args[2:]), 'unsupported assume operand bundle')
                else:
                    require(len(args)==1, 'invalid assume operand count')
                # An optimization promise has no runtime effect. Generated C
                # does not exploit its truth or alignment information.
                return []
            require(' [ ' not in text, 'operand bundles unsupported')
            if callee.value_kind.name == 'inline_asm':
                require(not args and str(i.type)=='void' and str(callee)=='ptr asm sideeffect "", "~{memory}"',
                        'unsupported inline assembly')
                # A real fence is stronger than Clang's compiler-only barrier;
                # do not silently erase its memory-ordering side effect.
                return ['__builtin(atomic_fence);']
            if name in {'llvm.eh.typeid.for', 'llvm.eh.typeid.for.p0'}:
                require(self.profile['exceptions'] and len(args) == 1, 'typeid requires exception profile')
                selector = '0xffffffffU' if args[0].value_kind.name == 'constant_pointer_null' else f'(unsigned int)({val(args[0])})'
                return [f'{out} = {selector};']
            if name.startswith('llvm.'):
                math_call = re.fullmatch(r'llvm\.(atan2|pow|rint)\.f(32|64)', name)
                if math_call:
                    operation, width = math_call.groups()
                    ty = 'float' if width == '32' else 'double'
                    require(str(i.type) == ty and len(args) == (1 if operation == 'rint' else 2)
                            and all(str(a.type) == ty for a in args), 'invalid scalar math intrinsic')
                    helper = operation + ('f' if width == '32' else '')
                    self.runtime_dependencies.add(helper)
                    return [f'{out} = {helper}(' + ', '.join(val(a) for a in args) + ');']
                if name in {'llvm.fabs.f32','llvm.fabs.f64'}:
                    require(len(args)==1 and args[0].type==i.type,'invalid fabs intrinsic')
                    wide=str(i.type)=='double'
                    integer='unsigned long long' if wide else 'unsigned int'
                    mask='0x7fffffffffffffffULL' if wide else '0x7fffffffU'
                    return [f'{{ union {{ {self.ctype(i.type)} f; {integer} u; }} __llc_abs; __llc_abs.f={val(args[0])}; __llc_abs.u &= {mask}; {out}=__llc_abs.f; }}']
                integer_abs = re.fullmatch(r'llvm\.abs\.i(8|16|32|64)', name)
                if integer_abs:
                    width = int(integer_abs[1])
                    require(len(args) == 2 and str(i.type) == f'i{width}'
                            and args[0].type == i.type and str(args[1].type) == 'i1'
                            and args[1].value_kind.name == 'constant_int',
                            'invalid scalar integer abs intrinsic')
                    # Unsigned subtraction avoids signed-minimum overflow. When
                    # the immarg is false LLVM requires the minimum bit pattern;
                    # when true that input is poison, so the same result is valid.
                    zero = '0ULL' if width == 64 else '0U'
                    value = val(args[0])
                    return [f'{out} = ({self.signed_operand(args[0])} < 0) ? {zero} - ({value}) : ({value});']
                if name.startswith('llvm.is.constant.'):
                    require(len(args)==1 and str(i.type)=='i1', 'invalid is.constant intrinsic')
                    known=args[0].value_kind.name in {'constant_int','constant_fp','constant_pointer_null','constant_aggregate_zero'}
                    return [f'{out} = {int(known)};']
                if name == 'llvm.experimental.noalias.scope.decl':
                    require(str(i.type)=='void' and len(args)==1 and str(args[0].type)=='metadata',
                            'invalid noalias scope declaration')
                    # Optimization-only alias scope; emitted C makes no corresponding
                    # aliasing assumption and therefore needs no runtime operation.
                    return []
                if name.startswith('llvm.expect.'):
                    require(len(args) in {2,3} and args[0].type==i.type, 'invalid expect intrinsic')
                    return [f'{out} = {val(args[0])};']
                if name.startswith(('llvm.va_start.', 'llvm.va_end.', 'llvm.va_copy.')):
                    require(self.va8, 'va_list requires the wasm-posix eight-byte-slot frontend ABI')
                    require(str(i.type)=='void' and all(str(a.type)=='ptr' for a in args), 'invalid va intrinsic ABI')
                    if name.startswith('llvm.va_end.'):
                        require(len(args)==1,'invalid va_end')
                        return []
                    self.runtime_dependencies.add('memcpy')
                    if name.startswith('llvm.va_copy.'):
                        require(len(args)==2,'invalid va_copy')
                        return [f'memcpy({val(args[0])}, {val(args[1])}, 4);']
                    fixed=list(self.current_function.arguments)
                    require(len(args)==1 and fixed and self.current_function.global_value_type.is_function_vararg,
                            'va_start requires a variadic function with a fixed argument')
                    return [f'{{ int *__llc_cursor; __builtin_va_start(__llc_cursor, a{len(fixed)-1}); memcpy({val(args[0])}, &__llc_cursor, 4); }}']
                if name.startswith(('llvm.lifetime.start.', 'llvm.lifetime.end.')):
                    return []
                if name.startswith(('llvm.memcpy.', 'llvm.memmove.', 'llvm.memset.')):
                    require(len(args) == 4 and args[-1].value_kind.name == 'constant_int' and args[-1].get_constant_value() == 0,
                            'volatile memory intrinsic unsupported')
                    helper = name.split('.')[1]
                    self.runtime_dependencies.add(helper)
                    return [helper + '(' + ', '.join(val(a) for a in args[:3]) + ');']
                if name.startswith('llvm.is.fpclass.'):
                    require(len(args)==2 and args[1].value_kind.name=='constant_int','nonconstant floating class mask')
                    mask=args[1].get_constant_value()
                    require(mask in {3,504},'unsupported floating class mask')
                    builtin='isnan' if mask==3 else 'isfinite'
                    return [f'{out} = __builtin_{builtin}({val(args[0])});']
                if name == 'llvm.trap':
                    return ['__builtin_trap();']
                match = re.fullmatch(r'llvm\.(u|s)(add|sub|mul)\.with\.overflow\.i(\d+)',name)
                if match:
                    signed,operation,width = match[1]=='s',match[2],int(match[3])
                    require(width in {8,16,32} and len(args)==2,'unsupported overflow intrinsic width')
                    a,b = map(val,args)
                    wide = 'long long' if signed else 'unsigned long long'
                    if signed:
                        a,b = self.signed_operand(args[0]),self.signed_operand(args[1])
                    expr = f'({wide})({a}) '+{'add':'+','sub':'-','mul':'*'}[operation]+f' ({wide})({b})'
                    overflow = (f'(__llc_wide < {-1<<(width-1)}LL || __llc_wide > {(1<<(width-1))-1}LL)' if signed else
                                (f'({a} < {b})' if operation=='sub' else f'(__llc_wide > {(1<<width)-1}ULL)'))
                    return [f'{{ {wide} __llc_wide = {expr}; {out}.f0 = __llc_wide; {out}.f1 = {overflow}; }}']
                match = re.fullmatch(r'llvm\.(cttz|ctlz|ctpop)\.i(32|64)',name)
                if match:
                    op_name,width = match[1],int(match[2])
                    require(len(args)==(1 if op_name=='ctpop' else 2),'invalid bit-count intrinsic')
                    if len(args)==2:
                        require(args[1].value_kind.name=='constant_int','nonconstant zero-poison flag')
                    builtin = {'cttz':'ctz','ctlz':'clz','ctpop':'popcount'}[op_name]+('ll' if width==64 else '')
                    value = val(args[0])
                    expr = f'__builtin_{builtin}({value})'
                    if op_name!='ctpop':
                        expr = f'({value} ? {expr} : {width})'
                    return [f'{out} = {expr};']
                raise Unsupported(f'unsupported intrinsic {name}')
            if callee.value_kind.name == 'function':
                self.dependencies.add(name)
                f = self.functions[name]
                self.prototype(f)
                expected = list(f.global_value_type.get_function_parameters())
                require(not (f.global_value_type.is_function_vararg and
                             re.search(r'\bbyval\(', text)),
                        'variadic aggregate call requires an ABI adapter')
                require([str(a.type) for a in args[:len(expected)]] == [str(t) for t in expected]
                        and (len(args) == len(expected) or f.global_value_type.is_function_vararg)
                        and str(i.type) == str(f.global_value_type.get_function_return()),
                        f'{name}: call signature differs from its declaration')
                # Calls use the same internal lowered ABI; validate byval in callee.
                expr = self.names[name]
            else:
                require(not re.search(r'\b(inalloca|preallocated|byref)\b', text),
                        'unsupported indirect parameter ABI attribute')
                if re.search(r'\bbyval\(', text):
                    require(self.profile.get('internalIndirectByval') == 'callee-copy-v1',
                            'indirect by-value aggregate ABI calls require --internal-indirect-byval')
                    require('...' not in text, 'variadic aggregate call requires an ABI adapter')
                    # The opt-in profile asserts translated targets. Their prologues
                    # already copy each byval argument, just as for direct calls.
                    # External C aggregate callbacks still need native ABI adapters.
                if '...' in text:
                    require(self.va8, 'indirect variadic calls require the eight-byte-slot frontend ABI')
                    # Opaque pointers lose their pointee signature. LLVM prints
                    # the explicit variadic function type before the callee;
                    # preserve its fixed prefix rather than making every actual
                    # a fixed C argument (which changes the destination ABI).
                    call_text = re.sub(r'^%(?:"(?:[^"\\]|\\.)*"|[-\w.$]+)\s*=\s*', '', text)
                    match = re.match(r'(?:(?:tail|notail)\s+)?call\s+(?:noundef\s+)?(void|i\d+|float|double|ptr)\s+\(([^()]*)\.\.\.\)\s+', call_text)
                    require(match is not None, 'unsupported indirect variadic signature')
                    fixed = [t.strip() for t in match[2].split(',') if t.strip()]
                    require(fixed and all(re.fullmatch(r'i(?:8|16|32|64)|float|double|ptr', t) for t in fixed),
                            'indirect variadic fixed arguments must be supported scalars')
                    require(match[1] == str(i.type) and len(args) >= len(fixed) and
                            [str(a.type) for a in args[:len(fixed)]] == fixed,
                            'indirect variadic call signature mismatch')
                    require(all(str(a.type) in {'i32','i64','double','ptr'} for a in args[len(fixed):]),
                            'indirect variadic arguments require promoted scalars')
                    params = ', '.join(self.ctype(a.type) for a in args[:len(fixed)]) + ', ...'
                else:
                    params = ', '.join(self.ctype(a.type) for a in args) or 'void'
                expr = f'(({self.ctype(i.type)} (*)({params})){val(callee)})'
            actuals = [val(a) for a in args]
            adapters = []
            if callee.value_kind.name == 'function' and name in self.api:
                for n in self.api[name].get('va_list_params', []):
                    require(self.va8, 'public va_list requires the eight-byte-slot frontend ABI')
                    adapters.append(f'int *__llc_ap{n} = (int *)({actuals[n]});')
                    actuals[n] = f'&__llc_ap{n}'
                fixed = len(self.api[name]['params'])
                actuals = [f'({ty})({arg})' for ty, arg in zip(self.api[name]['params'], actuals)] + actuals[fixed:]
            call = expr + '(' + ', '.join(actuals) + ')'
            statement = (f'{out} = ' if out else '') + call + ';'
            call_lines = ['{ ' + ' '.join(adapters) + ' ' + statement + ' }'] if adapters else [statement]
            if op == 'invoke':
                return ['__try {'] + ['  '+line for line in call_lines] + ['} __catch __wp_cxx_exception(__llc_payload) {', '  __llc_exception = (void *)__llc_payload;'] + ['  '+line for line in self.edge(block, vals[-2])] + ['}'] + self.edge(block, vals[-3])
            return call_lines
        if op == 'br':
            if len(vals) == 1:
                return self.edge(block, vals[0])
            return [f'if ({val(vals[0])}) {{'] + ['  ' + x for x in self.edge(block, vals[2])] + [
                '} else {'] + ['  ' + x for x in self.edge(block, vals[1])] + ['}']
        if op == 'switch':
            # LLVM 22's operand iterator exposes the condition and successor
            # blocks, but not case APInts. Read just those missing constants
            # from the verified instruction's canonical printer.
            body = text[text.index('[')+1:text.rindex(']')]
            pattern = r'i(\d+)\s+(-?\d+|true|false),\s+label\s+%(?:"(?:[^"\\]|\\.)*"|[-\w.$]+)'
            cases = re.findall(pattern,body)
            require(not re.sub(pattern,'',body).strip() and len(cases)==len(vals)-2,
                    'unsupported switch case representation')
            width = vals[0].type.type_width
            lines = [f'switch (({self.ctype(vals[0].type)})({val(vals[0])})) {{']
            for (bits,value),target in zip(cases,vals[2:]):
                require(int(bits)==width and target.value_kind.name=='basic_block','inconsistent switch operands')
                n = int({'true':'1','false':'0'}.get(value,value)) % (1<<width)
                literal = str(n)+('ULL' if width==64 else 'U')
                lines += [f'case {literal}:'] + ['  '+x for x in self.edge(block,target)]
            return lines + ['default:'] + ['  ' + x for x in self.edge(block, vals[1])] + ['}']
        if op == 'ret':
            return ['return' + (' ' + val(vals[0]) if vals else '') + ';']
        if op == 'unreachable':
            return ['__builtin_trap();']
        if op == 'fneg':
            return [f'{out} = -{val(vals[0])};']
        raise Unsupported(f'unsupported instruction {op}: {text}')

    def function(self, f):
        # LLVM may print an attribute comment before the actual definition.
        header = next(line for line in str(f).splitlines() if line.startswith('define '))
        if self.profile['exceptions'] and 'personality' in without_ir_names(header):
            require(re.search(r'personality ptr @__gxx_personality_v0(?:\s|$)', header), 'unsupported exception personality')
            header = re.sub(r'personality ptr @__gxx_personality_v0', '', header)
        require(not re.search(r'\b(personality|gc|prefix|prologue|fastcc|coldcc|cc \d+)\b', without_ir_names(header)),
                f'{f.name}: unsupported function convention/exception metadata')
        self.used_aggregates = set()
        self.current_function = f
        self.prototype(f)
        self.locals = {}
        self.dependencies = set()
        self.runtime_dependencies = set()
        self.declarations = ['void *__llc_exception;'] if self.profile['exceptions'] else []
        self.blocks = list(f.blocks)
        self.labels = {b: 'b' + str(n) for n, b in enumerate(self.blocks)}
        self.phis = {b: [i for i in b.instructions if i.opcode == 'phi'] for b in self.blocks}
        self.phi_inputs = {}
        for b, phis in self.phis.items():
            for phi in phis:
                for pred, value in zip(phi.incoming_blocks, phi.operands):
                    self.phi_inputs.setdefault((pred, b), []).append((phi, value))
        args = list(f.arguments)
        public = self.api.get(f.name)
        params = []
        entry = []
        for n, arg in enumerate(args):
            name = 'a' + str(n)
            self.locals[arg] = name
            params.append(declarator(public['params'][n] if public else self.ctype(arg.type), name))
            if public and n in public.get('va_list_params', []):
                require(self.va8, 'public va_list requires the eight-byte-slot frontend ABI')
                entry.append(f'void *{name}_cursor = *{name};')
                self.locals[arg] = name + '_cursor'
            attrs = b' '.join(arg.attributes).decode()
            # Public aggregate adapters are not implemented: fail instead of exporting
            # a lowered pointer signature as if it were the original C API.
            if re.search(r'\b(byval|sret)\(', attrs):
                require(f.name.startswith('_Z') or f.linkage.name in LOCAL or f.visibility.name == 'hidden',
                        f'{f.name}: public C aggregate ABI requires an adapter')
            require(not re.search(r'\b(inalloca|preallocated|nest|swiftself|swifterror|byref)\b', attrs),
                    f'{f.name}: unsupported parameter ABI attribute')
            type_text = byval_type(attrs)
            if type_text:
                t = self.parse_type(type_text)
                size, align = self.layout.get_abi_size(t), self.layout.get_abi_alignment(t)
                # LLVM's byval parameter alignment can exceed the structural
                # DataLayout alignment (for example C++ alignas(256)).
                explicit_align = re.search(r'\balign (\d+)', attrs)
                if explicit_align:
                    align = max(align, int(explicit_align[1]))
                self.declarations.append(f'unsigned char {name}_copy[{size}] __attribute__((aligned({align})));')
                self.runtime_dependencies.add('memcpy')
                entry += [f'memcpy({name}_copy, {name}, {size});', f'{name} = {name}_copy;']
        index = 0
        for b in self.blocks:
            for i in b.instructions:
                if i.type.type_kind.name != 'void':
                    name = 'v' + str(index)
                    index += 1
                    self.locals[i] = name
                    ty = self.ctype(i.type)
                    self.declarations.append(ty + ' ' + name + ';')
                    if i.opcode == 'phi':
                        self.declarations.append(ty + ' ' + name + '_edge;')
        body = []
        for b in self.blocks:
            body.append(self.labels[b] + ':;')
            for i in b.instructions:
                body.extend(self.instruction(i, b))
        startup_boundary = bool(self.profile['exceptions'] and f.name in self.constructors)
        if startup_boundary:
            self.runtime_dependencies.add('__cxa_call_terminate')
        decls = []
        for name in sorted(self.dependencies):
            if name == f.name:
                continue
            if name in self.functions:
                callee = self.functions[name]
                for arg in callee.arguments:
                    attrs = b' '.join(arg.attributes).decode()
                    require(not re.search(r'\b(byval|sret)\(', attrs) or name.startswith('_Z') or
                            callee.linkage.name in LOCAL or callee.visibility.name == 'hidden',
                            f'{name}: public C aggregate ABI requires an adapter')
                decls.append('extern ' + self.prototype(callee) + ';')
            else:
                decls.append(self.global_decl(self.globals[name], external=True))
        ret = public['return'] if public else self.ctype(f.global_value_type.get_function_return())
        for helper in sorted(self.runtime_dependencies - {'memcpy', 'memmove', 'memset'}):
            decls.append('extern ' + RUNTIME_SIGNATURES[helper] + ';')
        if f.global_value_type.is_function_vararg:
            params.append('...')
        constructor = f'__attribute__((constructor({self.constructors[f.name]}))) ' if f.name in self.constructors and not startup_boundary else ''
        lines = ['/* Generated by ' + PROFILE + '. */', '#include <string.h>',
                 *(['__exception __wp_cxx_exception(unsigned int);'] if self.profile['exceptions'] else []),
                 *(self.aggregate_types[n] for n in sorted(self.used_aggregates)),
                 *forward_decls('\n'.join(decls + params + [ret])), *decls,
                 constructor + declarator(ret, self.names[f.name] + '(' + (', '.join(params) or 'void') + ')') + ' {']
        lines += ['  ' + x for x in self.declarations + entry + body] + ['}', '']
        if startup_boundary:
            # Only startup execution terminates here. Preserve the original
            # function's identity and semantics for ordinary direct calls.
            wrapper = '__llc_startup_' + digest(self.names[f.name])[:24]
            lines += [f'__attribute__((constructor({self.constructors[f.name]}))) static void {wrapper}(void) {{',
                      f'  __try {{ {self.names[f.name]}(); }}',
                      '  __catch __wp_cxx_exception(payload) {',
                      '    __cxa_call_terminate((void *)payload);',
                      '    __builtin_trap();', '  }', '}', '']
        require(self.names[f.name] not in self.runtime_dependencies,
                f'{f.name}: lowering would recursively call its own runtime helper')
        self.used_runtime.update(self.runtime_dependencies)
        dependencies = {self.names[n] for n in self.dependencies if n != f.name}
        return '\n'.join(lines), sorted(dependencies | self.runtime_dependencies)

    def emit(self):
        artifacts = []
        files = {}
        values = list(self.globals.values()) + list(self.functions.values())
        for number,value in enumerate(values):
            if number and number%2000==0:
                print(f'Lowering {number}/{len(values)} symbols',file=sys.stderr,flush=True)
            if value.is_declaration or value.name.startswith('llvm.'):
                continue
            try:
                if value.is_function:
                    content, deps = self.function(value)
                    runtime_deps = sorted(self.runtime_dependencies)
                    signature = self.prototype(value)
                else:
                    content, deps = self.global_definition(value)
                    content = '/* Generated by ' + PROFILE + '. */\n' + content
                    runtime_deps = []
                    signature = self.global_decl(value, external=True)
            except Unsupported as error:
                raise Unsupported(f'{value.name}: {error}') from error
            symbol = self.names[value.name]
            artifact_id = digest(PROFILE + '\0' + symbol)
            path = 'definitions/' + artifact_id + '.c'
            literal = value.name in self.byte_constants
            if path in files:
                require(literal and files[path] == content, 'conflicting canonical byte constant')
                continue
            files[path] = content
            artifacts.append({'symbol': symbol, 'original': value.name, 'kind': 'function' if value.is_function else 'global',
                              'linkage': 'linkonce_odr' if literal else value.linkage.name,
                              'shareable': literal or value.linkage.name in SHARED,
                              'path': path, 'sha256': digest(content), 'signature': signature, 'dependencies': deps,
                              'runtimeDependencies': runtime_deps, **self.array_extent(value)})
            if value.name in self.constructors:
                artifacts[-1]['constructorPriority'] = self.constructors[value.name]
        externals = []
        def emit_bridge(name,signature):
            content,imports=self.bridges[name]
            path='definitions/'+digest(PROFILE+'\0'+name)+'.c'
            files[path]=content
            artifacts.append({'symbol':name,'original':name,'kind':'function',
                'linkage':'linkonce_odr','shareable':True,'path':path,'sha256':digest(content),
                'signature':signature,'dependencies':sorted(imports),'runtimeDependencies':[]})
            externals.extend({'symbol':n,'signature':s} for n,s in imports.items())
        for value in self.symbols.values():
            if value.is_declaration and not value.name.startswith('llvm.'):
                if self.profile['exceptions'] and value.name == '__gxx_personality_v0':
                    continue
                signature = self.prototype(value) if value.is_function else self.global_decl(value, external=True)
                if self.wasi_libc and value.name in self.bridges:
                    if value.name in runtime_abi.LAYOUTS:
                        args=list(value.arguments)
                        sizes=runtime_abi.LAYOUTS[value.name]
                        require(len(args)==len(sizes) and all(size is None or f'dereferenceable({size})' in b' '.join(arg.attributes).decode() for arg,size in zip(args,sizes)),
                                'libc++ adapter requires the pinned WASI object layout')
                    emit_bridge(value.name,signature)
                    continue
                externals.append({'symbol': self.names[value.name], 'signature': signature, **self.array_extent(value)})
        if self.wasi_libc:
            providers={a['symbol']:a['signature'] for a in artifacts}
            # Runtime facades can depend on other facades not mentioned by the
            # original LLVM module (for example the thread-exit TLS destructor).
            for row in externals:
                name=row['symbol']
                if name in self.bridges and name not in providers:
                    emit_bridge(name,row['signature']);providers[name]=row['signature']
                if name in providers:
                    require(providers[name]==row['signature'],f'{name}: conflicting runtime facade signature')
            externals=[row for row in externals if row['symbol'] not in providers]
        for name in sorted(self.used_runtime):
            if name in self.symbols and self.names.get(name) == name:
                value = self.symbols[name]
                require(value.is_function and self.prototype(value) == RUNTIME_SIGNATURES[name],
                        f'{name}: declaration conflicts with synthesized runtime helper')
            else:
                externals.append({'symbol': name, 'signature': RUNTIME_SIGNATURES[name]})
        entry = self.functions.get('__main_argc_argv')
        if entry is not None and not entry.is_declaration:
            require('main' not in self.symbols, 'ambiguous hosted main definitions')
            require(str(entry.global_value_type)=='i32 (i32, ptr)', 'unsupported hosted main signature')
            symbol=self.names[entry.name]
            content=('extern '+self.prototype(entry)+';\n'
                     'int main(int argc,char **argv){return (int)'+symbol+'((unsigned int)argc,(void *)argv);}\n')
            dependencies = [symbol]
            if self.profile['exceptions']:
                content=('__exception __wp_cxx_exception(unsigned int);\n'
                         'extern void __cxa_call_terminate(void *);\nextern '+self.prototype(entry)+';\n'
                         'int main(int argc,char **argv){int result; __try {result=(int)'+symbol+
                         '((unsigned int)argc,(void *)argv);} __catch __wp_cxx_exception(p){'
                         '__cxa_call_terminate((void *)p); __builtin_trap();} return result;}\n')
                dependencies.append('__cxa_call_terminate')
                externals.append({'symbol':'__cxa_call_terminate','signature':'void __cxa_call_terminate(void *)'})
            path='definitions/'+digest(PROFILE+'\0main')+'.c'
            files[path]=content
            artifacts.append({'symbol':'main','original':'main','kind':'function','linkage':'external',
                'shareable':False,'path':path,'sha256':digest(content),'signature':'int main(int, char * *)',
                'dependencies':dependencies,'runtimeDependencies':[],'hostedEntryAdapter':True})
        validate_runtime_cycles(artifacts)
        manifest = {'profile': self.profile, 'unitKey': self.unit_key, 'artifacts': sorted(artifacts, key=lambda a:a['symbol']),
                    'externals': sorted(externals, key=lambda a:a['symbol'])}
        return files, manifest


def publish(output, files, manifest):
    output = Path(output)
    require(not output.exists(), f'output already exists: {output}; choose a fresh directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.cpp-translate-', dir=output.parent))
    try:
        for name, content in files.items():
            dest = stage / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(content)
        (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        stage.rename(output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def translate(args):
    require(not (args.exception_runtime_rtti or args.rtti) or args.exceptions, 'RTTI requires exception profile')
    source = Path(args.source)
    if source.suffix == '.ll':
        text = source.read_text()
        producer = {'input': 'llvm-ir'}
    else:
        # O0 preserves concrete functions as independently linkable artifacts.
        # No LTO/inlining at this stage; optimized IR is a separately tested profile.
        command = [args.clang, '--target=wasm32-unknown-unknown', '-std=c++17',
                   '-fexceptions' if args.exceptions else '-fno-exceptions', '-frtti' if (args.exception_runtime_rtti or args.rtti) else '-fno-rtti', '-fno-use-cxa-atexit', '-fhosted', '-fno-builtin',
                   '-fno-vectorize', '-fno-slp-vectorize', '-ffp-contract=off', '-O0', '-S', '-emit-llvm',
                   '-Xclang', '-mno-constructor-aliases',
                   '-fno-discard-value-names', '-include', str(Path(__file__).with_name('frontend-abi.hpp')), '-x', 'c++']
        if args.wasi_libc:
            command += ['-include', str(Path(__file__).with_name('wasi-libc.hpp'))]
        # Flags are a bounded frontend configuration, not arbitrary output/ABI overrides.
        for flag in args.clang_arg:
            require(flag.startswith(('-I', '-D', '-U')), 'only -I/-D/-U extra clang arguments are supported')
        command += args.clang_arg + [str(source), '-o', '-']
        text = subprocess.check_output(command, text=True)
        producer = {'input': 'c++17', 'clang': subprocess.check_output([args.clang, '--version'], text=True).strip(),
                    'options': command[1:-3]}
    if args.ir_output:
        ir_path=Path(args.ir_output)
        require(not ir_path.exists(),'IR output already exists; choose a fresh path')
        ir_path.parent.mkdir(parents=True,exist_ok=True)
        ir_path.write_text(text)
    api = api_types(args.api_header, args.clang, args.clang_arg)
    if args.wasi_libc:
        runtime_abi.adapt_api(api)
    backend = Backend(text, args.unit_key, api, exceptions=args.exceptions)
    backend.profile['exceptions'] = 'itanium-libcxxabi-21.1.4-native-tag-v1' if args.exceptions else False
    backend.bridges = {name: row for name, row in runtime_abi.BRIDGES.items() if not args.exceptions or name.startswith('__wasm_posix_')}
    backend.wasi_libc = args.wasi_libc
    if args.internal_indirect_byval:
        backend.profile['internalIndirectByval'] = 'callee-copy-v1'
    backend.profile['wasiLibcAdapters'] = 3 if args.wasi_libc else None
    backend.profile['hostedMain'] = source.suffix != '.ll'
    backend.va8 = source.suffix != '.ll' or getattr(args, 'va8_ir', False)
    backend.profile['variadicSlots'] = 8 if backend.va8 else None
    # Producer options are part of compatibility; include search paths are not
    # identities. Emitted definitions and dependency declarations are checked too.
    backend.profile['frontend'] = {k: v for k, v in producer.items() if k != 'options'}
    backend.profile['frontend']['configuration'] = [f for f in args.clang_arg if not f.startswith('-I')]
    files, manifest = backend.emit()
    manifest['producer'] = producer
    manifest['omittedWasmStartupAliases'] = backend.omitted_entry_aliases
    manifest['provenance'] = {'sourceSha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                              'irSha256': digest(text),
                              'apiHeaderSha256': hashlib.sha256(Path(args.api_header).read_bytes()).hexdigest() if args.api_header else None}
    publish(args.output, files, manifest)
    return manifest


def link(args):
    manifests = [(Path(p), json.loads(Path(p).read_text())) for p in args.manifests]
    require(bool(manifests), 'at least one manifest is required')
    profile = manifests[0][1]['profile']
    require(profile.get('runtimeDependencies') == 2, 'regenerate manifests with synthesized runtime dependency tracking')
    providers, declarations, array_extents = {}, {}, {}
    for path, manifest in manifests:
        require(manifest['profile'] == profile, 'incompatible translation profiles')
        for decl in manifest.get('externals', []) + manifest['artifacts']:
            name, sig = decl['symbol'], decl['signature']
            require(name not in declarations or declarations[name] == sig, f'incompatible declarations for {name}')
            declarations[name] = sig
            if 'arrayExtent' in decl:
                extent = decl['arrayExtent']
                require(extent is None or (type(extent) is int and extent >= 0), 'invalid array extent')
                previous = array_extents.get(name)
                require(previous is None or extent is None or previous == extent,
                        f'incompatible array extents for {name}')
                array_extents[name] = extent if previous is None else previous
        for row in manifest['artifacts']:
            source = (path.parent / row['path']).resolve()
            require(source.is_relative_to(path.parent.resolve()), 'artifact path escapes manifest directory')
            content = source.read_text()
            require(digest(content) == row['sha256'], f'artifact digest mismatch: {row["symbol"]}')
            previous = providers.get(row['symbol'])
            if previous:
                old = previous[0]
                if (profile.get('exceptions') and row['symbol'].startswith(('_ZTI', '_ZTS'))
                        and ((row['linkage'] == 'external' and old['linkage'] in SHARED)
                             or (old['linkage'] == 'external' and row['linkage'] in SHARED))):
                    require(all(row[k] == old[k] for k in ('sha256', 'signature', 'dependencies', 'runtimeDependencies')),
                            f'incompatible exception typeinfo definition: {row["symbol"]}')
                    if row['linkage'] == 'external': providers[row['symbol']] = (row, content)
                    continue
                if profile.get('exceptions') and 'weak_any' in {row['linkage'], old['linkage']}:
                    # Resolve replaceable runtime definitions before emitting C.
                    # A strong definition wins; otherwise retain the first weak
                    # provider, matching ordinary native archive/link ordering.
                    if old['linkage'] == 'weak_any' and row['linkage'] != 'weak_any':
                        providers[row['symbol']] = (row, content)
                    continue
                require(row['shareable'] and old['shareable'], f'duplicate strong/local definition: {row["symbol"]}')
                require(all(row[k] == old[k] for k in ('sha256', 'signature', 'dependencies', 'runtimeDependencies', 'linkage')),
                        f'incompatible shared definition: {row["symbol"]}')
            else:
                providers[row['symbol']] = (row, content)
    # Check all duplicate candidates above, even if not selected. Never silently
    # choose between different same-named specializations supplied by libraries.
    validate_runtime_cycles([row for row, _ in providers.values()])
    selected = set()
    unresolved = set()
    pending = list(args.root or providers)
    pending += [name for name,(row,_) in providers.items() if 'constructorPriority' in row]
    require(all(n in providers for n in pending), 'requested root has no definition')
    while pending:
        name = pending.pop()
        if name in selected or name in unresolved:
            continue
        if name not in providers:
            unresolved.add(name)
            continue
        selected.add(name)
        pending += providers[name][0]['dependencies']
    files = {providers[n][0]['path']: providers[n][1] for n in sorted(selected)}
    manifest = {'profile': profile, 'artifacts': [providers[n][0] for n in sorted(selected)],
                'externals': [{'symbol': n, 'signature': declarations.get(n),
                               **({'arrayExtent': array_extents[n]} if n in array_extents else {})}
                              for n in sorted(unresolved)],
                'roots': args.root or [], 'sources': sorted(files)}
    publish(args.output, files, manifest)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('translate')
    p.add_argument('source')
    p.add_argument('--unit-key', required=True)
    p.add_argument('-o', '--output', required=True)
    p.add_argument('--clang', default=os.environ.get('CLANG', 'clang'))
    p.add_argument('--clang-arg', action='append', default=[])
    p.add_argument('--api-header', help='C header declaring public C imports and exports')
    p.add_argument('--va8-ir', action='store_true', help='Assert input IR was produced with frontend-abi.hpp (never for ordinary Clang IR)')
    p.add_argument('--exception-runtime-rtti', action='store_true', help='Enable RTTI inside the pinned exception runtime; not a general application ABI')
    p.add_argument('--internal-indirect-byval', action='store_true', help='Assert indirect byval targets use the translated callee-copy ABI; excludes external C aggregate callbacks')
    p.add_argument('--rtti', action='store_true', help='Itanium RTTI using pinned libc++abi within the native-tag exception profile; no shared C++ ABI claim')
    p.add_argument('--exceptions', action='store_true', help='Experimental Itanium cleanup/catch lowering over native Wasm tags')
    p.add_argument('--wasi-libc', action='store_true', help='Adapt WASI libc headers to the wasm-posix runtime')
    p.add_argument('--ir-output', help='Retain frontend IR for diagnostics, including failed C translation')
    p = sub.add_parser('link')
    p.add_argument('manifests', nargs='+')
    p.add_argument('--root', action='append', default=[])
    p.add_argument('-o', '--output', required=True)
    args = parser.parse_args()
    try:
        result = translate(args) if args.command == 'translate' else link(args)
        print(f'{len(result["artifacts"])} definition files: {args.output}')
    except (Unsupported, RuntimeError, subprocess.CalledProcessError) as e:
        parser.exit(1, 'cpp-translator: ' + str(e) + '\n')


if __name__ == '__main__':
    main()
