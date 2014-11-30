#ifndef __NSSER_H
#define __NSSER_H

#ifdef __MINGW32__
#define DEFAULTDEVICE "COM1"
#include <windows.h>
#include <winbase.h>

#define DEFAULT_BAUD CBR_57600
#define ser_t HANDLE
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#define DEFAULTDEVICE "/dev/ttyS0"
#define ser_t int
#endif

/* Pass the device name, if it returns it has succeeded */
ser_t openSerialPort(const char *devname);

/* Returns number of bytes read, or -1 for error */
int readSerial(ser_t s, char *buf, int size);

#endif
