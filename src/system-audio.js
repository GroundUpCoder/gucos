// The kernel mixes process streams into one float32 stereo ring. This receiver
// owns only Web Audio scheduling; process/thread lifetime stays in the kernel.
function createSystemAudioPlayer({sab,bufferSize,freq,channels}) {
  const control=new Int32Array(sab,0,4),ring=new DataView(sab,16,bufferSize);
  const context=new AudioContext({sampleRate:freq}),gain=context.createGain();
  gain.gain.value=0.16;gain.connect(context.destination);
  const sources=new Set();let next=0,readPos=0,closed=false;
  const resume=()=>{if(!closed&&context.state==='suspended')void context.resume().catch(()=>{});};
  document.addEventListener('pointerdown',resume);document.addEventListener('keydown',resume);
  resume();
  function flush(){
    if(closed||context.state!=='running'||sources.size>=3)return;
    const queued=Math.max(0,Atomics.load(control,1));
    const frames=Math.min(Math.floor(queued/(channels*4)),Math.ceil(freq*0.04));
    if(!frames)return;
    const bytes=frames*channels*4;
    const start=readPos;
    const buffer=context.createBuffer(channels,frames,freq);
    for(let ch=0;ch<channels;ch++){
      const output=buffer.getChannelData(ch);
      for(let i=0;i<frames;i++)output[i]=ring.getFloat32((start+(i*channels+ch)*4)%bufferSize,true);
    }
    readPos=(readPos+bytes)%bufferSize;
    Atomics.sub(control,1,bytes);
    const source=context.createBufferSource();source.buffer=buffer;source.connect(gain);
    source.onended=()=>{sources.delete(source);source.disconnect();};sources.add(source);
    next=Math.max(next,context.currentTime+0.005);source.start(next);next+=frames/freq;
  }
  const timer=setInterval(flush,15);
  return {flush,close(){
    if(closed)return;closed=true;clearInterval(timer);
    document.removeEventListener('pointerdown',resume);document.removeEventListener('keydown',resume);
    for(const source of sources){source.onended=null;source.stop();source.disconnect();}sources.clear();
    gain.disconnect();void context.close().catch(()=>{});
  }};
}
