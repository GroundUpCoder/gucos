import {useEffect,useMemo,useState} from 'react';
import {FileText,X,Check} from 'lucide-react';
import {MemoizedMarkdown} from './cc/components/custom/MemoizedMarkdown';
import {SourceCodeView} from './cc/components/custom/SourceCodeView';
import {useReaderSize,READER_SIZE_PROSE_CLASSES} from './cc/hooks/use-reader-size';
const session=()=>window.parent.shell.agentBridge.session();
function guestPath(path:string){return decodeURIComponent(new URL(path,'https://guest/home/user/').pathname);}
function GuestFile({path,label,embed=false}:{path:string,label:any,embed?:boolean}){
 const [open,setOpen]=useState(false),[text,setText]=useState(''),[error,setError]=useState(''),[saved,setSaved]=useState(false);
 const full=guestPath(path);
 async function load(){setOpen(true);try{const bytes=await session().read(full);if(!bytes)throw Error('File not found');setText(new TextDecoder().decode(bytes));}catch(e){setError(e.message);}}
 return <><button onClick={load} className={embed?'not-prose flex items-center gap-2 w-full rounded-md border border-border bg-muted/30 hover:bg-muted/60 px-3 py-2 my-1 text-left':'text-primary underline'}>{embed&&<FileText className="w-4 h-4"/>}{label||path}</button>{open&&<div role="dialog" aria-label={full} className="fixed inset-0 z-50 bg-background flex flex-col"><header className="flex p-3 border-b gap-3"><span className="flex-1 truncate font-mono">{full}</span><button onClick={()=>setOpen(false)} aria-label="Close file"><X/></button></header>{error?<p role="alert">{error}</p>:<><div className="overflow-auto flex-1"><SourceCodeView content={text} fileName={full}/></div><details className="p-3 border-t"><summary>Edit file</summary><textarea aria-label="File contents" value={text} onChange={e=>{setText(e.target.value);setSaved(false);}} className="w-full h-48 font-mono text-sm bg-background border p-2"/><button onClick={async()=>{try{await session().write(full,new TextEncoder().encode(text));setSaved(true);}catch(e){setError(e.message);}}} className="px-3 py-2 rounded bg-primary text-primary-foreground">{saved?<Check/>:'Save'}</button></details></>}</div>}</>;
}
function GuestMedia({path,alt}:{path:string,alt?:string}){
 const [url,setURL]=useState(''),[error,setError]=useState('');const ext=path.split('.').pop()?.toLowerCase();
 useEffect(()=>{let disposed=false,objectURL='';session().read(guestPath(path)).then(bytes=>{if(!bytes)throw Error('File not found: '+path);if(!disposed){objectURL=URL.createObjectURL(new Blob([bytes],{type:({png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',mp4:'video/mp4',mp3:'audio/mpeg',wav:'audio/wav',webp:'image/webp'} as any)[ext]||'application/octet-stream'}));setURL(objectURL);}}).catch(e=>setError(e.message));return()=>{disposed=true;if(objectURL)URL.revokeObjectURL(objectURL);};},[path]);
 if(error)return <span>{error}</span>;if(!url)return <span>Loading {path}…</span>;
 return <span className="not-prose block">{['mp4','webm'].includes(ext)?<video src={url} controls className="max-w-full rounded"/>:['mp3','wav','ogg'].includes(ext)?<audio src={url} controls className="w-full"/>:<img src={url} alt={alt} className="max-w-full rounded"/>}<span className="text-xs text-muted-foreground font-mono">{path}</span></span>;
}
export function GuestTextBlock({content}:{content:string}){
 const [size]=useReaderSize();const components=useMemo(()=>({
 a:({href,children}:any)=>!href||/^(https?:|mailto:|#)/.test(href)?<a href={href} target="_blank" rel="noopener noreferrer">{children}</a>:<GuestFile path={href} label={children}/>,
 img:({src,alt}:any)=>/^(https?:|data:)/.test(src)?<img src={src} alt={alt}/>: /\.(png|jpe?g|webp|gif|mp4|webm|mp3|wav|ogg)$/i.test(src)?<GuestMedia path={src} alt={alt}/>:<GuestFile path={src} label={alt} embed/>,
 }),[]);
 if(!content.trim())return null;
 return <div data-testid="text-block" className={`prose ${READER_SIZE_PROSE_CLASSES[size]} dark:prose-invert max-w-none`}><MemoizedMarkdown components={components}>{content}</MemoizedMarkdown></div>;
}
