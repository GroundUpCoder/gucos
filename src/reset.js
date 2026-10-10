// Run before importing editor saves or opening the filesystem owner.
async function resetInstallation(storage) {
  const url = new URL(location.href);
  if (url.searchParams.get('reset') !== '1') return;
  url.searchParams.delete('reset');
  history.replaceState(null, '', url);
  if (!confirm('Reset wasm-posix to a fresh install? All files, edits, and compiled programs on this site’s wasm-posix disk will be deleted.')) return;
  const source = `onmessage = async () => {
    const handles = [];
    let result;
    try {
      const root = await navigator.storage.getDirectory();
      // Acquire both exclusive locks before changing either file. Another
      // session must never observe a disk with only its journal reset.
      for (const name of ${JSON.stringify([storage.disk,storage.journal])})
        handles.push(await (await root.getFileHandle(name, {create:true})).createSyncAccessHandle());
      // Clear redo records before the image, so an interrupted reset cannot
      // replay stale records into an empty filesystem.
      for (const handle of handles.slice().reverse()) { handle.truncate(0); handle.flush(); }
      result = {ok:true};
    } catch (error) {
      result = {error:error.name + ': ' + error.message};
    } finally { for (const handle of handles) handle.close(); }
    postMessage(result);
  };`;
  const workerURL = URL.createObjectURL(new Blob([source], {type:'text/javascript'}));
  const worker = new Worker(workerURL);
  try {
    await new Promise((resolve, reject) => {
      worker.onmessage = ({data}) => data.ok ? resolve() : reject(new Error(data.error));
      worker.onerror = event => reject(new Error(event.message));
      worker.postMessage({});
    });
    localStorage.removeItem(storage.edits);
  } finally { worker.terminate(); URL.revokeObjectURL(workerURL); }
}
