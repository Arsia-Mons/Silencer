#!/usr/bin/env bash
# SIL-18: the screenshot op captures the composited frame. On the cppx path the
# UI is a premultiplied-RGBA layer the GPU composites at present; headless has no
# swapchain, so CaptureCompositedFrame composites the cppx layer over the world
# frame on the CPU — proving headless screenshots show the real UI. The Options
# screenshot returns once that cluster lands (SIL-19).
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" wait_frames --n 3
cli --port "$PORT" screenshot --out "$OUT_DIR/main.png"
test -s "$OUT_DIR/main.png"

# Valid PNG (magic) and a non-blank composite (the menu chrome shows through the
# headless CPU composite, not just a black world frame).
head -c 8 "$OUT_DIR/main.png" | xxd | head -1 | grep -q '8950 4e47'
bun -e '
import { inflateSync } from "node:zlib";
import { readFileSync } from "node:fs";
const bytes = new Uint8Array(readFileSync(process.argv[1]));
const u32 = (o) => ((bytes[o]<<24)|(bytes[o+1]<<16)|(bytes[o+2]<<8)|bytes[o+3])>>>0;
let w=0,h=0,bd=0,ct=0; const idat=[];
for (let o=8;o<bytes.length;){ const len=u32(o); const t=String.fromCharCode(...bytes.slice(o+4,o+8));
  if(t==="IHDR"){w=u32(o+8);h=u32(o+12);bd=bytes[o+16];ct=bytes[o+17];} else if(t==="IDAT") idat.push(bytes.slice(o+8,o+8+len)); o+=len+12; }
const bpp=ct===2?3:ct===6?4:0; if(bd!==8||!bpp) throw new Error("unsupported PNG");
const raw=inflateSync(Buffer.concat(idat.map((c)=>Buffer.from(c))));
const stride=w*bpp; const cur=new Uint8Array(stride),prev=new Uint8Array(stride); let p=0,nonBlack=0;
const paeth=(a,b,c)=>{const q=a+b-c,pa=Math.abs(q-a),pb=Math.abs(q-b),pc=Math.abs(q-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;};
for(let y=0;y<h;y++){const f=raw[p++]; for(let x=0;x<stride;x++){const v=raw[p++]; const left=x>=bpp?cur[x-bpp]:0,up=prev[x],ul=x>=bpp?prev[x-bpp]:0;
  cur[x]=f===0?v:f===1?(v+left)&255:f===2?(v+up)&255:f===3?(v+Math.floor((left+up)/2))&255:(v+paeth(left,up,ul))&255;}
  for(let x=0;x<w;x++){const o=x*bpp; if(cur[o]>40||cur[o+1]>40||cur[o+2]>40) nonBlack++;} prev.set(cur);}
if(nonBlack<2000){console.error(`headless composite looks blank: nonBlack=${nonBlack}`);process.exit(1);}
' "$OUT_DIR/main.png"

echo "PASS 20_screenshot ($OUT_DIR)"
