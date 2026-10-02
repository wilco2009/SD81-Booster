#ifndef DEBUGGER_H
#define DEBUGGER_H
// Depurador por hardware, lado del MCU (fase 2). Ver
// claude/planning/hw_debugger_plan.md, z80rom/debugmon.asm (el monitor que
// corre en el Z80) y FPGA/SD81V2.1000/sim_int.v.
#include <stdint.h>

#define CMD_DBG_BREAK 73    // monitor -> MCU: el programa se ha parado (registros)
#define CMD_DBG_POLL  74    // monitor -> MCU: resultado anterior; MCU -> monitor: peticion

void cmd_dbg_break(void);
void cmd_dbg_poll(void);

// Consola: si la linea es una orden del depurador (empieza por minuscula),
// la ejecuta y devuelve true
bool dbg_console(const char* line);
// Boton QuickSilva: < 1 s cambia QuickSilva; 1 s pausa o continua; 3 s
// snapshot (fase 2b). Se llama en cada vuelta del loop.
void dbg_qs_button(void);
// Pausa desde la consola (DBG_PAUSE, "p")
void dbg_pause(void);
// El Z80 se ha reseteado
void dbg_reset(void);
bool dbg_is_stopped(void);
// Parado, con el monitor esperando y sin peticiones pendientes: listo para
// la siguiente orden
bool dbg_waiting(void);
// LOAD ha cargado este fichero (para el nombre automatico de los snapshots)
void dbg_note_loaded(const char* name);
// OPENDIR ha abierto un listado con este argumento (el explorador lo
// necesita para GETROW: es estado del MCU, va al snapshot como DIR_OPEN)
void dbg_note_opendir(const char* arg, bool ok);
// LOAD *Z81 con el monitor (comando 75): lee y valida el fichero. 0 = la ROM
// lanza la trampa y el MCU lo carga al parar; 0xFF = sin monitor (la ROM usa
// el cargador de siempre); 1 sin fichero, 2 sin [MEMORY], 3 mal hecho. En
// im, el modo de interrupcion que tiene que poner la ROM.
uint8_t dbg_z81_prepare(const char* path, uint8_t* im);
// De COMMANDS.cpp: abre el listado de OPENDIR (0 = bien)
uint8_t opendir_list(const char* arg);
// Snapshot (.Z81): all = todas las paginas (si no, las mapeadas); name vacio
// = nombre automatico; resume = continuar al acabar (si estaba en marcha)
void dbg_snapshot(bool all, const char* name, bool resume);

#endif
