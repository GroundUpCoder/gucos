// Shared layout for the persistent system and the file:// fallback.
const HOME_DIRECTORY='/home/user';
const SYSTEM_DIRECTORIES=['/','/bin','/sbin','/dev','/etc','/home',HOME_DIRECTORY,'/lib','/mnt','/opt','/proc','/root','/run','/tmp','/usr','/usr/bin','/usr/sbin','/usr/lib','/usr/libexec','/usr/local','/usr/local/bin','/usr/local/lib','/usr/share','/usr/share/games','/usr/src','/var','/var/cache','/var/log','/var/tmp',...(typeof DISTRIBUTION_LAYOUT==='undefined'?[]:DISTRIBUTION_LAYOUT.directories)];
function legacyPath(path) {
  if(path==='/vendor'||path.startsWith('/vendor/'))return '/usr/src'+path.slice(7);
  if(path==='/cc-driver.c')return '/usr/libexec/cc-driver.c';
  if(typeof DISTRIBUTION_LAYOUT!=='undefined'&&Object.hasOwn(DISTRIBUTION_LAYOUT.paths,path))return DISTRIBUTION_LAYOUT.paths[path];
  if(/^\/[^/]+$/.test(path))return HOME_DIRECTORY+path;
  return path;
}
