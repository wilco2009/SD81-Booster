# Depurador por hardware: prueba de la fase 1

Prueba del motor de ruptura de la FPGA (`FPGA/SD81V2.1000/sim_int.v`, rev
0.06) y de la carga del monitor desde el MCU, sin el protocolo con el MCU
(eso es la fase 2). El plan completo está en
`claude/planning/hw_debugger_plan.md`.

## Ficheros

| Fichero | Qué es |
|---|---|
| `dbgmon.asm` → `debug.bin` | Monitor de juguete. Va en la SD como **`/SYS/DEBUG.BIN`** |
| `dbgtest.asm` → `dbgtest.bin` | Programa de prueba (en 24576) |
| `DBGTEST.B81` | Stub BASIC |

```
pasmo dbgmon.asm debug.bin
pasmo dbgtest.asm dbgtest.bin
```

## Cómo se prueba

1. Sintetizar la FPGA con `sim_int.v` 0.06 y grabar el firmware del MCU con
   la carga del monitor.
2. Copiar `debug.bin` a la SD como `/SYS/DEBUG.BIN`, y `dbgtest.bin` y
   `DBGTEST.B81` a una carpeta cualquiera.
3. **Apagar y encender:** el MCU escribe el monitor en la página 63 al
   arrancar, justo después de la ROM. En el log debe salir `Debug monitor
   loaded (193 bytes, page 63)`.
4. Para la prueba 8, `LOAD *FULLPAG` (paginación completa). Sin ella la
   prueba 8 se salta.
5. Cargar `DBGTEST.B81`.

Resultado esperado:

```
BREAKS IN LAST TEST   251
INTERRUPTIONS (TEST 7) n

OK
```

`n` (las interrupciones que entran durante los 250 pasos de la prueba 7)
depende de la velocidad; tiene que ser al menos 1. Sin `LOAD *FULLPAG` sale
además `TEST 8 SKIPPED (LOAD *FULLPAG)`. Si una prueba falla,
sale `TEST n WRONG` y las rupturas que se apuntaron en ella. `THE FPGA HAS
NO DEBUGGER` es una FPGA sin firma `52h`; `NO DEBUG MONITOR` es que no se
ha cargado `/SYS/DEBUG.BIN` (o no se ha apagado y encendido después de
copiarlo).

## Qué comprueba

El monitor de juguete apunta en la RAM del programa la dirección y el
estado del depurador (puerto `$3FEF`, índice 0: armado, NMI, nivel,
motivo) de cada ruptura. Si el motivo es un `FF` de la memoria en la
dirección que le dice el buzón, repone el byte original, y si el buzón pide
pasos, programa N = 1. La prueba compara lo apuntado con lo esperado:

1. **Trampa** (`OUT $10` a `$3FEF`): rompe en la instrucción siguiente.
2. **Paso a paso:** trampa y 4 pasos por instrucciones con prefijos CB, ED
   y DD: 5 rupturas, una por instrucción.
3. **Comparador de ejecución:** dos llamadas a la misma rutina, dos
   rupturas, y la rutina se ejecuta las dos veces (al continuar no vuelve a
   romper en la misma instrucción).
4. **Punto de vigilancia de escritura:** rompe detrás del `LD (nn),A`.
5. **Breakpoint por software:** `FF` en una instrucción; el monitor repone
   el byte y la instrucción se ejecuta.
6. **Anidado:** breakpoint por software dentro de la rutina de
   interrupción simulada (Superfast y `POKE 2040,1`), despertando de un
   `HALT`. Rompe con nivel = rutina, y las interrupciones siguen.
7. **El paso se queda en su nivel:** 250 pasos por un bucle con las
   interrupciones simuladas activas. Ninguna ruptura dentro de la rutina,
   y alguna interrupción por medio.
8. **La página 63 está protegida:** con la paginación completa, la mapea
   en el bloque 5, lee el `JP` del monitor (`$C3`), intenta escribir encima
   y tiene que seguir ahí. Al acabar repone la página del bloque 5.

## Desde la consola USB del MCU

- `DBG_PAUSE`: pausa. Con el programa en SLOW espera a que pase a FAST.
  Con el monitor de juguete no hace nada visible: fuera de la prueba el
  buzón no es válido y el monitor vuelve sin tocar nada.
- `DBG_RELOAD`: vuelve a escribir `/SYS/DEBUG.BIN` en la página 63. Es como
  un reset.

Las pruebas `EXAMPLES/SIMINT` (`inttest`, `halttest`) y la prueba 5 de
`SD81TEST` también tienen que seguir saliendo bien con esta FPGA: aceptan
la firma `51h` o `52h`.
