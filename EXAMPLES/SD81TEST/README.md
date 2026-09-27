# SD81TEST — test de hardware del SD81 Booster

Programa en ensamblador para comprobar de forma sistemática el interface,
sobre todo al probarlo en máquinas nuevas (TS1500, TS1000, clones...). Va
creciendo por fases; de momento solo incluye el test de memoria.

## Ensamblar

```
pasmo sd81test.asm SD81TEST.BIN
```

## Uso

Copia `SD81TEST.BIN` y el stub BASIC (`SD81TEST.B81`, pásalo a `.P` con el
emulador) a la misma carpeta de la SD. El stub carga el binario y muestra
un menú:

```
SD81 BOOSTER HARDWARE TEST

1 MEMORY
2 MC45
3 MC45 BLOCKS 6-7
0 EXIT
```

El menú está en BASIC a propósito: cuando el programa corre en FAST la
pantalla solo se ve mientras el BASIC espera (`PAUSE`/`INPUT`). Cada prueba
tiene su propio punto de entrada en la tabla de saltos del principio del
binario, deja sus resultados en pantalla y devuelve su número de fallos:

| Prueba | Entrada | Devuelve |
|---|---|---|
| Memoria | `USR 24576` | páginas con errores (255 = no se ejecutó, modo 32K) |
| MC45 | `USR 24579` | comprobaciones fallidas (0–10) |
| MC45 bloques 6–7 | `USR 24582` | comprobaciones fallidas (0–8; 255 = no se ejecutó) |

- El programa vive en 24576 ($6000, bloque 3), por encima de RAMTOP.
- Necesita el **modo 48K** (el de defecto). En modo 32K (`LOAD *RAM48
  STOP`) el bloque 6, que el test de memoria usa como ventana, es el espejo
  que lee el vídeo: el test lo detecta y no arranca.
- Para probar las 64 páginas (512K), ejecuta antes `LOAD *FULLPAG`. En
  paginación simple solo se prueban 32.

**El test de memoria es destructivo** para todas las páginas que no son de
sistema: discos RAM de CP/M, pantallas guardadas, etc. Lo normal es
ejecutarlo tras un reset. Los de MC45 solo machacan unos pocos bytes de
los bloques 4 a 7.

## Test de memoria

1. Guarda el mapeo de los 8 bloques (puerto `$E7`) y detecta paginación
   simple (32 páginas) o completa (64).
2. Las páginas mapeadas en los bloques 0–3 (ROM, ROM de expansión, BASIC,
   el propio programa y su pila) son de **sistema** y no se tocan.
3. **Pasada A:** rellena todas las demás páginas, vistas una a una por el
   bloque 6 (`$C000`), con `(H AND 1Fh) XOR L XOR página`, y después las
   verifica todas. Como el patrón depende de la dirección y de la página,
   detecta bits de datos, líneas de dirección dentro de la página y páginas
   que se solapan (escribir en una machaca otra).
4. **Pasada B:** lo mismo con el patrón invertido, para que cada bit se
   pruebe a 0 y a 1.
5. **Enrutado:** vuelve a leer cada página a través de los bloques 4, 5, 6
   y 7 (32 muestras) para comprobar que cada bloque lleva sus accesos a la
   página que indica el mapper.
6. Restaura el mapeo de los bloques 4–7 y deja el resumen en pantalla.

### Pantalla

```
SD81 BOOSTER TEST - MEMORY V0.4
PAGES: 64 (FULL PAGING)
S=SYSTEM .=OK X=FAIL R=ROUTING

00 S S S S . . . .
08 . . . . . . . .
...

VERIFYING                      <- fase en curso

PASS A (DIRECT): OK
PASS B (INVERTED): OK
ROUTING (BLOCKS 4-7): OK

PAGES WITH ERRORS: 00
```

- La página que se está probando aparece en **vídeo inverso** en la rejilla.
- `-`: pendiente. `S`: sistema (no se prueba). `.`: correcta.
- `X`: falla en el relleno/verificación (pasada A o B).
- `R`: pasa las pasadas A y B, pero falla al leerla a través de algún
  bloque del 4 al 7.
- Cada fase muestra `OK` o el número de páginas que han empezado a fallar
  en ella.

Si hay errores, al final se añade el primero encontrado: página, bloque por
el que se leía, dirección, valor esperado y valor leído.

La pantalla se escribe directamente en `D_FILE` (sin `RST 10h`). Como el
programa corre con el vídeo encendido, no toca IX, IY, I ni AF' ni
deshabilita las interrupciones: los usa la ROM para generar la imagen.

## Test de MC45

Escribe `LD BC,0302h / RET` (`01 02 03 C9`) en cinco puntos de los bloques
4 y 5 (`$8000`, `$9C40`, `$9FFC`, `$A000` y `$BFFC`) y lo ejecuta con BC=0:

1. Con **MC45 activo** (comando MCU `$13`) tiene que devolver `0302`: el
   código se ejecuta con normalidad.
2. Con **MC45 apagado** (comando `$14`) tiene que devolver `0000`: `01`,
   `02` y `03` tienen el bit 6 a 0 y A15=1, así que llegan a la CPU como
   NOPs forzados, y solo se ejecuta el `RET` (`C9`, bit 6 a 1).

La primera mitad comprueba que MC45 funciona. La segunda, que la máquina
fuerza de verdad los NOPs por encima de 32K, que es justo lo que MC45 tiene
que anular. Al terminar deja MC45 apagado.

```
SD81 BOOSTER TEST - MC45 V0.4

CODE IN BLOCKS 4-5: LD BC,0302
MC45 ON:  MUST RETURN 0302
MC45 OFF: FORCED NOPS, 0000

ADDR  MC45 ON     MC45 OFF
8000  0302 OK     0000 OK
...

RESULT: OK
```

## Test de MC45 en los bloques 6 y 7

Con MC45 activo y `POKE 2062,170` se puede ejecutar código también en los
bloques 6 y 7, siempre que estén mapeados a páginas propias (modo 48K) y no
al espejo de los bloques 2/3. La prueba escribe `LD BC,0302h / RET` en
cuatro direcciones de esos bloques y lo ejecuta:

1. **MC45 + extensión** (`POKE 2062,170`): tiene que devolver `0302`, el
   código corre desde las páginas de los bloques 6/7.
2. **MC45 sin extensión** (`POKE 2062,85`): tiene que devolver `0000`. En
   modo 48K la FPGA manda las búsquedas de opcode de `$C000-$FFFF` al mismo
   desplazamiento de los bloques 2/3, así que la CPU ejecuta lo que haya
   ahí.

Por eso solo se prueba en direcciones cuyo "espejo" en los bloques 2/3
controla el programa y contiene un `RET`: `$C03C` y `$C040` (su espejo es el
buffer de impresora, `$403C`/`$4040`, que se guarda y se restaura) y dos
direcciones `$E000+x` cuyo espejo `$6000+x` está dentro del propio programa.
Saltar a cualquier otra dirección ejecutaría variables de sistema o el
BASIC y colgaría la máquina.

- Se ejecuta en **FAST**: con la extensión activa el vídeo nativo, que
  ejecuta `D_FILE+$8000`, ya no iría a los bloques 2/3. El programa usa
  las rutinas de la ROM `SET_FAST` (`$02E7`) y `SLOW_FAST` (`$0207`), las
  mismas que el `LOAD` del interface, así que funciona se llame desde FAST
  o desde SLOW.
- Si los bloques 6/7 son espejo de 2/3 (modo 32K o un `MAP` manual) no se
  ejecuta: escribir el código de prueba machacaría el sistema.
- Necesita ROMLOCK apagado: con `LOAD *ROMLOCK` el `POKE 2062` no tiene
  efecto y la primera mitad falla.
- Al terminar deja MC45 y la extensión apagados.

```
SD81 TEST - MC45 BLOCKS 6-7 V0.4

CODE IN BLOCKS 6-7: LD BC,0302
MC45+POKE 2062,170: 0302
MC45+POKE 2062,85: MIRROR 0000

ADDR  EXT ON      EXT OFF
C03C  0302 OK     0000 OK
...

RESULT: OK
```

## Próximas fases

- Puertos mapeados en memoria (POKEs de configuración de la zona de ROM):
  escritura y efecto observable.
- Mapper: lectura/escritura de todos los bloques y los dos formatos.
- Prueba no destructiva de las páginas de sistema.
