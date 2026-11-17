// Optional artwork authoring only: npm install, then npm run preview.
// PIP_ART_MODULES can point to a directory containing @napi-rs/canvas.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
function dependency(name) {
  try { return require(name); } catch (e) {
    if (name === 'ag-psd' && fs.existsSync(path.join(root, '.tools/ag-psd.cjs')))
      return require(path.join(root, '.tools/ag-psd.cjs'));
    if (process.env.PIP_ART_MODULES) return require(path.join(process.env.PIP_ART_MODULES, name));
    throw e;
  }
}
const {createCanvas, loadImage, ImageData} = dependency('@napi-rs/canvas');
const psd = dependency('ag-psd');
psd.initializeCanvas(createCanvas, undefined, (w,h) => new ImageData(w,h));
const W=1276,H=716;
const composite=createCanvas(W,H), cx=composite.getContext('2d');
const children=[];
function layer(name, draw, extra={}) {
  const c=createCanvas(W,H),ctx=c.getContext('2d');draw(ctx);
  children.push({name,canvas:c,...extra});cx.drawImage(c,0,0);
}
function box(name,x,y,w,h,color) {
  layer(name,c=>{c.fillStyle=color;c.fillRect(x,y,w,h);});
}
function text(name,value,x,y,size,color,weight='normal') {
  const fontName=weight==='bold'?'Arial-BoldMT':'ArialMT';
  const rgb=color.match(/[0-9a-f]{2}/gi).map(n=>parseInt(n,16));
  layer(name,c=>{c.fillStyle=color;c.font=`${weight} ${size}px Arial`;c.fillText(value,x,y);},{
    text:{text:value,transform:[1,0,0,1,x,y],style:{font:{name:fontName},fontSize:size,fillColor:{r:rgb[0],g:rgb[1],b:rgb[2]}}},
  });
}
(async()=>{
  const texture=await loadImage(path.join(root,'art/textured-gradient.png'));
  layer('Mineral texture - graphite to emerald gradient',c=>c.drawImage(texture,0,0,W,H));
  const civilian=await loadImage(path.join(root,'art/running-civilian.png'));
  const figureHeight=584,figureWidth=civilian.width*figureHeight/civilian.height;
  layer('White 3D civilian - running and looking back - alpha cutout',c=>c.drawImage(civilian,780,67,figureWidth,figureHeight));
  text('Series label','CIVILIAN QUALITY OF LIFE',76,49,18,'#8db6b9');
  text('P - People initial','P',70,214,160,'#73e5d4');
  text('People remainder','EOPLE',171,214,70,'#ffffff');
  text('I - Improvement initial','I',70,410,160,'#73e5d4');
  text('Improvement remainder','MPROVEMENT',117,410,56,'#ffffff');
  text('P - Project initial','P',70,606,160,'#73e5d4');
  text('Project remainder','ROJECT',171,606,70,'#ffffff');
  box('Development badge',874,24,350,34,'#e3b45e');
  text('Status badge','DEVELOPMENT PROTOTYPE',892,48,18,'#14292e','bold');
  text('Author credit','BY GEORODIN',76,698,24,'#ffffff');
  text('First feature','FIRST FEATURE: LAYERED EVACUATION',762,696,18,'#c4e6df');
  fs.writeFileSync(path.join(root,'art/preview.png'),composite.toBuffer('image/png'));
  fs.writeFileSync(path.join(root,'Photoshop/preview.psd'),psd.writePsdBuffer({width:W,height:H,canvas:composite,children},{trimImageData:true,generateThumbnail:true}));
  const check=psd.readPsd(fs.readFileSync(path.join(root,'Photoshop/preview.psd')),{skipLayerImageData:true,skipCompositeImageData:true,skipThumbnail:true});
  const editable=check.children.filter(l=>l.text).length;
  if(check.width!==W||check.height!==H||check.children.length!==13||editable!==10) throw new Error('PSD layer validation failed');
  console.log(`Created ${W}x${H} PSD with ${check.children.length} layers and ${editable} editable text layers.`);
})().catch(e=>{console.error(e);process.exitCode=1;});
