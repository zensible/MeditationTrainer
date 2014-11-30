#include <string.h>
#include <stdlib.h>
#include "nsutil.h"
#include "nsnet.h"
#include "cmdhandler.h"
#include "nsd.h"

struct CommandEntry {
	char cmd[MAXLEN];
	void (*func)(int cliInd);
};

struct ClientState {
	int newLineStarted;
};

struct ClientState cs[MAXCLIENTS];

struct CommandEntry commands[COMMANDMAX];
int commandCount = 0;

static const char *curline;

void newClientStarted(int cliIndex)
{
}

/* Pass in command string and function pointer */
void enregisterCommand(const char *cmd, void (*func)(int))
{
	int myIndex = commandCount;
	commandCount += 1;
	strcpy(commands[myIndex].cmd, cmd);
	commands[myIndex].func = func;
}

void handleLine(const char *line, int cliIndex)
{
	int i;
	char cmd[MAXLEN+1];
	strcpy(cmd, line);
	strtok(cmd, " \r\n");
	curline = line;
//	rprintf("Handling line: <%s> (cmd:(%s)) from %d\n", line, cmd, cliIndex);
	for (i = 0; i < commandCount; ++i) {
		if (strcmp(commands[i].cmd, cmd) == 0) {
//			rprintf("Executing command for %d: %s\n", cliIndex, cmd);
			commands[i].func(cliIndex);
			break;
		}
	}
}

int fetchParameters(int *vals, int num)
{
	int gotCmd = 0;
	int curParam = 0;
	char *tok;
	char line[MAXLEN+1];
	strcpy(line, curline);
	for (tok = strtok(line, "\r\n "); tok && curParam < num; tok = strtok(NULL, "\r\n ")) {
//		rprintf("token: %d: <%s>\n", curParam, tok);
		if (!gotCmd)
			gotCmd = 1;
		else {
			vals[curParam++] = atoi(tok);
		}
	}
	return 0;
}

