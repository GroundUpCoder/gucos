"""LLVM constant storage, preserving DataLayout offsets and C relocations."""
import re
import struct


class Storage:
    def layout_identity(self,t):
        if t in self.layout_ids:
            return self.layout_ids[t]
        self.check(not (t.is_struct and t.is_opaque_struct), 'opaque storage layout is unsupported')
        size,align = self.layout.get_abi_size(t),self.layout.get_abi_alignment(t)
        self.check(size < 1<<32,'global type exceeds wasm32 address space')
        if t.is_pointer:
            self.check(str(t)=='ptr','nondefault pointer address space unsupported')
        if t.is_struct:
            detail = ('struct',[(self.layout.get_element_offset(t,i),self.layout_identity(e))
                                for i,e in enumerate(t.elements)])
        elif t.is_array:
            detail = ('array',t.element_count,self.layout_identity(next(t.elements)))
        else:
            detail = (t.type_kind.name,str(t))
        value = self.hash(repr((size,align,detail)))
        self.layout_ids[t] = value
        return value

    def storage_tag(self,g):
        t = g.global_value_type
        full_identity = self.layout_identity(t)  # Retain full-object size validation.
        if self.profile.get('exceptions') and g.name.startswith('_ZTI'):
            # Clang's ABI-owned typeinfo references can be opaque declarations.
            # Consumers take the address only; duplicate definitions are still
            # checked byte-for-byte by the artifact resolver.
            return '__llc_storage_'+self.hash(self.names[g.name]+'\0exception-typeinfo-address')[:24]
        if self.profile.get('exceptions') and g.name.startswith('_ZTV'):
            self.vtable_extent(g)
            return '__llc_storage_'+self.hash(self.names[g.name]+'\0exception-vtable-pointers')[:24]
        # Clients only take addresses. An outer array declaration may omit its
        # bound; keep the pointee layout in the tag and check known bounds in
        # manifest metadata rather than giving compatible declarations new tags.
        identity = ('array-element:' + self.layout_identity(next(t.elements))
                    if t.is_array else full_identity)
        return '__llc_storage_'+self.hash(self.names[g.name]+'\0'+identity)[:24]

    def array_extent(self,g):
        if g.is_global and self.profile.get('exceptions') and g.name.startswith('_ZTV'):
            return {'arrayExtent': self.vtable_extent(g)}
        if not g.is_global or not g.global_value_type.is_array:
            return {}
        n = g.global_value_type.element_count
        return {'arrayExtent': None if g.is_declaration and n == 0 else n}

    def vtable_extent(self,g):
        t = g.global_value_type
        parts = list(t.elements) if t.is_struct else [t]
        count = 0
        for part in parts:
            self.check(part.is_array and str(next(part.elements)) == 'ptr', 'unsupported vtable storage')
            count += part.element_count
        self.check(self.layout.get_abi_size(t) == 4*count and self.layout.get_abi_alignment(t) == 4,
                   'vtable storage has padding or unexpected alignment')
        return None if g.is_declaration and count == 0 else count

    def check_global(self,g):
        if g.name in self.checked_globals:
            return
        self.check(g.linkage.name in {'private','internal'} or g.name.startswith('_Z') or g.visibility.name == 'hidden',
                   g.name+': unmangled public data ABI is not supported yet')
        self.check(str(g.type)=='ptr','nondefault global address space unsupported')
        text=str(g)
        head=re.match(r'^@(?:"(?:[^"\\]|\\.)*"|[-\w.$]+)\s*=\s*',text)
        self.check(head is not None,'unsupported global symbol spelling')
        payload=text[head.end():]
        keyword=re.search(r'\b(?:global|constant)\s+',payload)
        self.check(keyword is not None,'missing global storage kind')
        rest=payload[keyword.end():]
        if not g.is_declaration:
            rest=self.untype(rest,g.global_value_type)
        _,attributes=self.split_field(rest)
        self.check(not re.search(r'\b(thread_local|externally_initialized|section)\b',payload[:keyword.start()]+' '+attributes),
                   g.name+': unsupported global storage')
        self.checked_globals.add(g.name)

    def fields(self, text):
        # Split in one scan; repeatedly slicing the remaining tail is quadratic
        # for HarfBuzz's large Unicode lookup tables.
        start, depth, quoted, escaped = 0, 0, False, False
        for i,c in enumerate(text):
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
            elif c in ']}> )'.replace(' ',''):
                depth -= 1
            elif c == ',' and depth == 0:
                yield text[start:i].strip()
                start = i+1
        if text[start:].strip():
            yield text[start:].strip()

    def type_spelling(self, t):
        if t.is_struct and not t.is_literal_struct:
            return '%' + (t.name if re.fullmatch(r'[\w.]+', t.name) else '"'+t.name+'"')
        return str(t)

    def untype(self, text, t):
        token = self.type_spelling(t)
        self.check(text.startswith(token+' '), 'constant has unexpected type: '+text[:160])
        return text[len(token)+1:].strip()

    def constant_pointer(self, value):
        value = value.strip()
        if value == 'null':
            return '0', set()
        if value.startswith('inttoptr '):
            match = re.fullmatch(r'inttoptr \(i(8|16|32|64) (-?\d+) to ptr\)',value)
            self.check(match is not None,'unsupported integer pointer initializer')
            number = int(match[2]) % (1<<int(match[1])) & 0xffffffff
            return f'(void *){number}U', set()
        if value.startswith('@'):
            name = value[1:]
            if name.startswith('"'):
                name = name[1:-1]
                name = re.sub(r'\\([0-9a-fA-F]{2})',lambda m:chr(int(m[1],16)),name)
            self.check(name in self.names, 'unknown constant symbol '+name)
            self.check(not self.api.get(name, {}).get('va_list_params'),
                       'address of public va_list function requires a callback ABI adapter')
            return '(void *)&'+self.names[name], {name}
        if value.startswith('getelementptr '):
            rest = re.sub(r'^getelementptr\s+(?:(?:inbounds|nuw|nusw|inrange\(-?\d+,\s*-?\d+\))\s+)*', '', value)
            self.check(rest.startswith('(') and rest.endswith(')'), 'invalid constant GEP')
            parts = list(self.fields(rest[1:-1]))
            t = self.parse_type(parts[0])
            self.check(parts[1].startswith('ptr '), 'nondefault constant pointer address space')
            base, deps = self.constant_pointer(parts[1][4:])
            offset = 0
            for pos, index in enumerate(parts[2:]):
                match = re.fullmatch(r'i(\d+) (-?\d+)', index)
                self.check(match is not None, 'noninteger constant GEP index')
                width, n = int(match[1]), int(match[2])
                n %= 1 << width
                if n & (1 << (width-1)):
                    n -= 1 << width
                if pos == 0:
                    offset += n * self.layout.get_abi_size(t)
                elif t.is_struct:
                    elems = list(t.elements)
                    self.check(0 <= n < len(elems), 'invalid constant struct index')
                    offset += self.layout.get_element_offset(t,n)
                    t = elems[n]
                elif t.is_array:
                    t = next(t.elements)
                    offset += n * self.layout.get_abi_size(t)
                else:
                    self.check(False, 'invalid constant GEP type')
            if not deps:
                match=re.fullmatch(r'(?:\(void \*\))?(\d+)U?',base)
                self.check(match is not None,'unsupported numeric pointer expression')
                return f'(void *){(int(match[1])+offset)&0xffffffff}U',set()
            return f'(void *)((unsigned char *)({base}) + ({offset}))', deps
        self.check(False, 'unsupported pointer initializer: '+value[:160])

    def flatten_constant(self, t, value, data, relocations, offset=0):
        if value in {'zeroinitializer', 'undef'}:
            # A deterministic zero initializer is a valid refinement of LLVM
            # undef storage, including the padding of empty C++ functors.
            return
        size = self.layout.get_abi_size(t)
        if t.is_pointer:
            expr, deps = self.constant_pointer(value)
            if expr != '0':
                relocations.append((offset,expr,deps))
            return
        if t.type_kind.name == 'integer':
            self.check(re.fullmatch(r'-?\d+|true|false',value) is not None, 'invalid constant integer')
            n = int({'true':'1','false':'0'}.get(value,value)) % (1 << t.type_width)
            data.extend((offset+i,b) for i,b in enumerate(n.to_bytes(size,'little')) if b)
            return
        if t.type_kind.name in {'float','double'}:
            # LLVM hexadecimal float syntax uses the double representation even
            # for float; decimal syntax is rounded to the declared precision.
            if value.startswith('0x') and (int(value[2:],16) >> 52) & 0x7ff == 0x7ff:
                bits=int(value[2:],16)
                if size==4:
                    bits=((bits >> 32) & 0x80000000) | 0x7f800000 | ((bits & ((1<<52)-1)) >> 29)
                encoded=bits.to_bytes(size,'little')
            else:
                n = struct.unpack('>d',int(value[2:],16).to_bytes(8,'big'))[0] if value.startswith('0x') else float(value)
                encoded=struct.pack('<f' if size == 4 else '<d',n)
            data.extend((offset+i,b) for i,b in enumerate(encoded) if b)
            return
        if t.is_array and str(next(t.elements)) == 'i8' and value.startswith('c"'):
            text, decoded = value[2:-1], bytearray()
            i = 0
            while i < len(text):
                if text[i] == '\\':
                    if text[i:i+2] == '\\\\':
                        decoded.append(92); i += 2
                        continue
                    self.check(re.fullmatch('[0-9A-Fa-f]{2}',text[i+1:i+3]) is not None,'invalid byte string escape')
                    decoded.append(int(text[i+1:i+3],16)); i += 3
                else:
                    decoded.append(ord(text[i])); i += 1
            self.check(len(decoded) == size,'constant string size mismatch')
            data.extend((offset+i,b) for i,b in enumerate(decoded) if b)
            return
        if t.is_array or t.is_struct:
            if t.is_struct and t.is_packed_struct:
                self.check(value.startswith('<{') and value.endswith('}>'),'invalid packed initializer')
                inner = value[2:-2]
            else:
                self.check(value[0] == ('[' if t.is_array else '{') and value[-1] == (']' if t.is_array else '}'), 'invalid aggregate initializer')
                inner = value[1:-1]
            elems = list(t.elements)
            count = t.element_count if t.is_array else len(elems)
            fields = list(self.fields(inner))
            self.check(len(fields) == count, 'aggregate initializer field count mismatch')
            for i, field in enumerate(fields):
                child = elems[0] if t.is_array else elems[i]
                child_offset = i*self.layout.get_abi_size(child) if t.is_array else self.layout.get_element_offset(t,i)
                self.flatten_constant(child,self.untype(field,child),data,relocations,offset+child_offset)
            return
        self.check(False, 'unsupported constant type '+str(t))

    def storage(self, g):
        if g.name in self.storage_cache:
            return self.storage_cache[g.name]
        self.check_global(g)
        text = str(g)
        t = g.global_value_type
        size = self.layout.get_abi_size(t)
        match = re.search(r'\balign (\d+)', text)
        alignment = int(match[1]) if match else self.layout.get_abi_alignment(t)
        # Keep only explicit nonzero bytes. A huge zeroinitializer is BSS, not
        # a request to allocate or print its entire address range in Python.
        data, relocations = [], []
        if not g.is_declaration:
            rest = re.split(r'\b(?:global|constant) ',text,maxsplit=1)[1]
            init = self.split_field(self.untype(rest,t))[0]
            self.flatten_constant(t,init,data,relocations)
        # A relocation-bearing aggregate gets a deterministic C storage struct.
        # Explicit byte spans preserve LLVM padding; pointer members let the C
        # linker perform actual relocations instead of baking host addresses.
        name = self.names[g.name]
        parts, values, dependencies = [], [], set()
        data_cursor = 0
        def span(start,end):
            nonlocal data_cursor
            if end > start:
                parts.append(f'unsigned char b{start}[{end-start}];')
                pieces,next_offset = [],start
                while data_cursor<len(data) and data[data_cursor][0]<end:
                    offset,byte = data[data_cursor]
                    self.check(offset>=next_offset,'overlapping constant initializer bytes')
                    gap = offset-next_offset
                    if gap<=8:
                        pieces.extend('0' for _ in range(gap))
                        pieces.append(str(byte))
                    else:
                        pieces.append(f'[{offset-start}]={byte}')
                    next_offset=offset+1;data_cursor+=1
                values.append('{'+(','.join(pieces) or '0')+'}')
        cursor = 0
        for offset, expr, deps in relocations:
            self.check(offset >= cursor,'overlapping pointer relocation unsupported')
            span(cursor,offset)
            parts.append(f'void *p{offset};'); values.append(expr); dependencies.update(deps)
            cursor = offset+4
        span(cursor,size)
        self.check(data_cursor==len(data),'constant initializer bytes outside storage')
        if not parts:
            parts,values = ['unsigned char empty[1];'],['{0}']
        tag = self.storage_tag(g)
        types = f'struct {tag} {{ '+ ' '.join(parts) +' } __attribute__((packed));'
        decl = f'struct {tag} {name}'
        initializer = '{'+', '.join(values)+'}'
        decl += f' __attribute__((aligned({alignment})))'
        result = (types, decl, initializer, dependencies)
        self.storage_cache[g.name] = result
        return result

    def global_decl(self,g,external=False):
        self.check_global(g)
        if external:
            # Only addresses are used by generated clients. An incomplete tag
            # avoids repeating storage layouts in every referring artifact and
            # is identical whether or not the initializer is visible in this TU.
            tag = self.storage_tag(g)
            return f'struct {tag};\nextern struct {tag} {self.names[g.name]};'
        types,decl,init,_ = self.storage(g)
        return (types+'\n' if types else '') + ('extern '+decl+';' if external else decl+' = '+init+';')

    def global_definition(self,g):
        types,decl,init,deps = self.storage(g)
        declarations = []
        for name in sorted(deps-{g.name}):
            if name in self.functions:
                declarations.append('extern '+self.prototype(self.functions[name])+';')
            else:
                declarations.append(self.global_decl(self.globals[name],external=True))
        text = '\n'.join(declarations)
        return '\n'.join([*self.forwards(text),text,types,decl+' = '+init+';', '']), sorted(self.names[n] for n in deps-{g.name})
