"""Audit a retained Pages release against an exact preview or production URL."""
import concurrent.futures, hashlib, json, pathlib, sys, urllib.request, urllib.error
base, release, out = sys.argv[1:]
root = pathlib.Path(release)
manifest = json.loads((root / 'pages-release.json').read_text())
build = json.loads((root / 'build.json').read_text())
headers = {'User-Agent': 'Mozilla/5.0 (gucOS release verification)', 'Accept': '*/*'}
def check(item):
    name, want = item
    if name == '_headers':
        return None
    url = base + ('/' if name == 'index.html' else '/' + name.removesuffix('.html'))
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        data = response.read()
        assert len(data) == want['size'], name
        assert hashlib.sha256(data).hexdigest() == want['sha256'], name
        for key, value in [('Cross-Origin-Opener-Policy', 'same-origin'), ('Cross-Origin-Embedder-Policy', 'require-corp'), ('Cross-Origin-Resource-Policy', 'same-origin')]:
            assert response.headers.get(key) == value, (name, key, response.headers.get(key))
        cache = response.headers.get('Cache-Control', '')
        assert ('immutable' in cache if name.startswith('assets/') else 'no-cache' in cache), (name, cache)
    return name
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    files = [name for name in pool.map(check, manifest['files'].items()) if name]
for name in ['absent-release-probe-' + build['runtime'][:12], 'release.json', 'pages-release.json']:
    try:
        urllib.request.urlopen(urllib.request.Request(base + '/' + name, headers=headers), timeout=30)
        raise AssertionError(name + ' should be 404')
    except urllib.error.HTTPError as error:
        assert error.code == 404, (name, error.code)
record = {'status': 'passed', 'url': base, 'files': len(files), 'runtime': build['runtime'], 'checks': ['exact sizes and SHA256', 'COOP/COEP/CORP', 'cache policy', 'missing objects and operator manifests return 404']}
pathlib.Path(out).write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record))
