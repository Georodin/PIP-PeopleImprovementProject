// Read-only PE/Odin RTTI inspection for the installed dev-2026-10 format.
const fs=require('node:fs');
const crypto=require('node:crypto');
const input=process.argv[2];
if(!input)throw new Error('Usage: node inspect-odin.cjs <game.exe> [output.json]');
const b=fs.readFileSync(input);
const sha256=crypto.createHash('sha256').update(b).digest('hex');
if(sha256!=='e297bc7db1174eb67116b2b8387b8640554e1bd46598d8d3178091d9df19b9a9')
 throw new Error('Unsupported executable fingerprint. Inspect the compiler/type format before adding a new build.');
const pe=b.readUInt32LE(0x3c),opt=pe+24;
if(b.toString('ascii',pe,pe+4)!=='PE\0\0'||b.readUInt16LE(opt)!==0x20b)throw new Error('Expected PE32+');
const imageBase=b.readBigUInt64LE(opt+24),sectionOffset=opt+b.readUInt16LE(pe+20),sections=[];
for(let i=0;i<b.readUInt16LE(pe+6);i++){
 const p=sectionOffset+i*40;
 sections.push({name:b.toString('ascii',p,p+8).replace(/\0/g,''),rva:b.readUInt32LE(p+12),size:b.readUInt32LE(p+16),raw:b.readUInt32LE(p+20)});
}
function raw(va,size=1){
 const rva=Number(va-imageBase),s=sections.find(s=>rva>=s.rva&&rva+size<=s.rva+s.size);
 if(!s)throw new Error('Address not backed by file data');return s.raw+rva-s.rva;
}
function u(va){return b.readBigUInt64LE(raw(va,8));}
function str(ptr,len){
 const n=Number(len);if(n<0||n>1000)throw new Error('Invalid string length');
 if(n===0)return '';const p=raw(ptr,n);return b.toString('utf8',p,p+n);
}
function stringAt(va){return str(u(va),u(va+8n));}
const ti=sections.find(s=>s.name==='.odinti');if(!ti)throw new Error('No .odinti section');
const named=[];
for(let p=ti.raw;p+112<=ti.raw+ti.size;p+=8){
 try{
  if(b.readBigUInt64LE(p+104)!==1n)continue;
  const size=Number(b.readBigUInt64LE(p)),align=Number(b.readBigUInt64LE(p+8));
  if(size<0||size>1e8||align<1||align>4096||(align&(align-1)))continue;
  const name=str(b.readBigUInt64LE(p+32),b.readBigUInt64LE(p+40));
  if(!/^[A-Za-z_][A-Za-z0-9_]*(?:\([^\r\n]*\))?$/.test(name))continue;
  const base=b.readBigUInt64LE(p+48),baseRaw=raw(base,112),kind=Number(b.readBigUInt64LE(baseRaw+104));
  const pkg=str(b.readBigUInt64LE(p+56),b.readBigUInt64LE(p+64));
  const record={name,pkg,size,align,rva:ti.rva+p-ti.raw,baseKind:kind,fields:[]};
  if(kind===19){
   const types=b.readBigUInt64LE(baseRaw+32),names=b.readBigUInt64LE(baseRaw+40),offsets=b.readBigUInt64LE(baseRaw+48);
   const count=b.readUInt32LE(baseRaw+72);if(count>10000)throw new Error('Implausible field count');
   for(let i=0;i<count;i++){
    const type=u(types+BigInt(i*8)),tr=raw(type,112);
    record.fields.push({name:stringAt(names+BigInt(i*16)),offset:Number(u(offsets+BigInt(i*8))),size:Number(b.readBigUInt64LE(tr)),kind:Number(b.readBigUInt64LE(tr+104)),typeRva:Number(type-imageBase)});
   }
  }
  named.push(record);
 }catch{}
}
const byRva=new Map(named.map(t=>[t.rva,t]));
for(const t of named)for(const f of t.fields)if(byRva.has(f.typeRva))f.type=byRva.get(f.typeRva).name;
const result={sha256,timestamp:b.readUInt32LE(pe+8),imageSize:b.readUInt32LE(opt+56),typeFormat:'Odin dev-2026-10 inline Type_Info (112 bytes)',namedTypes:named};
if(process.argv[3])fs.writeFileSync(process.argv[3],JSON.stringify(result,null,2));
const selected=['Gate','Safe_Cells','Home_Field','Home_Buf','Evac_Field','Barricade'];
console.log(JSON.stringify({sha256,namedTypes:named.length,relevant:named.filter(t=>t.pkg==='sim'&&selected.includes(t.name)).map(t=>({name:t.name,size:t.size,rva:'0x'+t.rva.toString(16),fields:t.fields.map(f=>f.name+'@'+f.offset)}))},null,2));
