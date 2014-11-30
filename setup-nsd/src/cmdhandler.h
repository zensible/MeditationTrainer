#ifndef __CMDHANDLER_H
#define __CMDHANDLER_H

#define COMMANDMAX 256

/* Call this when you accept a new client */
void newClientStarted(int cliIndex);

/* Pass in command string and function pointer */
void enregisterCommand(const char *cmd, void (*func)(int));

/* Handles characters using the command handler state machine */
void handleChar(const char c, int cliIndex);
void handleLine(const char *line, int cliIndex);
int fetchParameters(int *vals, int num);

#endif
