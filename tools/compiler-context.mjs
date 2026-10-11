// Explicit Node VM adapter: install disk libc when a compiler script is loaded.
import vm from 'node:vm';
import {installLibc} from '../src/libc-sources.js';
const installed=new WeakSet();
export function runCompilerScript(source,context,options) {
  const result=vm.runInContext(source,context,options);
  const compiler=context.CompilerJS;
  if(compiler?.setStdlibSources&&!installed.has(compiler)) {
    // Bundled scripts already carry their own provider.
    try { compiler.getStdlibProvider(); } catch { installLibc(compiler); }
    installed.add(compiler);
  }
  return result;
}
