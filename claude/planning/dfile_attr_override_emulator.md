# Overrides de pantalla y atributos (Superfast) — para portar al emulador

Resumen técnico de dos mecanismos nuevos en la FPGA, ya commiteados en
`master` y documentados en el manual (ES/EN). **No incluye** ninguna vía
de puerto `$E7`/pseudo-bloque: se implementó una extensión así durante el
desarrollo pero se descartó sin llegar a commitear — el mecanismo real es
únicamente por POKE de memoria, tal como se describe abajo.

Commits de referencia (rama `master`):
- `551ec92` — FPGA: override de `D_FILE`
- `f483cbb` — Manual: override de `D_FILE`
- `844e694` — FPGA: override de atributos (Chroma modo 1)
- `505dbb4` — Manual: override de atributos

## 1. Override de `D_FILE` — POKE 2096/2097/2098

Los formatos de texto Superfast (32/70/80 columnas) toman la dirección de
pantalla de un registro `DFILE` que la FPGA mantiene por **snooping** de
cualquier escritura a la variable de sistema `D_FILE` (16396/16397), sin
condicionar por modo. El override permite sustituir esa dirección por
otra, sin tocar `D_FILE`:

```
POKE 2096, <low>   ; byte bajo de la direccion alternativa
POKE 2097, <high>  ; byte alto
POKE 2098, 170     ; activa el override
POKE 2098, 85      ; lo desactiva, vuelve a D_FILE
```

Registros (nombres del RTL): `DFILE_OVERRIDE` (16 bits), `dfile_ovr_en`
(1 bit). Los tres son registros de memoria, decodificados por dirección
literal (`nMREQ`+`nWR`+`Addr==2096/2097/2098`), **no** por puerto de E/S.

**Compatibilidad**: `dfile_ovr_en` arranca en 0 tras reset. Mientras esté
en 0, el comportamiento es exactamente el de siempre (snoop de `D_FILE`)
— el software existente no nota que este mecanismo existe.

**Apagado automático**: volver a vídeo nativo (`POKE 2045,85`) fuerza
`dfile_ovr_en <= 0`, para que "modo estándar" siga significando siempre
lo mismo, sin depender de si alguien activó el override y olvidó
desactivarlo. Cambiar entre los distintos submodos Superfast (`POKE
2045,170/171/172/173/174`) **no** lo toca — si estaba activo, sigue
estándolo.

**Dirección efectiva**: se define un valor `DFILE_eff`:

```
DFILE_eff = dfile_ovr_en ? DFILE_OVERRIDE : DFILE
```

y las tres fórmulas de dirección que antes usaban `DFILE` a secas
(texto 32 columnas, texto ancho 70/80 vía `char_addr_80`, y Chroma modo 1
por posición) pasan a usar `DFILE_eff`.

## 2. Override de atributos — POKE 2059/2060/2061

Chroma modo 1 (fichero de atributos) colocaba la tabla de color en la
misma dirección que el carácter pero con el bit 15 forzado a 1 (es decir,
a `$8000` de distancia de `DFILE_eff`, con el resultado truncado a 15
bits — un "espejo" de la pantalla, limitado a módulo 32K). Este segundo
override independiza también esa tabla:

```
POKE 2059, <low>   ; byte bajo de ATTR_BASE_OVERRIDE
POKE 2060, <high>  ; byte alto
POKE 2061, 170     ; activa el override
POKE 2061, 85      ; lo desactiva, vuelve al comportamiento de siempre
```

Registros: `ATTR_BASE_OVERRIDE` (16 bits), `attr_ovr_en` (1 bit). Mismo
patrón de decodificación que el anterior (memoria, dirección literal).

**Compatibilidad y apagado**: idénticos al override de `D_FILE` —
`attr_ovr_en` arranca en 0, y `POKE 2045,85` lo apaga también (comparte
el mismo evento de apagado que `dfile_ovr_en`; son dos flags
**independientes** entre sí, pero los dos escuchan el mismo "volver a
nativo").

**Fórmula de dirección**: se factorizó una parte común, `attr_rel_addr`
("posición dentro de la rejilla fila/columna, sin base ni el +1 de
relleno"), que ya era idéntica en las dos ramas (32 columnas y modo
ancho) antes de este cambio:

```
attr_rel_addr = sf80_en ? (row_stride_80 + scr_col_80)
                        : (scr_row*32 + scr_row + scr_col2)

attr_addr_m1 = attr_ovr_en
             ? (ATTR_BASE_OVERRIDE + 1 + attr_rel_addr)          ; 16 bits COMPLETOS
             : (0x8000 | ((DFILE_eff + 1 + attr_rel_addr) & 0x7FFF))  ; formula de siempre
```

Con el override activo, la dirección resultante es de **16 bits
completos**, sin el bit 15 forzado ni el truncado a 15 bits que tenía la
fórmula de siempre — puede vivir en cualquier dirección, no solo en el
"espejo" de la pantalla. Se mantiene el mismo `+1` de relleno inicial que
usa `D_FILE`, para que el software pueda reutilizar la misma aritmética
fila×paso+columna en los dos buffers.

## 3. Qué replicar en el emulador (checklist)

Para cada uno de los dos overrides:

- [ ] Registro de 16 bits (low/high en direcciones consecutivas), inerte
      hasta que se activa.
- [ ] Flag de activación en la tercera dirección: `170`=on, `85`=off,
      cualquier otro valor no lo toca. A 0 tras reset.
- [ ] `POKE 2045,85` (vídeo nativo) fuerza el flag a 0, sea cual sea su
      estado previo.
- [ ] Cambiar entre `POKE 2045,170/171/172/173/174` (los distintos
      submodos Superfast) **no** toca ninguno de los dos flags.
- [ ] Los dos overrides son independientes entre sí — activar uno no
      activa ni desactiva el otro.
- [ ] Con el override de `D_FILE` activo, sustituir `DFILE_eff` en las
      tres fórmulas que antes usaban `DFILE` (texto 32 col, texto ancho,
      Chroma modo 1 por posición).
- [ ] Con el override de atributos activo, la dirección de Chroma modo 1
      es `ATTR_BASE_OVERRIDE + 1 + attr_rel_addr` en 16 bits completos —
      **sin** el `0x8000 |` ni el `& 0x7FFF` de la fórmula por defecto.
- [ ] Chroma modo 0 (por código de carácter, tabla fija en `$C000`) no
      se ve afectado por ninguno de los dos overrides — es independiente
      de `D_FILE`/atributos por posición.

## 4. Validado en hardware real

Ambos overrides, y su combinación con `SEL_256CHARS` y Superfast 80
columnas, se probaron en hardware real con las direcciones exactas que
usa CP/M-SD81 (`$EA00` para pantalla, `$F800`/`I=$F8` para la fuente de
256 caracteres) — ver los ficheros de prueba en `test/` de esta sesión
(`dfile_override_cpmaddr_test.asm`, `sel256chars_test.asm`) si hace falta
reproducir el caso exacto.

Un hallazgo relevante de esas pruebas, no relacionado con estos
registros pero sí con Superfast en general: `SEL_256CHARS` **debe
activarse después** de poner el vídeo en modo Superfast (`POKE
2045,17x`), nunca antes — activarlo en pleno modo nativo descontrola el
generador de vídeo nativo (que ejecuta el DFILE como opcodes vía HALT).
El propio comentario del RTL ya lo advertía: *"1=256 caracteres (SOLO
Superfast texto)"*.
