#ifndef ISR80H_H
#define ISR80H_H

void isr80h_register_commands();

enum SystemCommands
{
    SYSTEM_COMMAND_SUM,
    SYSTEM_COMMAND_PRINT,
    SYSTEM_COMMAND_GETKEY,
    SYSTEM_COMMAND_PUTCHAR,
    SYSTEM_COMMAND_MALLOC,
    SYSTEM_COMMAND_FREE,
};

#endif