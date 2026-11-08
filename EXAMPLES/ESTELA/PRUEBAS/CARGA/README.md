# CARGA — fase 0 (a) de ESTELA

Comprueba que un código cargado en **`$4800`** (18432) con `LOAD THEN CLEAR 18431`
desde un `.B81` arranca, y que los **14 KB de `$4800` a `$7FFF`** se cargan bien y se
pueden ejecutar hasta el final. Es la memoria que tendrá el código del juego.

`CARGA.BIN` mide exactamente 14.336 bytes (`$4800-$7FFF`):

- el programa, al principio;
- un patrón conocido hasta `$7FEF` (el byte de la dirección `a` vale
  `(a mod 256) XOR (a / 256) XOR $A5`);
- una rutina en `$7FF0`, en los últimos 16 bytes, que el programa llama.

El programa comprueba el patrón byte a byte, llama a la rutina del final, mira
`RAMTOP` y la pila, y vuelve a BASIC.

## Cómo se prueba

Copia `CARGA.BIN` y `CARGA.B81` a una carpeta de la SD y carga `CARGA.B81`
(`LOAD "CARGA.B81"` y `RUN`).

## Qué tiene que verse

Tras un momento con la pantalla en blanco (está en FAST), sale:

```
ESTELA - PRUEBA A: CARGA

RESULTADO: 0
ERRORES DEL PATRON: 0
RAMTOP: 18432
LIBRE DEBAJO DE 18432: (un numero)
BIEN
```

`RESULTADO` es la suma de: 1 = el patrón tiene errores, 2 = la rutina de `$7FF0`
no ha devuelto lo que tocaba, 4 = `RAMTOP` no es 18432, 8 = la pila no está por
debajo de `$4800`. `LIBRE DEBAJO DE 18432` es lo que queda entre el área de trabajo
del BASIC (`E_LINE`) y el código: el margen del cargador.

**Qué hay que mandar:** las cuatro líneas de números (o el informe de error, si
el BASIC se para).

## Comprobado en el PC

`claude/mqtttest/build_estela0.sh`: con el Z80 de EightyOne devuelve 0, y 1, 2, 4
y 8 con un byte del patrón cambiado, la rutina del final cambiada, `RAMTOP` a
18433 y la pila por encima de `$4800`. **Sin probar en la placa.**
