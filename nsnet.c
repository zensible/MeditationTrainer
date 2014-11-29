#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <nsnet.h>
#include <nsutil.h>
#include <monitor.h>

#ifndef __MINGW32__
#include <signal.h>
#endif

#define NUM_WRITEN 1
#define NUM_READN   2
#define NUM_MYREAD   3
#define NUM_READLINE   4
#define NUM_WRITEN2 5

#define NUM_MAX 6

#define NUM_QUIET

unsigned int cur[NUM_MAX], last[NUM_MAX];

void addCount(int where)
{
	assert(where >= 0 && where < NUM_MAX);
	cur[where] += 1;
}

void showCounts()
{
#ifdef NUM_QUIET
	return;
#else
	int i;

	for (i = 0; i < NUM_MAX; ++i)
		rprintf("%d:%d\t", i, cur[i]);
	rprintf("\n");
#endif
}


int isEOF(sock_t con, const struct InputBuffer *ib)
{
	return ib->isEOF;
}

const char *stringifyErrorCode(int code)
{
	static char buf[80];
#ifdef __MINGW32__
	switch (code) {
		case WSAECONNRESET:
			return "WSAECONNRESET";
			break;
		case WSAECONNREFUSED:
			return "WSAECONNREFUSED";
			break;
		case WSAECONNABORTED:
			return "WSAECONNABORTED";
			break;
		case WSAENOTSOCK:
			return "WSAENOTSOCK";
			break;
		case WSAEINVAL:
			return "WSAEINVAL";
			break;
		case WSAEISCONN:
			return "WSAEISCONN";
			break;
		case WSAEALREADY:
			return "WSAEALREADY";
			break;
		case WSAEWOULDBLOCK:
			return "WSAEWOULDBLOCK";
			break;
		case WSAEINPROGRESS:
			return "WSAEINPROGRESS";
			break;
		default:
			sprintf(buf, "<unknown win:%d>", code);
			return buf;
			break;
	}
#else
	sprintf(buf, "<unknown unix:%d>", code);
	return buf;
#endif
}

void rshutdown(sock_t fd)
{
#ifdef __MINGW32__
	shutdown(fd, SD_BOTH);
#else
	shutdown(fd, SHUT_RDWR);
#endif
}

void setblocking(sock_t sock_fd)
{
	int retval;
#ifdef __MINGW32__
	do {
		u_long val;
		val = 0;
		retval = ioctlsocket(sock_fd, FIONBIO, &val);
		if (retval != 0) {
			int winerr;
			winerr = WSAGetLastError();
			monitorLog(PLACE_SETBLOCKING, winerr);
			rexit(1);
		}
	} while (0);
#else
	do {
		int flags;
		flags = fcntl(sock_fd, F_GETFL, 0);
		retval = fcntl(sock_fd, F_SETFL, flags & (~O_NONBLOCK));
		if (retval != 0) {
			rprintf("Error clearing O_NONBLOCK\n");
			rexit(1);
		}
	} while (0);
#endif
}
void setnonblocking(sock_t sock_fd)
{
	int retval;
#ifdef __MINGW32__
	do {
		u_long val;
		val = 1;
		retval = ioctlsocket(sock_fd, FIONBIO, &val);
		if (retval != 0) {
			rprintf("Error setting FIONBIO\n");
			rexit(1);
		}
	} while (0);
#else
	do {
		int flags;
		flags = fcntl(sock_fd, F_GETFL, 0);
		retval = fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
		if (retval != 0) {
			rprintf("Error setting O_NONBLOCK\n");
			rexit(1);
		}
	} while (0);
#endif
}

sock_t rsocket(void) {
	sock_t sock_fd;
	sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	setnonblocking(sock_fd);
	return sock_fd;
}

#ifndef __MINGW32__
void sigPIPEHandler(int i) {
}
#endif

int rinitNetworking(void) {
	static int mustInit = 1;
	if (mustInit) {
		mustInit = 0;
#ifdef __MINGW32__
	WSADATA wsadata;
	WSAStartup(MAKEWORD(2,0), &wsadata);
	WSASetLastError(0);
#else
	signal(SIGPIPE, sigPIPEHandler);
#endif
	}
	return 0;
}

int rbindAll(sock_t sock_fd)
{
	rsockaddr_t local;
	int retval;
	int one = 1;

	retval = setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one));

	if (retval != 0) {
		rprintf("setsockopt error\n");
		rexit(1);
	}

	local.sin_family = AF_INET;
	local.sin_port = htons(DEFAULTPORT);
	local.sin_addr.s_addr = INADDR_ANY;

	retval = bind(sock_fd, (struct sockaddr *) &local, sizeof(local));
	if (retval != 0) {
		rprintf("bind error\n");
		rexit(1);
	}
	retval = listen(sock_fd, MAXCLIENTS);
	if (retval != 0) {
		rprintf("listen error\n");
		rexit(1);
	}

	return 0;
}

int rconnectName(sock_t sock_fd, const char *hostname, unsigned short portno)
{
	int retval;
#ifdef __MINGW32__
	int winerr;
#endif
	rsockaddr_t local;
	hostent_t host;
	host = gethostbyname(hostname);
	if(host == NULL) {
		rprintf("Error looking up %s\n", hostname);
		perror("gethostbyname");
		rexit(1);
	}
	local.sin_family = AF_INET;
	local.sin_port = htons(portno);
#ifdef __MINGW32__
	local.sin_addr = *((LPIN_ADDR)*host->h_addr_list);
#else
	local.sin_addr = *((struct in_addr *)host->h_addr);
#endif

	rprintf("Connecting to %s at %s:%d\n", hostname, inet_ntoa(local.sin_addr), portno);

	do {
		retval = connect(sock_fd, (struct sockaddr *) &local, sizeof(local));
#ifdef __MINGW32__
		winerr = WSAGetLastError();
		if (retval == SOCKET_ERROR && winerr == WSAEISCONN)
			retval = 0;
		if (retval == SOCKET_ERROR)
			monitorLog(PLACE_CONNECT, winerr);
	} while (retval == SOCKET_ERROR);
#else
		if (retval != 0 && errno != EINPROGRESS) {
			perror("connect");
			exit(1);
		}
		monitorLog(PLACE_CONNECT, errno);
	} while (retval == -1 && errno == EINPROGRESS);
#endif
	return retval;
}

sock_t raccept(sock_t sock_fd)
{
#ifdef __MINGW32__
	int winerr;
#endif
	rsockaddr_t modaddr;
	int modaddrsize = sizeof(modaddr);

	sock_t retval;

	do {
		retval = accept(sock_fd, (struct sockaddr *) &modaddr, &modaddrsize);
#ifdef __MINGW32__
		if (retval == SOCKET_ERROR) {
			winerr = WSAGetLastError();
			rprintf("Got error code %s\n", stringifyErrorCode(winerr));
		}
	} while (retval == SOCKET_ERROR);
#else
	} while (retval == -1);
#endif
	rprintf("Received new connection from %s\n", inet_ntoa(modaddr.sin_addr));
	return retval;
}

int writeString(sock_t con, const char *buf, struct OutputBuffer *ob)
{
	return writeBytes(con, buf, strlen(buf), ob);
}

int rrecv(sock_t fd, char *read_buf, size_t count)
{
	int retval = -1;
	retval = recv(fd, read_buf, count, 0);
#ifdef __MINGW32__
	int winerr;
	if (retval == SOCKET_ERROR) {
		winerr = WSAGetLastError();
		if (winerr == WSAEISCONN || winerr == WSAEWOULDBLOCK)
			retval = 0;
		if (winerr == WSAENOTCONN || winerr == WSAECONNRESET)
			retval = -1;
		if (retval == -1)
			monitorLog(PLACE_RRECV, winerr);
	}
#endif
	return retval;
}

int writeBytes(sock_t con, const char *buf, int size, struct OutputBuffer *ob)
{
	int retval;
	char obuf[MAXLEN];
	memcpy(obuf, buf, size);
	obuf[size] = 0;
//	rprintf("Trying to write (%s) to socket %x\n", obuf, con);
	setblocking(con);
	retval = send(con, buf, size, 0);
	setnonblocking(con);
	if (retval != size) {
		rprintf("send error: %d\n", retval);
#ifdef __MINGW32__
		do {
			int winerr;
			winerr = WSAGetLastError();
			monitorLog(PLACE_WRITEBYTES, winerr);
		} while (0);
#endif
	}
	return retval;
}

void initOutputBuffer(struct OutputBuffer *ob)
{
	memset(ob, 0, sizeof(*ob));
}

void initInputBuffer(struct InputBuffer *ib)
{
	memset(ib, 0, sizeof(*ib));
}

int my_read(sock_t fd, char *ptr, size_t maxlen, struct InputBuffer *ib)
{
	addCount(NUM_MYREAD);
	showCounts();
	//setblocking(fd);
	//again:
	if (maxlen == 1) {
		rprintf("Inefficient myread(%d)\n", maxlen);
	}
	if (ib->read_cnt <= 0) {
			if ( (ib->read_cnt = rrecv(fd, ib->read_buf, maxlen)) < 0) {
#ifdef __MINGW32__
				int winerr;
				winerr = WSAGetLastError();
				if (winerr != WSAEWOULDBLOCK)
					monitorLog(PLACE_MYREAD, winerr);
				if (winerr == WSAECONNABORTED || winerr == WSAECONNRESET) {
					ib->isEOF = 1;
					return -1;
				}
				if (winerr == WSAEWOULDBLOCK)
					return 0;
//				if (winerr == WSAESHUTDOWN || winerr == WSAENOTCONN || winerr == WSAECONNRESET)
#else
#endif
				return -1;
			} else {
				if (ib->read_cnt == 0) {
#ifdef __MINGW32__
					ib->isEOF = 1;
#endif
					return 0;
				}
			}
			ib->read_ptr = ib->read_buf;
	}
	
	ib->read_cnt--;
	*ptr = *ib->read_ptr++;
	return 1;
}
	
size_t readn(sock_t fd, void *vptr, size_t len, struct InputBuffer *ib)
{
	size_t nleft;
	size_t nread;
	char *ptr;
	addCount(NUM_READN);
	showCounts();
	ptr = vptr;
	nleft = len;
	while (nleft > 0) {
		if ( (nread = recv(fd, ptr, nleft, 0)) < 0) {
			return -1;
		} else if (nread == 0)
				break;
		nleft -= nread;
		ptr += nread;
	}
	printf("In readn, returning %d\n", len-nleft);
	return len - nleft;
}

size_t writen(sock_t fd, const void *vptr, size_t len, struct OutputBuffer *ob)
{
	int nleft;
	int nwritten;
	const char *ptr;
	addCount(NUM_WRITEN);
	showCounts();

	ptr = vptr;
	nleft = len;
	while (nleft > 0) {
		addCount(NUM_WRITEN2);
		showCounts();
		nwritten = send(fd, ptr, nleft, 0);
		if (nwritten <= 0)
			return -1;
		nleft -= nwritten;
		ptr += nwritten;
	}
	return len;
}

size_t readlinebuf(void **vptrptr, struct InputBuffer *ib)
{
	if (ib->read_cnt)
		*vptrptr = ib->read_ptr;
	return (ib->read_cnt);
}

int inputBufferEmpty(const struct InputBuffer *ib)
{
	return ib->read_cnt == 0;
}

int getOK(sock_t sock_fd, struct InputBuffer *ib)
{
	int retcode;
	rprintf("Waiting for OK\n");
	do {
		retcode = getResponseCode(sock_fd, ib);
	} while (retcode == 0);
	if (retcode == -1) {
		rprintf("nsnet.o: Server died, exitting.\n");
		exit(0);
	}
	if (retcode != 200) {
		printf("Got bad response: %d\n", retcode);
		exit(1);
	}
	return 0;
}

int getResponseCode(sock_t sock_fd, struct InputBuffer *ib)
{
	char linebuf[MAXLEN+1];
	int len;
	do {
		len = readline(sock_fd, linebuf, MAXLEN, ib);
	} while (len == 0 && !isEOF(sock_fd, ib));
	if (isEOF(sock_fd, ib))
		return -1;
	linebuf[len] = '\0';
//	rprintf("Got response: <%s>\n", linebuf);
	strtok(linebuf, " ");
	return atoi(linebuf);
}

static int rselect_real(sock_t max_fd, fd_set *toread, fd_set *towrite, 
	fd_set *toerr,
	struct timeval *tv)
{
	return select(max_fd, toread,towrite, toerr, tv);
}

int rselect_timed(sock_t max_fd, fd_set *toread, fd_set *towrite, 
	fd_set *toerr, struct timeval *tv)
{
	int retval;
	retval = rselect_real(max_fd, toread, towrite, toerr, tv);
	return retval;
}
	
int rselect(sock_t max_fd, fd_set *toread, fd_set *towrite, fd_set *toerr)
{
	int retval;

	retval = rselect_real(max_fd, toread, towrite, toerr, NULL);
#ifdef __MINGW32__
	if (retval == SOCKET_ERROR) {
		int winerr;
		winerr = WSAGetLastError();
		monitorLog(PLACE_RSELECT, winerr);
	}
#else
#endif

	return retval;
}

int readline(sock_t fd, char *vptr, size_t maxlen, struct InputBuffer *ib)
{
	size_t n;
	int rc;
	char c, *ptr;

	addCount(NUM_READLINE);
	showCounts();

	setblocking(fd);
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		rc = my_read(fd, &c, maxlen, ib);
		if (rc == -1)
			return -1;
		if (rc == 0) {
			*ptr = 0;
			n -= 1;
			break;
		}
		if (rc == 1) {
			if (c != '\n' && c != '\r')
				*ptr++ = c;
			if (c == '\n')
				break;
		} 
	}
	*ptr = '\0';
	return n;
}

