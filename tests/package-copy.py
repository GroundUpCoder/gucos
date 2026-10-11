"""Native bulk-copy boundary tests. Run under machine-job, passing helper path."""
import hashlib, os, subprocess, sys, tempfile
from pathlib import Path

helper = str(Path(sys.argv[1]).resolve())
assert os.environ.get('MACHINE_JOB_ID'), 'machine-job required'
with tempfile.TemporaryDirectory(prefix='guc-copy-test-') as tmp:
    root = Path(tmp)
    stage = root/'stage'; stage.mkdir()
    outputs = root/'outputs'
    (stage/'bin').mkdir(); (stage/'share').mkdir()
    (stage/'bin/probe').write_bytes(bytes(range(256))*600)
    (stage/'share/empty').write_bytes(b'')
    rows = [('bin/probe', 493), ('share/empty', 420)]
    for i in range(527):
        p = f'share/header-{i:04}.h'
        (stage/p).write_text(f'#define VALUE_{i} {i}\n')
        rows.append((p, 420))
    outputs.write_text(''.join(f'{p}\t{m}\n' for p,m in rows))
    release = root/'release'; release.mkdir()
    result = subprocess.run([helper,'copy',str(stage),str(release),str(outputs),'probe'], capture_output=True)
    assert result.returncode == 0, result.stderr
    assert b'529/529 files' in result.stderr
    for p,m in rows:
        assert (release/p).read_bytes() == (stage/p).read_bytes()
        assert (release/p).stat().st_mode & 0o777 == m
    checksums = {p:hashlib.sha256((release/p).read_bytes()).hexdigest() for p,m in rows}
    assert subprocess.run([helper,'copy',str(stage),str(release),str(outputs),'probe'],capture_output=True).returncode
    assert checksums == {p:hashlib.sha256((release/p).read_bytes()).hexdigest() for p,m in rows}
    bad_lists = [b'bin/probe\t511\n', b'../escape\t420\n', b'/escape\t420\n',
                 b'bin/probe\t493\nbin/probe\t493\n', b'bin/probe\t493\nshare/missing\t420\n',
                 b'lib/gucman/current\t420\n', b'lib/wasm-posix/current\t420\n',
                 b'share/a b\t420\n', b'bin/probe\x00invisible\t493\n',
                 b'bin/probe\t493\nshare/empty/child\t420\n', b'',
                 b'bin/probe\t493\nshare/parent-link/empty\t420\n', b'share/file-link\t420\n']
    (stage/'share/parent-link').symlink_to(stage/'share',target_is_directory=True)
    (stage/'share/file-link').symlink_to(stage/'share/empty')
    for i,data in enumerate(bad_lists):
        dest=root/f'bad-{i}';dest.mkdir();outputs.write_bytes(data)
        result=subprocess.run([helper,'copy',str(stage),str(dest),str(outputs),'probe'],capture_output=True)
        assert result.returncode, (i,result.stderr)
        assert not list(dest.iterdir()), i
    outputs.write_text('share/empty\t420\n')
    dest=root/'parent-destination';dest.mkdir();(dest/'share').symlink_to(release/'share',target_is_directory=True)
    assert subprocess.run([helper,'copy',str(stage),str(dest),str(outputs),'probe'],capture_output=True).returncode
print('PASS 529 outputs, binary/empty content, modes, duplicates, missing files, traversal, reserved state, symlink files/parents, no overwrite')
