// Shared virtual-filesystem path normalization.
function normalize(path) {
  const parts = [];
  for (const p of path.split('/')) {
    if (p === '..') parts.pop();
    else if (p && p !== '.') parts.push(p);
  }
  return '/' + parts.join('/');
}
