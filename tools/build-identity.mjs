import {createHash} from 'node:crypto';
export const digest = text => createHash('sha256').update(text).digest('hex');
export function buildIdentity(versions, compilerSources) {
  if (!/^\d+\.\d+\.\d+$/.test(versions.compiler) || !/^\d+\.\d+\.\d+$/.test(versions.runtime)) throw Error('Invalid compiler/runtime version');
  for (const name of ['objects','runtime']) if (!Number.isSafeInteger(versions.abi[name]) || versions.abi[name]<1) throw Error('Invalid ABI epoch: '+name);
  const abi={objects:versions.abi.objects,runtime:versions.abi.runtime};
  return {version:versions.compiler, runtimeVersion:versions.runtime,
    live:digest(compilerSources), id:digest(JSON.stringify(abi)), abi};
}
export function validateVersions(versions, packages) {
  const names=new Set(packages.map(p=>p.name));
  for(const name of names) if(!/^\d+(?:\.\d+){2,}-[1-9]\d*$/.test(versions.packages[name]||'')) throw Error('Missing/invalid package port version: '+name);
  for(const name of Object.keys(versions.packages)) if(!names.has(name)) throw Error('Unknown versioned package: '+name);
}
