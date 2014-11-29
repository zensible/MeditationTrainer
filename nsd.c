#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <openedf.h>
#include <nsnet.h>
#include <nsutil.h>
#include <config.h>
#include "cmdhandler.h"
#include "nsd.h"

#define MAXEDFCHANNELS 64

#define TIMEOUT 30

enum RoleCode { Unknown,  EEG, Display, Controller };

struct EDFDecodedConfig current;

static char *lineBuf;

struct Client {
	sock_t fd;
	enum RoleCode role;
	int markedForDeletion;
	int headerLen;
	time_t lastAlive;
	int isDisplaying[MAXCLIENTS];;
	int linePos;
	struct InputBuffer ib;
	struct OutputBuffer ob;
	char lineBuf[MAXLEN];
	char headerBuf[MAXHEADERLEN];
};

struct Client clients[MAXCLIENTS];
int clientCount = 0;

const char *stringifyRole(enum RoleCode role);
int countInRole(enum RoleCode role);
void sendClientMsg(int cliInd, const char *msg);
void sendClientMsgNL(int cliInd, const char *msg);

void sendClientMsgNL(int cliInd, const char *msg)
{
	sendClientMsg(cliInd, msg);
	sendClientMsg(cliInd, "\r\n");
}

const char *getLineBuf(void)
{
	return lineBuf;
}

void sendClientMsg(int cliInd, const char *msg)
{
	int retval = writen(clients[cliInd].fd, msg,strlen(msg), &clients[cliInd].ob);
//	printf("Tried to send to %d:%s", cliInd, msg);
//	rprintf("Tried to write %d bytes, got %d retval\n", strlen(msg), retval);
	if (retval < 0) {
		rprintf("Got write error... %d\n", retval);
		clients[cliInd].markedForDeletion = 1;
#if 0
#ifdef __MINGW32__
		int winerr;
		winerr = WSAGetLastError();
		if (winerr == WSAENOTCONN)
			clients[cliInd].markedForDeletion = 1;
#endif
#endif
	}
	if (retval > 0)
		rtime(&clients[cliInd].lastAlive);
	
}

void sendResponseOK(int cliInd)
{
	sendClientMsg(cliInd, "200 OK\r\n");
	/*rtime(&clients[cliInd].lastAlive); */
}

void sendResponseBadRequest(int cliInd)
{
	sendClientMsg(cliInd, "400 BAD REQUEST\r\n");
}

void cmdRole(int cliInd)
{
	rprintf("Handling role with cliInd %d\n", cliInd);
	sendResponseOK(cliInd);
	sendClientMsgNL(cliInd, stringifyRole(clients[cliInd].role));
}

void cmdUnwatch(int cliInd)
{
	if (clients[cliInd].role == Display) {
		int who;
		fetchParameters(&who, 1);
		if (who >= 0 && who < clientCount && clients[who].role == EEG) {
			sendResponseOK(cliInd);
			clients[who].isDisplaying[cliInd] = 0;
			return;
		}
	}
	sendResponseBadRequest(cliInd);
}


void cmdWatch(int cliInd)
{
	if (clients[cliInd].role == Display) {
		int who;
		fetchParameters(&who, 1);
		if (who >= 0 && who < clientCount && clients[who].role == EEG) {
			sendResponseOK(cliInd);
			clients[who].isDisplaying[cliInd] = 1;
			return;
		}
	}
	sendResponseBadRequest(cliInd);
}

void cmdDataFrame(int cliInd)
{
	int vals[3];
	int samples[MAXEDFCHANNELS+2];
	/* vals[0] == pktCounter */
	/* vals[1] == channelCount */
	fetchParameters(vals, 2);
	if (vals[1] > 0 && vals[1] < MAXEDFCHANNELS) {
		int i;
		char buf[MAXLEN];
		int bufPos = 0;
		// rprintf("Fetching %d parameters\n", vals[1] + 2);
		fetchParameters(samples, vals[1] + 2);
		sendResponseOK(cliInd);
		bufPos += sprintf(buf+bufPos, "! 0");
		for (i = 0; i < vals[1] + 2; i += 1) {
			bufPos += sprintf(buf+bufPos, " %d", samples[i]);
		}
		bufPos += sprintf(buf+bufPos, "\r\n");
		if (vals[0] == 0) {
//			rprintf("Sending packet <%s>\n", buf); 
			fflush(stdout);
		}
		for (i = 0; i < clientCount; ++i) {
			if (clients[cliInd].isDisplaying[i]) {
				sendClientMsg(i, buf);
			}
		}
	}
	else
		sendResponseBadRequest(cliInd);
}

void cmdGetHeader(int cliInd)
{
	if (clients[cliInd].role == Display) {
		int who;
		fetchParameters(&who, 1);
		if (who >= 0 && who < clientCount && clients[who].role == EEG && clients[who].headerLen) {
			sendResponseOK(cliInd);
			sendClientMsgNL(cliInd, clients[who].headerBuf);
			return;
		}
	}
	sendResponseBadRequest(cliInd);
}

void cmdSetHeader(int cliInd)
{
	const char *buf;
	const char *hdr;
	struct EDFDecodedConfig tmp;
	if (clients[cliInd].role != EEG) {
		sendResponseBadRequest(cliInd);
		return;
	}
	buf = getLineBuf();
	hdr = buf + 10; // strlen("setheader ");
	rprintf("Got header:\n");
	rprintf("The header is <%s>\n", hdr);
	readEDFString(&tmp, hdr, strlen(buf));
	makeREDFConfig(&current, &tmp);
	clients[cliInd].headerLen = sizeof(clients[0].headerBuf);
	writeEDFString(&current, clients[cliInd].headerBuf, &clients[cliInd].headerLen);
	if (memcmp(clients[cliInd].headerBuf, hdr, clients[cliInd].headerLen) != 0) {
		printf("Warning: changed header from <%s> to <%s>\n", hdr, clients[cliInd].headerBuf);
	}
	sendResponseOK(cliInd);
}

void cmdStatus(int cliInd)
{
	char buf[16384];
	int bufPos = 0, i;
	bufPos += sprintf(buf+bufPos, "%d clients connected\r\n", clientCount);
	for (i = 0; i < clientCount; ++i)
		bufPos += sprintf(buf+bufPos, "%d:%s\r\n", i, stringifyRole(clients[i].role));
	sendResponseOK(cliInd);
	sendClientMsg(cliInd, buf);
}

void cmdClose(int cliInd)
{
	sendResponseOK(cliInd);
	clients[cliInd].markedForDeletion = 1;
}

void cmdDisplay(int cliInd)
{
	if (clients[cliInd].role == Unknown) {
		clients[cliInd].role = Display;
		sendResponseOK(cliInd);
		return;
	}
	sendResponseBadRequest(cliInd);
}

void cmdEEG(int cliInd)
{
	if (clients[cliInd].role == Unknown) {
		clients[cliInd].role = EEG;
		sendResponseOK(cliInd);
		return;
	}
	sendResponseBadRequest(cliInd);
}

void cmdControl(int cliInd)
{
	if (clients[cliInd].role == Unknown && countInRole(Controller) == 0) {
		clients[cliInd].role = Controller;
		sendResponseOK(cliInd);
		return;
	}
	sendResponseBadRequest(cliInd);
}

void cmdHello(int cliInd)
{
	sendResponseOK(cliInd);
}

int countInRole(enum RoleCode role)
{
	int i, acc = 0;
	for (i = 0; i < clientCount; ++i)
		if (role == clients[i].role)
			acc += 1;
	return acc;
}

const char *stringifyRole(enum RoleCode role)
{
	switch (role) {
		case Unknown:
			return "Unknown";
			break;
		case Display:
			return "Display";
			break;
		case Controller:
			return "Controller";
			break;
		case EEG:
			return "EEG";
			break;
		default:
			return "<error, unknown role>";
			break;
	}
}

int makeNewClient(sock_t fd) {
	int myIndex = clientCount;
	clientCount += 1;
	memset(&clients[myIndex], 0, sizeof(clients[0]));
	clients[myIndex].fd = fd;
	clients[myIndex].role = Unknown;
	clients[myIndex].markedForDeletion = 0;
	clients[myIndex].linePos = 0;
	clients[myIndex].headerLen = 0;
	initInputBuffer(&clients[myIndex].ib);
	initOutputBuffer(&clients[myIndex].ob);
	newClientStarted(myIndex);
	rtime(&clients[myIndex].lastAlive);
	return myIndex;
}

int main()
{
	int i;
	sock_t mod_fd;
	int retval;
	sock_t sock_fd;
	fd_set toread, toerr;
	rprintf("NSD (NeuroServer Daemon) v. %s-%s\n", VERSION, OSTYPESTR);
	enregisterCommand("hello", cmdHello);
	enregisterCommand("control", cmdControl);
	enregisterCommand("display", cmdDisplay);
	enregisterCommand("close", cmdClose);
	enregisterCommand("status", cmdStatus);
	enregisterCommand("role", cmdRole);
	enregisterCommand("eeg", cmdEEG);
	enregisterCommand("setheader", cmdSetHeader);
	enregisterCommand("getheader", cmdGetHeader);
	enregisterCommand("!", cmdDataFrame);
	enregisterCommand("watch", cmdWatch);
	enregisterCommand("unwatch", cmdUnwatch);
	rinitNetworking();
	sock_fd = rsocket();

	updateMaxFd(sock_fd);

	rprintf("Binding network socket at %d\n", DEFAULTPORT); 

	retval = rbindAll(sock_fd);
	if (retval != 0) {
		rprintf("bind error\n");
		rexit(1);
	}
	rprintf("Socket bound.\n");

	rprintf("Please start the modeegdriver.\n");

	for (;;) {
		time_t now;
		rtime(&now);
		for (i = 0; i < clientCount;) {
			if (clients[i].markedForDeletion || now > clients[i].lastAlive + TIMEOUT){
				rprintf("Shutting down client %d\n", i);
				rshutdown(clients[i].fd);
				if (clients[i].role == Display) {  // Remove all watchers
					int j;
					for (j = 0; j < clientCount; j++) {
						if (clients[j].role == EEG)
							clients[j].isDisplaying[i] = 0;
					}
				}
				if (i != clientCount - 1)   // Move clients array down as a chunk
					memmove((char *) (&clients[i]), (char *) (&clients[i+1]), (clientCount - i - 1)*sizeof(clients[0]));
				clientCount -= 1;
			}
			else
				i += 1;
		}
		FD_ZERO(&toread);
		FD_SET(sock_fd, &toread);
		for (i = 0; i < clientCount; ++i) {
			FD_SET(clients[i].fd, &toread);
		}
		toerr = toread;
		retval = rselect(max_fd, &toread, NULL, &toerr);
//		rprintf("Just selected, with retval %d\n", retval);
		if (FD_ISSET(sock_fd, &toread)) {
			int myIndex;
			rprintf("Accepting from %p\n", sock_fd);
			mod_fd = raccept(sock_fd);
			updateMaxFd(mod_fd);
			myIndex = makeNewClient(mod_fd);
			rprintf("Got connection on client %d.\n", myIndex);
			continue;
		}
		for (i = 0; i < clientCount; ++i) {
			if (FD_ISSET(clients[i].fd, &toerr)) {
				rprintf("Got error condition on %d\n", i);
			}
			if (!inputBufferEmpty(&clients[i].ib) || FD_ISSET(clients[i].fd, &toread)) {
				int j;
				do {
//					rprintf("About to read for client %d and linePos %d\n", i, clients[i].linePos);
					j = my_read(clients[i].fd, clients[i].lineBuf+clients[i].linePos, MAXLEN - clients[i].linePos, &clients[i].ib);
					if (isEOF(clients[i].fd, &clients[i].ib)) {
						clients[i].markedForDeletion = 1;
						rprintf("Got EOF from client %d\n", i);
						break;
					}
					if (j <= 0)
						continue;
					if (j > 0) {
						char *curPtr, *nlPtr;
						curPtr = clients[i].lineBuf;
						clients[i].linePos += j;
						clients[i].lineBuf[clients[i].linePos] = '\0';
						while ( (nlPtr = strchr(curPtr, '\n')) ) {
							*nlPtr = '\0';
							lineBuf = curPtr;
							handleLine(curPtr, i);
							curPtr = nlPtr + 1;
						}
						memmove(clients[i].lineBuf, curPtr, strlen(curPtr)+1);
						clients[i].linePos -= curPtr - clients[i].lineBuf;
						rtime(&clients[i].lastAlive);
					}
				} while (!inputBufferEmpty(&clients[i].ib));
			}
		}
	}
	
	return 0;
}

