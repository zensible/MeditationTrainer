#ifndef __MONITOR_H
#define __MONITOR_H

#define MAX_PLACECODE 0x10000

enum PlaceCode { PLACE_CONNECT, PLACE_WRITEBYTES, PLACE_MYREAD, 
PLACE_SETBLOCKING, PLACE_RRECV, PLACE_RSELECT,
 };

int monitorLog(enum PlaceCode placeCode, int errNum);

#endif
