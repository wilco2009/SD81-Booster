// Prueba del dibujado de la captura de pantalla de la pagina /debug (DEBUG_PAGE.h).
// Saca el JavaScript de la pagina, lo ejecuta con un lienzo de mentira y le da
// documentos hechos a mano (el formato de la estructura cap de DEBUGGER.cpp);
// despues mira los pixeles que salen.
//   node page_test.js        (desde esta carpeta)
const fs = require('fs');
const path = require('path');
const src = fs.readFileSync(path.join(__dirname, '..', '..', 'Arduino', 'Wifi_module_01', 'DEBUG_PAGE.h'), 'utf8');
const js = src.slice(src.indexOf('<script>') + 8, src.indexOf('</script>'));

// ---- el lienzo y el DOM de mentira
let last = null;                                        // lo ultimo que se pinto: {w, h, data, bx, by}
const els = {};
function el(id) {
  if (!els[id]) {
    els[id] = {
      id, textContent: '', value: '', checked: true, dataset: {}, classList: { toggle() {}, add() {}, remove() {} },
      innerHTML: '', style: {}, width: 0, height: 0, scrollTop: 0, clientHeight: 0, scrollHeight: 0,
      addEventListener() {}, appendChild() {}, querySelectorAll() { return []; }, blur() {}, set onclick(f) {}, set onchange(f) {}, set onkeydown(f) {},
      getContext() {
        const self = this;
        return {
          fillStyle: '', font: '', fillRect() {}, fillText() {},
          createImageData(w, h) { return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }; },
          putImageData(img, x, y) { last = { w: img.width, h: img.height, data: img.data, bx: x, by: y, cw: self.width, ch: self.height }; },
        };
      },
    };
  }
  return els[id];
}
global.document = {
  getElementById: el, addEventListener() {}, hidden: true, querySelectorAll() { return []; },
  createElement() { return { dataset: {}, className: '', innerHTML: '', appendChild() {}, classList: { toggle() {} } }; },
};
global.window = { addEventListener() {} };
global.fetch = () => Promise.reject('sin red');
global.setInterval = () => 0;
global.prompt = () => null;
new Function(js + '\nglobalThis.__drawCap = drawCap; globalThis.__zxKeysFor = zxKeysFor; globalThis.__zxMatrix = zxMatrix;')();
const drawCap = globalThis.__drawCap, zxKeysFor = globalThis.__zxKeysFor, zxMatrix = globalThis.__zxMatrix;

// ---- documentos
function doc({ mode, chroma = 0x20, flags = 0, I = 0x1E, hs = 0, dfile = 0, abase = 0, fbase = 0x1E00, vbase = 0, masks = [0xFF, 0xFF, 0xFF], regions }) {
  const parts = [];
  for (const [a, bytes] of regions) {
    const h = [a & 255, a >> 8, bytes.length & 255, bytes.length >> 8];
    parts.push(...h, ...bytes);
  }
  const head = new Array(24).fill(0);
  head[0] = 1; head[1] = mode; head[2] = chroma; head[3] = flags; head[4] = I; head[5] = hs;
  head[6] = dfile & 255; head[7] = dfile >> 8; head[8] = abase & 255; head[9] = abase >> 8;
  head[10] = fbase & 255; head[11] = fbase >> 8; head[12] = vbase & 255; head[13] = vbase >> 8;
  head[14] = regions.length; head[16] = masks[0]; head[17] = masks[1]; head[18] = masks[2];
  return new Uint8Array([...head, ...parts]);
}
const px = (x, y) => { const o = ((y * last.w) + x) * 4; return [last.data[o], last.data[o + 1], last.data[o + 2]]; };
const eq = (a, b) => a[0] === b[0] && a[1] === b[1] && a[2] === b[2];
const RED = [205, 0, 0], GREEN = [0, 205, 0], BLUE = [0, 0, 205], BLACK = [0, 0, 0], WHITE = [255, 255, 255];
let errors = 0;
function check(ok, what) { if (!ok) { errors++; console.log('ERROR ' + what); } }
const pokes = new Array(61).fill(0);

// 1. Superfast texto de 32 columnas, color por codigo de caracter
{
  const font = new Array(512).fill(0);
  for (let ln = 0; ln < 8; ln++) font[5 * 8 + ln] = ln === 0 ? 0x80 : 0x01;     // el caracter 5: un punto a la izquierda arriba, otro a la derecha
  const dfileData = new Array(1 + 24 * 33).fill(0);
  dfileData[1] = 5; dfileData[2] = 0x85;                                       // fila 0: el 5 y el 5 inverso
  const tab = new Array(1024).fill(0);
  for (let ln = 0; ln < 8; ln++) { tab[5 * 8 + ln] = (1 << 4) | 4; tab[512 + 5 * 8 + ln] = 0x72; }   // tinta verde sobre papel azul
  drawCap(doc({ mode: 1, dfile: 0x5000, fbase: 0x1E00, regions: [[0x1E00, font], [0x5000, dfileData], [0xC000, tab]] }));
  check(last && last.w === 256 && last.h === 192, 'texto: 256x192');
  check(eq(px(0, 0), GREEN) && eq(px(1, 0), BLUE) && eq(px(7, 1), GREEN), 'texto: tinta verde sobre papel azul, con el punto de la izquierda y el de la derecha');
  // el caracter 0x85 es el 5 invertido: sus pixeles son los contrarios (el color de la tabla de ese codigo: 0x72)
  check(eq(px(8, 0), pal7()) && eq(px(9, 0), pal2()), 'texto: el caracter inverso invierte los bits (papel donde la normal tiene tinta)');
}
function pal7() { return [205, 205, 205]; }   // 0x72: tinta 2 (rojo), papel 7 (blanco): bit 0 = papel
function pal2() { return [205, 0, 0]; }

// 2. ZX81 nativo: D_FILE con HALT al final de cada fila; monocromo
{
  const font = new Array(512).fill(0);
  for (let ln = 0; ln < 8; ln++) font[3 * 8 + ln] = 0xFF;                      // el caracter 3: un bloque lleno
  const df = [0x76, 3, 0x76, 0x76, 3, 3, 0x76];                                // fila 0 = "3", fila 1 vacia, fila 2 = "33"
  drawCap(doc({ mode: 0, chroma: 0, dfile: 0x4400, fbase: 0x1E00, regions: [[0x1E00, font], [0x4400, df]] }));
  check(eq(px(0, 0), BLACK) && eq(px(8, 0), WHITE), 'nativo mono: tinta negra donde hay caracter y papel blanco detras de el');
  check(eq(px(0, 16), BLACK) && eq(px(8, 16), BLACK) && eq(px(16, 16), WHITE), 'nativo: la fila 2 son 2 caracteres (cada fila acaba en su HALT)');
  check(eq(px(0, 8), WHITE), 'nativo: la fila 1 esta vacia');
}

// 3. Spectrum: la linea 1 va en el segundo "scanline" del bloque (y&7 << 8); atributo con brillo
{
  const bm = new Array(6912).fill(0);
  bm[0x100] = 0x80;                                                             // y = 1, x = 0
  bm[0x1800] = 0x40 | (1 << 3) | 2;                                             // brillo, papel azul, tinta roja
  drawCap(doc({ mode: 5, vbase: 0x8000, regions: [[0x8000, bm]] }));
  check(eq(px(0, 1), [255, 0, 0]) && eq(px(1, 1), [0, 0, 255]) && eq(px(0, 0), [0, 0, 255]), 'Spectrum: tinta roja brillante sobre papel azul brillante y la linea 1 en su sitio');
}

// 4. HiRes con color por 8 pixeles (tabla en $C000)
{
  const bm = new Array(6144).fill(0);
  bm[32 * 9 + 2] = 0xF0;                                                         // y = 9, byte 2
  const colr = new Array(6144).fill(0);
  colr[32 * 9 + 2] = (2 << 4) | 4;                                                // tinta verde, papel rojo
  drawCap(doc({ mode: 4, vbase: 0x8000, regions: [[0x8000, bm], [0xC000, colr]] }));
  check(eq(px(16, 9), GREEN) && eq(px(20, 9), RED), 'HiRes: color por byte (tinta verde / papel rojo)');
}

// 5. Sprites: uno activo en (X=40, Y=40) y 13 en una misma linea (los 12 de indice mas alto)
{
  const spr = new Array(1024).fill(0), spr2 = new Array(1024).fill(0);
  const set = (i, x, y, ink) => {
    const b = i * 32;
    spr[b] = 1; spr[b + 1] = x & 255; spr[b + 2] = x >> 8; spr[b + 3] = y;
    for (let r = 0; r < 8; r++) { spr[b + 4 + r] = (ink << 4) | 0; spr[b + 12 + r] = 0xFF; spr[b + 20 + r] = 0xFF; }
  };
  set(0, 40, 40, 4);
  drawCap(doc({ mode: 4, chroma: 0x20, flags: 8, vbase: 0x8000, regions: [[0x8000, new Array(6144).fill(0)], [0xC000, new Array(6144).fill(0x70)], [0x0C00, spr], [0x1800, spr2]] }));
  check(eq(px(8, 8), GREEN) && eq(px(15, 15), GREEN) && !eq(px(16, 8), GREEN) && !eq(px(8, 7), GREEN), 'sprite: cuadrado 8x8 en (8,8) (X=40, Y=40 menos 32)');
  for (let k = 0; k < 13; k++) set(k, 40 + 10 * k, 60, 2);                        // 13 sprites en la misma linea (indices 0-12)
  const s3 = spr.slice();
  drawCap(doc({ mode: 4, chroma: 0x20, flags: 8, vbase: 0x8000, regions: [[0x8000, new Array(6144).fill(0)], [0xC000, new Array(6144).fill(0x70)], [0x0C00, s3], [0x1800, spr2]] }));
  check(!eq(px(8, 28), RED) && eq(px(18, 28), RED) && eq(px(8 + 10 * 12, 28), RED),
        'sprites: con 13 en una linea no sale el de indice 0 (el limite es 12) y salen el 1 y el 12');
}

// 6. Un modo que no se dibuja y los modos anchos no pintan sprites
{
  drawCap(doc({ mode: 6, regions: [[2038, pokes]] }));
  check(last !== null, 'modo desconocido: no revienta');
  const spr = new Array(1024).fill(0);
  spr[0] = 1; spr[1] = 40; spr[3] = 40;
  for (let r = 0; r < 8; r++) { spr[4 + r] = 0x40; spr[12 + r] = 0xFF; spr[20 + r] = 0xFF; }
  const df = new Array(1 + 24 * 71).fill(0);
  drawCap(doc({ mode: 2, chroma: 0x20, flags: 8, dfile: 0x6000, regions: [[0x6000, df], [0xC000, new Array(2048).fill(0x70)], [0x1E00, new Array(512).fill(0)], [0x0C00, spr]] }));
  check(last.w === 560 && last.h === 192 && !eq(px(8, 8), GREEN), 'modo de 70 columnas: 560x192 y sin sprites');
}

// 7. El teclado: de la tecla del PC a la matriz del ZX81 (fila = A(8+i), columna = D(j), como en la ROM)
{
  const m = (...ks) => zxMatrix([ks]).map(b => b.toString(2).padStart(5, '0')).join(' ');
  const row = (i, bits) => { const a = new Array(8).fill(0); a[i] = bits; return a; };
  const eqm = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
  check(eqm(zxMatrix([zxKeysFor('a')]), row(1, 1)), 'teclado: A = fila 1, columna 0');
  check(eqm(zxMatrix([zxKeysFor('A')]), row(1, 1)), 'teclado: A mayuscula igual');
  check(eqm(zxMatrix([zxKeysFor('z')]), row(0, 2)), 'teclado: Z = fila 0, columna 1');
  check(eqm(zxMatrix([zxKeysFor('b')]), row(7, 16)), 'teclado: B = fila 7, columna 4');
  check(eqm(zxMatrix([zxKeysFor('0')]), row(4, 1)) && eqm(zxMatrix([zxKeysFor('6')]), row(4, 16)), 'teclado: 0 y 6 en la fila 4');
  check(eqm(zxMatrix([zxKeysFor('Enter')]), row(6, 1)) && eqm(zxMatrix([zxKeysFor(' ')]), row(7, 1)), 'teclado: ENTER y SPACE');
  check(eqm(zxMatrix([zxKeysFor('.')]), row(7, 2)), 'teclado: el punto');
  const q = zxMatrix([zxKeysFor('"')]);                                    // SHIFT + P
  check(q[0] === 1 && q[5] === 1 && q.filter(x => x).length === 2, 'teclado: " = SHIFT + P');
  const par = zxMatrix([zxKeysFor('(')]);                                  // SHIFT + I
  check(par[0] === 1 && par[5] === 4, 'teclado: ( = SHIFT + I');
  const bs = zxMatrix([zxKeysFor('Backspace')]);                           // SHIFT + 0 (RUBOUT)
  check(bs[0] === 1 && bs[4] === 1, 'teclado: Backspace = SHIFT + 0');
  const lf = zxMatrix([zxKeysFor('ArrowLeft')]), up = zxMatrix([zxKeysFor('ArrowUp')]);
  check(lf[0] === 1 && lf[3] === 16 && up[0] === 1 && up[4] === 8, 'teclado: flechas = SHIFT + 5 / SHIFT + 7');
  const brk = zxMatrix([zxKeysFor('Escape')]);
  check(brk[0] === 1 && brk[7] === 1, 'teclado: Escape = BREAK (SHIFT + SPACE)');
  check(zxKeysFor('F5') === null && zxKeysFor('Shift') === null && zxKeysFor('@') === null, 'teclado: lo que no existe en el ZX81 no se manda');
  // dos teclas a la vez (A y S) y la misma fila con dos columnas
  const two = zxMatrix([['A'], ['S']]);
  check(two[1] === 3, 'teclado: A y S a la vez, en la misma fila');
  // SHIFT sumado a una tecla que ya lo lleva no cambia nada
  const sh = zxMatrix([['SHIFT'], ['SHIFT', 'P']]);
  check(sh[0] === 1 && sh[5] === 1, 'teclado: SHIFT repetido no se duplica');
}

console.log(errors ? errors + ' ERRORES' : 'TODO OK');
process.exit(errors ? 1 : 0);
