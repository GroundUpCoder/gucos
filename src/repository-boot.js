// Stable mode-owned storage, independent of repository URL, generation and runtime.
// Repository mode never opens the legacy disk or imports its editor saves.
function installationStorage(repository) {
  if(repository===undefined)return {disk:'wasm-posix-busybox-v1',journal:'wasm-posix-busybox-v1.journal',edits:'wasm-posix.files'};
  repositoryBootConfig(repository);
  return {disk:'wasm-posix-core-v1',journal:'wasm-posix-core-v1.journal',edits:'wasm-posix.core.files'};
}
// Bundle-owned repository manager. Fetch all companions before changing the
// installed manager; never compile from mutable installed source on a reload.
function repositoryBootConfig(value) {
  if(!value||value.format!==1||typeof value.url!=='string'||typeof value.runtime!=='string'||!/^[a-f0-9]{64}$/.test(value.runtime))throw Error('Invalid repository boot configuration');
  const url=new URL(value.url);
  if(!['http:','https:'].includes(url.protocol)||url.username||url.password||url.search||url.hash)throw Error('Invalid repository boot URL');
  if(value.desktopLauncher!==undefined&&(typeof value.desktopLauncher!=='string'||!(/^[a-z0-9][a-z0-9+.-]{0,127}$/).test(value.desktopLauncher)))throw Error('Invalid repository desktop launcher');
  return {format:1,url:url.href,runtime:value.runtime,platform:value.platform,...(value.desktopLauncher===undefined?{}:{desktopLauncher:value.desktopLauncher})};
}
// The boot configuration selects one reviewed launcher, not a bundled catalog.
// Legacy disks keep their original GTK action. A repository must opt in explicitly.
function desktopLaunchCommand(repository) {
  if(repository===undefined)return typeof DISTRIBUTION_LAYOUT==='undefined'?null:DISTRIBUTION_LAYOUT.desktopLauncher;
  const config=repositoryBootConfig(repository);
  return config.desktopLauncher?'gucman install '+config.desktopLauncher+' && exec gucman run '+config.desktopLauncher+' </dev/null':null;
}
async function installRepositoryManager(data,rawAsset,writeFile,readFile) {
  const config=repositoryBootConfig(data.repository);
  const paths=['/usr/libexec/guc-lock','/usr/libexec/guc-json','/usr/libexec/guc-repo-fetch','/usr/libexec/guc-unpack','/usr/libexec/guc-links-v2'];
  const prepared=[];
  for(const path of paths){
    const entry=data.prebuilt?.[path];
    if(!entry||(!entry.url&&!entry.bytes))throw Error('Missing repository companion: '+path);
    prepared.push([path,entry.url?await rawAsset(entry.url):entry.bytes]);
  }
  if(typeof data.repositoryScripts?.manager!=='string'||!data.repositoryScripts.manager.startsWith('#!/bin/sh\n'))throw Error('Missing package manager');
  for(const [path,bytes] of prepared)writeFile(path,bytes,0o755);
  writeFile('/usr/libexec/guc-manager',data.repositoryScripts.manager,0o755);
  writeFile('/usr/bin/gucman','#!/bin/sh\nset -eu\nmkdir -p /usr/local/lib/gucman\nexec /usr/libexec/guc-lock /usr/local/lib/gucman/transaction.lock /bin/sh /usr/libexec/guc-manager "$@"\n',0o755);
  writeFile('/etc/gucman/runtime',config.runtime+'\n',0o644);
  if(config.platform)writeFile('/etc/gucman/platform.json',JSON.stringify(config.platform)+'\n',0o644);
  if(!readFile('/etc/gucman/initialized')){
    writeFile('/etc/gucman/sources.d/groundupcoder.url',config.url+'\n',0o644);
    writeFile('/etc/gucman/initialized','1\n',0o644);
  }
}
