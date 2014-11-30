#include <stdio.h>
#include <monitor.h>
#include <nsnet.h>

int monitorLog(enum PlaceCode placeCode, int errNum)
{
	char *descPlace;
	char buf[MAXLEN];
	sprintf(buf, "unknown error code: %d", errNum);
	switch (placeCode) {
		case PLACE_CONNECT: descPlace = "connect"; break;
		case PLACE_WRITEBYTES: descPlace = "writebytes"; break;
		case PLACE_MYREAD: descPlace = "myread"; break;
		case PLACE_SETBLOCKING: descPlace = "setblocking"; break;
		case PLACE_RRECV: descPlace = "rrecv"; break;
		case PLACE_RSELECT: descPlace = "rselect"; break;
		default:
			descPlace = buf;
			break;
	}
	fprintf(stdout, "Recieved error code %d from placeCode %d.  errstr is %s and descPlace is %s\n", errNum, placeCode, stringifyErrorCode(errNum), descPlace);
	fflush(stdout);
	return 0;
}

