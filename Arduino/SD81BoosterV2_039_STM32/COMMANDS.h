#ifndef COMMANDS_H
#define COMMANDS_H

#define LAST_COMMAND 65
typedef void (*command_handler)(void);
extern command_handler commands[];

#endif