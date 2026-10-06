#pragma once
// La pagina del depurador (/debug): ver handleDebugPage en Wifi_module_01.ino
static const char DEBUG_PAGE[] = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SD81 debugger</title>
<style>
:root{--bg:#f4f4f2;--pn:#fff;--bd:#d6d6d0;--tx:#222;--mu:#777;--pc:#ffe9a8;--bp:#d33;--hd:#e9e9e4}
body{font-family:sans-serif;margin:0;background:var(--bg);color:var(--tx)}
header{display:flex;flex-wrap:wrap;align-items:center;gap:8px;padding:8px 12px;background:var(--pn);border-bottom:1px solid var(--bd);position:sticky;top:0;z-index:2}
header h1{font-size:18px;margin:0 8px 0 0}
#st{font-weight:bold;margin-right:auto}
.run{color:#080}.stop{color:#c00}.off{color:#888}
button{font-size:13px;padding:4px 9px;cursor:pointer}
main{display:grid;grid-template-columns:minmax(0,3fr) minmax(0,2fr);gap:10px;padding:10px;max-width:1300px;margin:0 auto}
@media(max-width:850px){main{grid-template-columns:1fr}}
.pn{background:var(--pn);border:1px solid var(--bd);border-radius:6px;overflow:hidden}
.pn h2{font-size:13px;margin:0;padding:5px 8px;background:var(--hd);display:flex;gap:6px;align-items:center}
.pn h2 span{margin-right:auto}
.pn h2 input{font:12px monospace;width:90px}
.mono{font:13px/1.45 monospace}
table{border-collapse:collapse;width:100%}
td{padding:0 6px;white-space:nowrap}
#dis tr:hover{background:#f0f4ff}
#dis tr.pc{background:var(--pc)}
td.bp{width:14px;cursor:pointer;color:var(--bp);text-align:center}
td.bp:hover::before{content:'\25CB';color:#bbb}
td.bp.on::before{content:'\25CF'!important;color:var(--bp)!important}
td.lab{color:#06c}
td.hx{color:var(--mu)}
#regs{display:grid;grid-template-columns:repeat(4,1fr);gap:2px 10px;padding:6px 8px}
#regs div{cursor:pointer}#regs div:hover{background:#f0f4ff}
#regs b{display:inline-block;width:30px;color:var(--mu);font-weight:normal}
#flags{padding:0 8px 6px}
#flags i{font-style:normal;padding:0 3px;margin-right:2px;border-radius:3px;background:#eee;color:#aaa}
#flags i.on{background:#c33;color:#fff}
#stk td:first-child,#mem td:first-child{color:var(--mu)}
#mem td.b{cursor:pointer;padding:0 2px}#mem td.b:hover{background:#f0f4ff}
#mem td.zx{color:#063;padding-left:10px}
#bps{padding:4px 8px}
#bps div{display:flex;justify-content:space-between}
#bps a{cursor:pointer;color:var(--bp);text-decoration:none}
.row{display:flex;gap:4px;padding:4px 8px;flex-wrap:wrap}
.row input,.row select{font:12px monospace}
#out{background:#111;color:#ddd;font:12px/1.35 monospace;height:22vh;overflow:auto;white-space:pre;padding:6px 8px}
#cmd{width:100%;box-sizing:border-box;font:14px monospace;padding:5px;border:0;border-top:1px solid var(--bd)}
.dim{opacity:.45}
.full{grid-column:1/-1}
</style></head><body>
<header>
<h1>SD81 debugger</h1><span id="st" class="off">...</span>
<button data-c="p">Pause</button><button data-c="c">Continue</button>
<button data-c="s">Step</button><button data-c="o">Over</button><button data-c="u">Out</button>
<button data-c="snap">Snapshot</button><button data-c="ui">ZX81 screen</button><button id="capbtn">Capture</button>
<a href="/list?path=/" style="margin-left:8px">files</a>
</header>
<main>
<section class="pn" id="pdis">
<h2><span>Disassembly <small id="where"></small></span>
<label><input type="checkbox" id="follow" checked> follow PC</label>
<button id="dup">&#9650;</button><button id="ddn">&#9660;</button>
<input id="dat" placeholder="addr / symbol"><button id="dgo">Go</button></h2>
<table class="mono" id="dis"></table>
<div class="row" style="color:#777;font-size:12px">click the left column: breakpoint &middot; double click a line: run to it</div>
</section>
<div>
<section class="pn" id="preg"><h2><span>Registers</span><small id="slow"></small></h2>
<div class="mono" id="regs"></div><div class="mono" id="flags"></div></section>
<section class="pn" style="margin-top:10px"><h2><span>Breakpoints</span></h2>
<div class="mono" id="bps"></div>
<div class="row"><input id="bpa" placeholder="addr / symbol"><button id="bpadd">Add</button></div>
<div class="row"><select id="wm"><option value="w">write</option><option value="r">read</option><option value="io">I/O</option></select>
<input id="wa" placeholder="watch addr"><button id="wset">Watch</button><button data-c="w">Clear</button></div>
</section>
<section class="pn" style="margin-top:10px"><h2><span>Stack</span></h2><table class="mono" id="stk"></table></section>
</div>
<section class="pn full" id="pmem"><h2><span>Memory</span>
<button id="mup">&#9664;</button><button id="mdn">&#9654;</button>
<input id="mat" placeholder="addr / symbol"><button id="mgo">Go</button></h2>
<table class="mono" id="mem"></table></section>
<section class="pn full" id="pcap"><h2><span>Screen capture <small id="capinfo"></small></span>
<label><input type="checkbox" id="capspr" checked> sprites</label><button id="capgo">Capture</button><small id="capmsg"></small></h2>
<div style="padding:8px;text-align:center;background:#222"><canvas id="capcv" width="288" height="224" style="image-rendering:pixelated;width:576px;max-width:100%;background:#000"></canvas></div>
</section>
<section class="pn full"><h2><span>Console</span>
<button data-c="th 20">History</button><button data-c="t 100">Trace 100</button><button data-c="sym">Symbols</button>
<button data-c="h">Help</button><button id="clr">Clear</button></h2>
<div id="out"></div>
<input id="cmd" autocomplete="off" spellcheck="false" placeholder="command, as in the USB console (h for help) - Enter sends, arrows: history">
</section>
</main>
<script>
let seq=0,busy=false,queue=[],hist=[],hi=0,vhave=-1,vbusy=false,stopped=false;
let dis0=0,disN=0,mem0=0x4000,bpset=new Set(),regv={};
const $=id=>document.getElementById(id);
const out=$('out'),st=$('st'),cmd=$('cmd');
const ZX=' ▘▝▀▖▌▞▛▒▒▒"£$:?()><=+-*/;,.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ';
const hx=(v,n)=>v.toString(16).toUpperCase().padStart(n,'0');
const esc=t=>t.replace(/&/g,'&amp;').replace(/</g,'&lt;');
function add(t){
  if(!t)return;
  const end=out.scrollTop+out.clientHeight>=out.scrollHeight-4;
  out.textContent+=t;
  if(out.textContent.length>200000)out.textContent=out.textContent.slice(-150000);
  if(end)out.scrollTop=out.scrollHeight;
}
function state(s){
  if(!(s&1)){st.textContent='no debug monitor (/SYS/DEBUG.BIN)';st.className='off';return;}
  stopped=!!(s&2);
  let t=stopped?'STOPPED':'running';
  if(s&16)t+=' - tracing';
  if(s&32)t+=' - snapshot';
  if(s&4)t+=' - ZX81 screen';
  t+=(s&8)?' - history on':' - history off';
  st.textContent=t;st.className=stopped?'stop':'run';
  for(const id of['pdis','pmem'])$(id).classList.toggle('dim',!stopped);
}
async function poll(c){
  if(busy){if(c)queue.push(c);return;}
  busy=true;
  try{
    let u='/debug/poll?seq='+seq;
    if(c)u+='&c='+encodeURIComponent(c);
    const j=await (await fetch(u)).json();
    if(j.ok){
      if(j.lost)add('\n[... '+j.lost+' bytes lost ...]\n');
      add(j.text);seq=j.seq;state(j.state);
      if(j.vver!==vhave)view();
    }else{st.textContent='no answer from the STM32 (firmware without the web debugger?)';st.className='off';}
  }catch(e){st.textContent='connection error';st.className='off';}
  busy=false;
  if(queue.length)poll(queue.shift());
}
async function view(){
  if(vbusy)return;vbusy=true;
  try{
    const r=await fetch('/debug/view');
    if(r.ok)render(await r.text());
  }catch(e){}
  vbusy=false;
}
function render(doc){
  const L=doc.split('\n');
  let dis='',stk='',mem='',bps=[],watch='',stop=false;
  disN=0;
  for(const l of L){
    const k=l[0],a=l.slice(2);
    if(k==='V')vhave=parseInt(a);
    else if(k==='S'){const p=a.split(' ');const w=a.slice(a.indexOf(' ',a.indexOf(' ')+1)+1).split('|');
      stop=p[1]==='1';if(stop)$('where').textContent=w[0]+(w[1]?' at '+w[1]:'');}
    else if(k==='R')regs(a.split(' '));
    else if(k==='F'){const p=a.split(' ');$('follow').checked=p[0]==='1';dis0=parseInt(p[1],16);mem0=parseInt(p[2],16);}
    else if(k==='B')bps=a.trim()?a.trim().split(' '):[];
    else if(k==='W')watch=a;
    else if(k==='D'){
      const pc=l[2]==='>',bp=l[3]==='*',f=l.slice(4).split('|'),ad=parseInt(f[0],16);
      if(!disN)dis0=ad;disN=ad;
      dis+='<tr class="'+(pc?'pc':'')+'" data-a="'+f[0]+'"><td class="bp'+(bp?' on':'')+'"></td><td>'+f[0]+'</td><td class="lab">'+esc(f[1])+
        '</td><td class="hx">'+f[2]+'</td><td>'+esc(f.slice(3).join('|'))+'</td></tr>';
    }
    else if(k==='K'){const p=a.split(' ');stk+='<tr><td>'+p[0]+'</td><td>'+p[1]+'</td></tr>';}
    else if(k==='M'){
      const p=a.split(' '),base=parseInt(p[0],16);let h='',z='';
      for(let i=1;i<p.length;i++){const v=parseInt(p[i],16);h+='<td class="b" data-a="'+hx(base+i-1,4)+'">'+p[i]+'</td>';
        z+=(v&64)?'.':ZX[v&63];}
      mem+='<tr><td>'+p[0]+'</td>'+h+'<td class="zx">'+esc(z)+'</td></tr>';
    }
  }
  if(stop){$('dis').innerHTML=dis;$('stk').innerHTML=stk;$('mem').innerHTML=mem;}   // en marcha: lo de la ultima parada
  bpset=new Set(bps);
  $('bps').innerHTML=(bps.length?bps.map(b=>'<div><span>'+b+'</span><a data-b="'+b+'">&#10005;</a></div>').join(''):'<div>none</div>')+
    (watch?'<div><span>watch '+esc(watch)+'</span><a data-c="w">&#10005;</a></div>':'');
}
function regs(p){
  const n=['PC','SP','AF','BC','DE','HL','IX','IY',"AF'","BC'","DE'","HL'",'I','R'];
  let h='';regv={};
  n.forEach((r,i)=>{regv[r]=p[i];h+='<div data-r="'+r+'"><b>'+r+'</b>'+p[i]+'</div>';});
  h+='<div><b>IFF</b>'+p[14]+'</div><div><b>page</b>'+p[15]+'</div>';
  $('regs').innerHTML=h;
  const f=parseInt(p[2],16)&255;
  $('flags').innerHTML='SZ5H3PNC'.split('').map((c,i)=>'<i class="'+((f>>(7-i))&1?'on':'')+'">'+c+'</i>').join('');
  $('slow').textContent=p[16]?'SLOW':'';
}
function send(c){if(!c)return;if(c[0]!=='@'){if(hist[hist.length-1]!==c)hist.push(c);hi=hist.length;}poll(c);}
document.addEventListener('click',e=>{
  const t=e.target;
  if(t.dataset.c){send(t.dataset.c);return;}
  if(t.dataset.b){send('bc '+t.dataset.b);return;}
  if(t.classList.contains('bp')){const a=t.parentNode.dataset.a;send((bpset.has(a)?'bc ':'b ')+a);return;}
  const r=t.closest('#regs div[data-r]');
  if(r){const n=r.dataset.r,v=prompt(n+' =',regv[n]);if(v)send('x '+n.toLowerCase()+'='+v.trim());return;}
  if(t.classList.contains('b')){const a=t.dataset.a,v=prompt('POKE '+a+' =',t.textContent);if(v)send('e '+a+' '+v.trim());}
});
$('dis').addEventListener('dblclick',e=>{const tr=e.target.closest('tr');if(tr)send('g '+tr.dataset.a);});
$('follow').onchange=e=>send(e.target.checked?'@d':'@d '+hx(dis0,4));
$('dup').onclick=()=>send('@d '+hx((dis0-24)&0xFFFF,4));
$('ddn').onclick=()=>send('@d '+hx(disN&0xFFFF,4));
$('dgo').onclick=()=>{const v=$('dat').value.trim();if(v)send('@d '+v);};
$('dat').onkeydown=e=>{if(e.key==='Enter')$('dgo').onclick();};
$('mup').onclick=()=>send('@m '+hx((mem0-128)&0xFFFF,4));
$('mdn').onclick=()=>send('@m '+hx((mem0+128)&0xFFFF,4));
$('mgo').onclick=()=>{const v=$('mat').value.trim();if(v)send('@m '+v);};
$('mat').onkeydown=e=>{if(e.key==='Enter')$('mgo').onclick();};
$('bpadd').onclick=()=>{const v=$('bpa').value.trim();if(v){send('b '+v);$('bpa').value='';}};
$('bpa').onkeydown=e=>{if(e.key==='Enter')$('bpadd').onclick();};
$('wset').onclick=()=>{const v=$('wa').value.trim();if(v)send('w '+$('wm').value+' '+v);};
$('clr').onclick=()=>{out.textContent='';};
cmd.addEventListener('keydown',e=>{
  if(e.key==='Enter'){send(cmd.value.trim());cmd.value='';}
  else if(e.key==='ArrowUp'){if(hi>0){hi--;cmd.value=hist[hi];}e.preventDefault();}
  else if(e.key==='ArrowDown'){if(hi<hist.length){hi++;cmd.value=hist[hi]||'';}e.preventDefault();}
});

// ---- captura de pantalla: el documento del STM32 (DEBUGGER.cpp, estructura cap) y su dibujo ----
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let capBusy=false;
function capMsg(t){$('capmsg').textContent=t;}
async function capGet(off){
  const r=await fetch('/debug/cap?off='+off);
  if(!r.ok)throw 'no answer';
  const b=new Uint8Array(await r.arrayBuffer());
  return {state:b[0],total:b[1]|b[2]<<8,data:b.subarray(6,6+b[5])};
}
async function capture(){
  if(capBusy)return;capBusy=true;
  try{
    capMsg('stopping the program...');
    const r0=await fetch('/debug/cap?go=1'+($('capspr').checked?'':'&s=0'));
    if(!r0.ok)throw 'no answer';
    const h=new Uint8Array(await r0.arrayBuffer());
    if(h[0]===4){capMsg('no debug monitor (/SYS/DEBUG.BIN)');return;}
    let g=null;
    for(let i=0;i<80;i++){await sleep(150);g=await capGet(0);if(g.state===2||g.state===3)break;}
    if(!g||g.state!==2){capMsg(g&&g.state===3?'the program did not stop':'timed out');return;}
    const doc=new Uint8Array(g.total);doc.set(g.data,0);let off=g.data.length;
    while(off<g.total){
      const c=await capGet(off);
      if(c.state!==2||!c.data.length)throw 'lost';
      doc.set(c.data,off);off+=c.data.length;capMsg('reading '+off+' / '+g.total);
    }
    drawCap(doc);
    capMsg('');
  }catch(e){capMsg('error: '+e);}
  capBusy=false;
}
const MODES=['ZX81 text','Superfast text 32','Superfast text 70','Superfast text 80','Superfast HiRes','Superfast Spectrum','unknown'];
function pal(v){
  if(v&7){const b=(v&8)?255:205;return [(v&2)?b:0,(v&4)?b:0,(v&1)?b:0];}
  return (v&8)?[70,70,70]:[0,0,0];
}
function drawCap(d){
  const mode=d[1],chroma=d[2],fl=d[3],hs=d[5];
  const dfile=d[6]|d[7]<<8,abase=d[8]|d[9]<<8,fbase=d[10]|d[11]<<8,vbase=d[12]|d[13]<<8;
  const col=!!(chroma&0x20),a1=!!(chroma&0x10),s128=!!(fl&1),s256=!!(fl&2),aov=!!(fl&32),hasSpr=!!(fl&8);
  const mem=new Uint8Array(65536);
  let p=24;
  for(let i=0;i<d[14];i++){
    const a=d[p]|d[p+1]<<8,l=d[p+2]|d[p+3]<<8;p+=4;
    for(let k=0;k<l;k++)mem[(a+k)&65535]=d[p+k];
    p+=l;
  }
  $('capinfo').textContent='- '+MODES[mode]+(col?(a1?', colour by position':', colour by character'):', mono')+
    (s256?', 256 chars':s128?', 128 chars':'')+(hasSpr?', sprites':'');
  const cv=$('capcv'),B=16;
  const wide=mode===2||mode===3,ncols=mode===2?70:mode===3?80:32,cw=mode===3?7:8;
  const stride=mode===2?71:mode===3?81:33;
  const W=(mode>=4)?256:ncols*cw,H=192;
  cv.width=W+2*B;cv.height=H+2*B;
  const ctx=cv.getContext('2d');
  const bc=col?pal(chroma&15):[255,255,255];
  ctx.fillStyle='rgb('+bc.join(',')+')';ctx.fillRect(0,0,cv.width,cv.height);
  if(mode===6){
    ctx.fillStyle='#fff';ctx.font='14px sans-serif';ctx.fillText('this video mode is not drawn',B,B+20);return;
  }
  const img=ctx.createImageData(W,H),px=img.data;
  for(let i=0;i<W*H;i++){px[i*4]=bc[0];px[i*4+1]=bc[1];px[i*4+2]=bc[2];px[i*4+3]=255;}   // lo que no se pinta (filas cortas del ZX81): como el borde
  const put=(x,y,c)=>{if(x<0||x>=W)return;const o=(y*W+x)*4;px[o]=c[0];px[o+1]=c[1];px[o+2]=c[2];px[o+3]=255;};
  const BLACK=[0,0,0],WHITE=[255,255,255];
  const attrCell=(r,c,addr)=>{                         // atributo por posicion
    if(mode===0)return mem[(addr|0x8000)&65535];
    const rel=r*stride+c;
    return mem[aov?(abase+rel)&65535:(0x8000|((dfile+1+rel)&0x7FFF))];
  };
  if(mode<=3){
    const fidx=ch=>s256?ch:s128?((ch&0x80?64:0)+(ch&63)):(ch&63);
    const tab=(ch,ln)=>mem[(0xC000+(s256?ch*8:((ch&0x80?512:0)+(ch&63)*8))+ln)&65535];
    const rows=[];
    if(mode===0){
      let q=(dfile+1)&65535;
      for(let r=0;r<24;r++){
        const row=[];
        while(row.length<32){const c=mem[q&65535];if(c===0x76)break;row.push({ch:c,addr:q});q++;}
        if(mem[q&65535]===0x76)q++;
        rows.push(row);
      }
    }else{
      for(let r=0;r<24;r++){
        const row=[];
        for(let c=0;c<ncols+(mode===1?1:0);c++){const a=(dfile+1+r*stride+c)&65535;row.push({ch:mem[a],addr:a});}
        rows.push(row);
      }
    }
    for(let r=0;r<24;r++){
      let sh=0;
      if(mode===1&&hs){const m=r<8?d[16]>>r:r<16?d[17]>>(r-8):d[18]>>(r-16);if(m&1)sh=hs;}
      for(let c=0;c<rows[r].length;c++){
        const {ch,addr}=rows[r][c];
        const blank=mode===0&&!s256&&!s128&&(ch&0x40);
        let a=0;
        if(col)a=a1?attrCell(r,c,addr):0;
        for(let ln=0;ln<8;ln++){
          if(col&&!a1)a=tab(ch,ln);
          const g=blank?0:mem[(fbase+fidx(ch)*8+ln)&65535];
          const inv=(ch&0x80)?1:0;
          const ink=col?pal(a&15):BLACK,paper=col?pal(a>>4):WHITE;
          for(let x=0;x<cw;x++){
            const bit=((g>>(7-x))&1)^inv;
            put(c*cw+x-sh,r*8+ln,bit?ink:paper);
          }
        }
      }
    }
  }else if(mode===4){
    for(let y=0;y<192;y++)for(let xb=0;xb<32;xb++){
      const g=mem[(vbase+y*32+xb)&65535];
      let a=0;
      if(col){const rel=(y>>3)*33+xb;a=a1?mem[aov?(abase+rel)&65535:(0x8000|((dfile+1+rel)&0x7FFF))]:mem[(0xC000+y*32+xb)&65535];}
      const ink=col?pal(a&15):BLACK,paper=col?pal(a>>4):WHITE;
      for(let x=0;x<8;x++)put(xb*8+x,y,((g>>(7-x))&1)?ink:paper);
    }
  }else{
    for(let y=0;y<192;y++)for(let xb=0;xb<32;xb++){
      const g=mem[(vbase+((y&0xC0)<<5)+((y&7)<<8)+((y&0x38)<<2)+xb)&65535];
      const a=mem[(vbase+0x1800+(y>>3)*32+xb)&65535];
      const sp=c3=>pal(c3|((c3&&(a&0x40))?8:0));
      const ink=col?sp(a&7):BLACK,paper=col?sp((a>>3)&7):WHITE;
      for(let x=0;x<8;x++)put(xb*8+x,y,((g>>(7-x))&1)?ink:paper);
    }
  }
  if(hasSpr&&!wide){                                    // los sprites: hasta 12 por linea, gana el de indice mas alto
    const sp=[];
    for(let i=0;i<64;i++){
      const b=i<32?0x0C00+i*32:0x1800+(i-32)*32;
      if(mem[b]&1)sp.push({b:b,x:mem[b+1]|(mem[b+2]&1)<<8,y:mem[b+3]});
    }
    for(let y=0;y<192;y++){
      const list=[];
      for(let k=sp.length-1;k>=0&&list.length<12;k--){const dy=(y+32-sp[k].y)&511;if(dy<8)list.push([sp[k],dy]);}
      if(!list.length)continue;
      for(let x=0;x<256;x++){
        for(const [s,dy] of list){
          const dx=(x+32-s.x)&511;
          if(dx>=8)continue;
          const bit=7-dx;
          if(!((mem[s.b+20+dy]>>bit)&1))continue;
          const on=(mem[s.b+12+dy]>>bit)&1,c=mem[s.b+4+dy];
          put(x,y,col?pal(on?c>>4:c&15):(on?BLACK:WHITE));
          break;
        }
      }
    }
  }
  ctx.putImageData(img,B,B);
}
$('capbtn').onclick=$('capgo').onclick=capture;
setInterval(()=>{if(!document.hidden)poll();},400);
poll();
</script></body></html>
)HTML";
