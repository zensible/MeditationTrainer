#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "nsutil.h"
#include "nsser.h"

void set_port_options(int fd);

ser_t openSerialPort(const char *devname)
{
#ifdef __MINGW32__
	ser_t retval;
	retval = CreateFile(devname,
-~-~-~      GENERIC_READ | GENERIC_WRITE,
-~-~-~      0, NULL, OPEN_EXISTING,
-~-~-~      FILE_ATTRIBUTE_NORMAL, NULL);
	if (retval == INVALID_HANDLE_VALUE) {
-~  rprintf("Couldn't open serial port: %s", devname);
		rexit(1);
	}
	if (!SetCommMask(retval, EV_RXCHAR))
-~	rprintf("Couldn't do a SetCommMask(): %s", devname);

	// Set read buffer == 10K, write buffer == 4K
	SetupComm(retval, 10240, 4096);

	do {
	DCB dcb;

	dcb.DCBlength= sizeof(DCB);
GetCommState(retval, &dcb);

		dcb.BaudRate= DEFAULT_BAUD;
		dcb.ByteSize= 8;
		dcb.Parity= NOPARITY;
		dcb.StopBits= ONESTOPBIT;
		dcb.fOutxCtsFlow= FALSE;
		dcb.fOutxDsrFlow= FALSE;
		dcb.fInX= FALSE;
		dcb.fOutX= FALSE;
		dcb.fDtrControl= DTR_CONTROL_DISABLE;
		dcb.fRtsControl= RTS_CONTROL_DISABLE;
		dcb.fBinary= TRUE;
		dcb.fParity= FALSE;

	if (!SetCommState(retval, &dcb)) {
			rprintf("%s", "Unable to set up serial port parameters");
			rexit(1);
		}
	} while (0);
	return retval;
#else
	ser_t fd; /* File descriptor for the port */


	fd = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1)
		{
		fprintf(stderr, "Cannot open port %s\n", devname);
		exit(1);
		}
	else
		fcntl(fd, F_SETFL, FNDELAY);
	set_port_options(fd);
	return (fd);
#endif
}
/* Returns number of bytes read, or -1 for error */
#ifdef __MINGW32__
int readSerial(ser_t s, char *buf, int size)
{
   BOOL rv;
   COMSTAT comStat;
   DWORD errorFlags;
   DWORD len;
   // Only try to read number of bytes in queue
   ClearCommError(s, &errorFlags, &comStat);
   len = comStat.cbInQue;
   if (len > size) len = size;
   if (len > 0) {
      rv = ReadFile(s, (LPSTR)buf, len, &len, NULL);
			if (!rv) {
				 len= 0;
				 ClearCommError(s, &errorFlags, &comStat);
			}
		if (errorFlags > 0) {
			rprintf("Serial read error");
			ClearCommError(s, &errorFlags, &comStat);
			return -1;
		}
	}
	return len;
}
#else

int readSerial(ser_t s, char *buf, int size)
{
	int retval;
	retval = read(s, buf, size);
	if (retval < 0)
		retval = 0;
	return retval;
}

void set_port_options(int fd)
{
	int retval;
	struct termios options;
/*
 *      * Get the current options for the port...
 *      */

    retval = tcgetattr(fd, &options);
		assert(retval == 0 && "tcgetattr problem");

/*
 *      * Set the baud rates to 19200...
 *      */

    cfsetispeed(&options, B57600);
    cfsetospeed(&options, B57600);

/*
 *      * Enable the receiver and set local mode...
 *      */

    options.c_cflag |= (CLOCAL | CREAD);

/* 8-N-1 */
options.c_cflag &= ~PARENB;
options.c_cflag &= ~CSTOPB;
options.c_cflag &= ~CSIZE;
options.c_cflag |= CS8;

/* Disable flow-control */
  options.c_cflag &= ~CRTSCTS;

/* Raw processing mode */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);





/*
 *      * Set the new options for the port...
 *      */

	retval = tcsetattr(fd, TCSANOW, &options);
}
#endif


