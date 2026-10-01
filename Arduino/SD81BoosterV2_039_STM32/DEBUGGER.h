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

#endif
