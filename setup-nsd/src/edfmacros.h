#ifndef __EDFMACROS_H
#define __EDFMACROS_H

#define DECODEHEADERFIELD(packet, fieldName) \
				packet+offsetof(struct EDFPackedHeader,fieldName), sizeof((struct EDFPackedHeader *)0)->fieldName

#define DECODECHANNELFIELD(packet, fieldName, whichChannel, totalChannels) \
				packet+totalChannels*offsetof(struct EDFPackedChannelHeader,fieldName) + whichChannel*sizeof(((struct EDFPackedChannelHeader *)0)->fieldName),sizeof((struct EDFPackedChannelHeader *)0)->fieldName

#define LOADHFI(m) \
	result->m = EDFUnpackInt(DECODEHEADERFIELD(packet, m))
#define LOADHFD(m) \
	result->m = EDFUnpackDouble(DECODEHEADERFIELD(packet, m))
#define LOADHFS(m) \
	strcpy(result->m, EDFUnpackString(DECODEHEADERFIELD(packet, m)))

#define LOADCFI(m) \
	result->m = EDFUnpackInt(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels))
#define LOADCFD(m) \
	result->m = EDFUnpackDouble(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels))
#define LOADCFS(m) \
	strcpy(result->m, EDFUnpackString(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels)))

#define STORECFI(m, i) \
	storeEDFInt(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels), i)
#define STORECFD(m, d) \
	storeEDFDouble(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels), d)
#define STORECFS(m, s) \
	storeEDFString(DECODECHANNELFIELD(packet, m, whichChannel, totalChannels), s)

#define STOREHFI(m, i) \
	storeEDFInt(DECODEHEADERFIELD(packet, m), i)
#define STOREHFD(m, d) \
	storeEDFDouble(DECODEHEADERFIELD(packet, m), d)
#define STOREHFS(m, s) \
	storeEDFString(DECODEHEADERFIELD(packet, m), s)

#endif
