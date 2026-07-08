# DBUF — Demo del doble buffer (POKE 2057)

Demo del **doble buffer "present-blit"** del SD81 Booster: una pelota rebota en
modo Superfast HiRes Spectrum mientras el programa borra y redibuja en cada
frame, con ~6 ms de "trabajo pesado" intercalado a propósito para que el haz
cruce la zona borrada.

| Tecla | Acción |
|-------|--------|
| SPACE | Conmutar doble buffer — borde **verde** = ON, borde **rojo** = OFF |
| M     | Congelar / reanudar el movimiento de la pelota |
| Q     | Salir a BASIC |

**Qué observar:** con el doble buffer activo la pelota se ve sólida y estable;
al desactivarlo (SPACE) aparece un parpadeo intenso, porque el barrido muestra
la pantalla en mitad del ciclo borrado→redibujado.

## Cómo funciona el doble buffer

- `POKE 2057,168+B` lo activa con el **front buffer** en el bloque lógico `B`
  (0-7); `POKE 2057,85` lo desactiva.
- Con dbuf activo, el vídeo deja de leer la página HFILE y lee un front buffer
  privado del hardware (bloque `B` de la memoria interna de vídeo, invisible
  para la CPU). En cada blanking vertical el interface copia automáticamente
  la página HFILE al front buffer.
- Resultado: la pantalla muestra siempre la **instantánea completa** del último
  VSYNC. El programa dibuja sobre una única superficie (HFILE), sin alternar
  páginas ni redibujar frames antiguos.
- **Disciplina de uso:** espera el flanco de subida del VSYNC (bit 0 del puerto
  $AF) y haz todo el borrado/dibujado a continuación; dispones de ~16 ms hasta
  la siguiente instantánea.
- Elección de `B`: usa **4 o 5** (el que no sea tu HFILE). Evita 0-3 y 6-7
  (glyphs ROM, chr RAM, DFILE, atributos Chroma81).

Documentación completa: Manual, **Apéndice F → "Doble buffer (present-blit)"**.

En esta demo: HFILE = $8000 (bloque 4), front = bloque 5 → `POKE 2057,173`.

## Ensamblar y ejecutar

Con [zmac](http://48k.ca/zmac.html):

```
zmac bounce.asm
```

El binario queda en `zout/bounce.cim`; renómbralo a `BOUNCE.BIN` y cópialo a la
SD (o a la carpeta de la SD virtual del emulador EightyOne). Después, desde
BASIC:

```basic
10 FAST
20 LOAD FAST 'BOUNCE.BIN' CODE 30000
30 RAND USR 30000
40 SLOW
```

Requisitos: core FPGA con soporte de POKE 2057 (o el emulador
EightyOne-CrossPlatform con emulación SD81 Booster actualizada).
