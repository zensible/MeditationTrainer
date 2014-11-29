#ifndef __TESTMODEEGDRIVER_H
#define __TESTMODEEGDRIVER_

void fetchLine(int commandIndex, int cliIndex);
void eatCharacter(char c, int cliIndex);
void resetLine(int cliIndex);
const char *getLineBuf(void);

#endif

