#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <openedf.h>
#include <unistd.h>
#include <edfmacros.h>
#include <nsutil.h>

#define MAXERRORLEN 1024

static char errorBuffer[MAXERRORLEN];

void setLastError(const char *fmt, ...);

int EDFUnpackInt(const char *inp, int fieldLen)
{
	return atoi(EDFUnpackString(inp, fieldLen));
}

double EDFUnpackDouble(const char *inp, int fieldLen)
{
	return atof(EDFUnpackString(inp, fieldLen));
}

const char *EDFUnpackString(const char *inp, int fieldLen)
{
	static char retbuf[256];
	strncpy(retbuf, inp, fieldLen);
	retbuf[fieldLen] = '\0';
	return retbuf;
}

void storeEDFString(char *packet, size_t memsize, const char *str)
{
	char fmt[8];
	char buf[257];
	sprintf(fmt, "%%- %ds", memsize);
	sprintf(buf, fmt, str);
	memcpy(packet, buf, memsize);
}

// TODO: Make this more correct for combinations of d and memsize
void storeEDFDouble(char *packet, size_t memsize, double d)
{
	char buf[64];
	sprintf(buf, "%g", d);
	storeEDFString(packet, memsize, buf);
}

void storeEDFInt(char *packet, size_t memsize, int i)
{
	char buf[64];
	sprintf(buf, "%d", i);
	storeEDFString(packet, memsize, buf);
}

int EDFEncodePacket(char *packet, const struct EDFDecodedConfig *cfg)
{
	int whichChannel, totalChannels;
	STOREHFS(dataFormat, cfg->hdr.dataFormat);
	STOREHFS(localPatient, cfg->hdr.localPatient);
	STOREHFS(localRecorder, cfg->hdr.localRecorder);
	STOREHFS(recordingStartDate, cfg->hdr.recordingStartDate);
	STOREHFS(recordingStartTime, cfg->hdr.recordingStartTime);
	STOREHFI(headerRecordBytes, cfg->hdr.headerRecordBytes);
	STOREHFS(manufacturerID, cfg->hdr.manufacturerID);
	STOREHFI(dataRecordCount, cfg->hdr.dataRecordCount);
	STOREHFD(dataRecordSeconds, cfg->hdr.dataRecordSeconds);
	STOREHFI(dataRecordChannels, cfg->hdr.dataRecordChannels);
	totalChannels = cfg->hdr.dataRecordChannels;
	packet += 256;
	for (whichChannel = 0; whichChannel < totalChannels; whichChannel++) {
		STORECFS(label, cfg->chan[whichChannel].label);
		STORECFS(transducer, cfg->chan[whichChannel].transducer);
		STORECFS(dimUnit, cfg->chan[whichChannel].dimUnit);
		STORECFD(physMin, cfg->chan[whichChannel].physMin);
		STORECFD(physMax, cfg->chan[whichChannel].physMax);
		STORECFD(digiMin, cfg->chan[whichChannel].digiMin);
		STORECFD(digiMax, cfg->chan[whichChannel].digiMax);
		STORECFS(prefiltering, cfg->chan[whichChannel].prefiltering);
		STORECFI(sampleCount, cfg->chan[whichChannel].sampleCount);
		STORECFS(reserved, cfg->chan[whichChannel].reserved);
	}
	return 0;
}

void EDFDecodeChannelHeader(struct EDFDecodedChannelHeader *result, const char *packet, int whichChannel, int totalChannels)
{
	memset((char *) result, 0, sizeof(*result));
	LOADCFS(label);
	LOADCFS(transducer);
	LOADCFS(dimUnit);
	LOADCFD(physMin);
	LOADCFD(physMax);
	LOADCFD(digiMin);
	LOADCFD(digiMax);
	LOADCFS(prefiltering);
	LOADCFI(sampleCount);
	LOADCFS(reserved);
}

void EDFDecodeHeaderPreamble(struct EDFDecodedHeader *result, const char *packet) {
	memset((char *) result, 0, sizeof(*result));
	LOADHFS(dataFormat);
	LOADHFS(localPatient);
	LOADHFS(localRecorder);
	LOADHFS(recordingStartDate);
	LOADHFS(recordingStartTime);
	LOADHFI(headerRecordBytes);
	LOADHFS(manufacturerID);
	LOADHFI(dataRecordCount);
	LOADHFD(dataRecordSeconds);
	LOADHFI(dataRecordChannels);
}

int readEDFString(struct EDFDecodedConfig *cfg, const char *buf, int len)
{
	int i;
	if (len >= 256)
		EDFDecodeHeaderPreamble(&cfg->hdr, buf);
	else {
		rprintf("Header too small\n");
		return 1;
	}
	assert(cfg->hdr.dataRecordChannels > 0);
	assert(cfg->hdr.dataRecordChannels < MAXCHANNELS);
	setEDFHeaderBytes(cfg);
	if (len < cfg->hdr.headerRecordBytes) {
		rprintf("Header too small\n");
		return 1;
	}
	for (i = 0; i < cfg->hdr.dataRecordChannels; ++i) {
		EDFDecodeChannelHeader(&cfg->chan[i], buf+256, i, cfg->hdr.dataRecordChannels);
	}
	return 0;
}

int writeEDFString(const struct EDFDecodedConfig *cfg, char *buf, int *buflen)
{
	int retval;
	char pktbuf[(MAXCHANNELS+1) * 256];
	retval = EDFEncodePacket(pktbuf, cfg);
	if (retval) return retval;
	if (cfg->hdr.headerRecordBytes <= *buflen) {
		*buflen = cfg->hdr.headerRecordBytes;
		memcpy(buf, pktbuf, *buflen);
		return 0;
	}
	else {
		*buflen = 0;
		return 1;
	}
}

int writeEDF(int fd, const struct EDFDecodedConfig *cfg)
{
	int retval;
	char pktbuf[(MAXCHANNELS+1) * 256];
	retval = EDFEncodePacket(pktbuf, cfg);
	if (retval) return retval;
	retval = write(fd, pktbuf, cfg->hdr.headerRecordBytes);
	if (retval != cfg->hdr.headerRecordBytes) {
		perror("write");
		return 1;
	}
	return 0;
}

int setEDFHeaderBytes(struct EDFDecodedConfig *cfg)
{
	cfg->hdr.headerRecordBytes = 256 * (cfg->hdr.dataRecordChannels + 1);
	return 0;
}

int readEDF(int fd, struct EDFDecodedConfig *cfg)
{
	int i;
	int retval;
	char pktbuf[(MAXCHANNELS+1) * 256];
	retval = read(fd, pktbuf, 256);
	if (retval != 256) {
		perror("read");
		return 1;
	}
	EDFDecodeHeaderPreamble(&cfg->hdr, pktbuf);
	assert(cfg->hdr.dataRecordChannels > 0);
	assert(cfg->hdr.dataRecordChannels < MAXCHANNELS);
	setEDFHeaderBytes(cfg);
	retval = read(fd, pktbuf+256, cfg->hdr.headerRecordBytes - 256);
	if (retval != cfg->hdr.headerRecordBytes - 256) {
		perror("read");
		return 1;
	}
	for (i = 0; i < cfg->hdr.dataRecordChannels; ++i) {
		EDFDecodeChannelHeader(&cfg->chan[i], pktbuf+256, i, cfg->hdr.dataRecordChannels);
	}
	return 0;
}


double getSecondsPerSample(const struct EDFDecodedConfig *cfg, int whichChan)
{
	return (double) cfg->hdr.dataRecordSeconds / cfg->chan[whichChan].sampleCount;
}

double getSamplesPerSecond(const struct EDFDecodedConfig *cfg, int whichChan)
{
	return (double) cfg->chan[whichChan].sampleCount / cfg->hdr.dataRecordSeconds;
}

int getDataRecordChunkSize(const struct EDFDecodedConfig *cfg)
{
	int i, sum=0;
	for (i = 0; i < cfg->hdr.dataRecordChannels; ++i)
		sum += cfg->chan[i].sampleCount;
	return sum * BYTESPERSAMPLE;
}

struct EDFInputIterator *newEDFInputIterator(const struct EDFDecodedConfig *cfg)
{
	struct EDFInputIterator *edfi;
	edfi = calloc(1, sizeof(struct EDFInputIterator));
	assert(edfi && "out of memory!");
	edfi->cfg = *cfg;
	edfi->dataRecordNum = 0;
	edfi->sampleNum = 0;
	edfi->dataRecord = calloc(1, getDataRecordChunkSize(&edfi->cfg));
	assert(edfi->dataRecord && "out of memory!");
	return edfi;
}

void freeEDFInputIterator(struct EDFInputIterator *edfi)
{
	free(edfi->dataRecord);
	edfi->dataRecord = NULL;
	free(edfi);
}

int stepEDFInputIterator(struct EDFInputIterator *edfi)
{
	edfi->sampleNum += 1;
	if (edfi->sampleNum == edfi->cfg.chan[0].sampleCount) {
		edfi->sampleNum = 0;
		edfi->dataRecordNum += 1;
	}
	return 0;
}


int readDataRecord(const struct EDFInputIterator *edfi, FILE *fp)
{
	int retval;
	retval = fseek(fp, 
			edfi->cfg.hdr.headerRecordBytes + 
			getDataRecordChunkSize(&edfi->cfg) * edfi->dataRecordNum,
			SEEK_SET);
	if (retval != 0) return retval;
	retval = fread(edfi->dataRecord, 1, getDataRecordChunkSize(&edfi->cfg), fp);
	if (retval != getDataRecordChunkSize(&edfi->cfg))
		return 1;
	return 0;
}

/* Assumes all channels sample at the same frequency */
int fetchSamples(const struct EDFInputIterator *edfi, short *samples, FILE *fp)
{
	int retval;
	int i;
	int sampleCount;
	if (edfi->sampleNum == 0) {
		retval = readDataRecord(edfi, fp);
		if (retval != 0) return retval;
	}
	sampleCount = edfi->cfg.chan[0].sampleCount;
	for (i = 0; i < edfi->cfg.hdr.dataRecordChannels; ++i) {
		// TODO: Make this big-endian-safe
		samples[i] = * (short *) 
			(&edfi->dataRecord[BYTESPERSAMPLE*(edfi->sampleNum + i * sampleCount)]);
	}
	return 0;
}

int isValidREDF(const struct EDFDecodedConfig *cfg)
{
	int i;
	if (cfg->hdr.dataRecordSeconds != 1.0) {
		setLastError("The data record must be exactly 1 second, not %f.", 
								 cfg->hdr.dataRecordSeconds);
		return 0;
	}
	if (cfg->hdr.dataRecordChannels < 1) {
		setLastError("The data record must have at least one channel.");
		return 0;
	}
	if (cfg->chan[0].sampleCount < 1) {
		setLastError("Channel 0 must have at least one sample.");
		return 0;
	}
	for (i = 1; i < cfg->hdr.dataRecordChannels; ++i) {
		if (cfg->chan[i].sampleCount != cfg->chan[0].sampleCount) {
			setLastError("Channel %d has %d samples, but channel 0 has %d.  These must be the same.", cfg->chan[i].sampleCount, cfg->chan[0].sampleCount);
			return 0;
		}
	}
	return 1;
}

const char *getDMY(void)
{
	static char buf[81];
	time_t t;
	struct tm *it;
	time(&t);
	it = localtime(&t);
	sprintf(buf, "%02d.%02d.%02d", it->tm_mday, it->tm_mon+1, it->tm_year % 100);
	return buf;
}

const char *getHMS(void)
{
	static char buf[81];
	time_t t;
	struct tm *it;
	time(&t);
	it = localtime(&t);
	sprintf(buf, "%02d.%02d.%02d", it->tm_hour, it->tm_min, it->tm_sec);
	return buf;
}

/* Sets the header byte count correctly and sets the data record
 * duration to be 1 second and scaling sampleCount appropriately
 * for each channel.  This function assumes only one sampling rate
 * is used for all channels.  Also, sets the date and time correctly
 * according to the system clock.
 */

int makeREDFConfig(struct EDFDecodedConfig *result, const struct EDFDecodedConfig *source)
{
	int newSamples, i;
	*result = *source;
	if (source->hdr.dataRecordSeconds != 1.0) {
		result->hdr.dataRecordSeconds = 1;
		newSamples = ((double)source->chan[0].sampleCount) / 
			             source->hdr.dataRecordSeconds;
		for (i = 0; i < source->hdr.dataRecordChannels; ++i)
			result->chan[i].sampleCount = newSamples;
	}
	setEDFHeaderBytes(result);
	strcpy(result->hdr.recordingStartDate, getDMY());
	strcpy(result->hdr.recordingStartTime, getHMS());
	assert(isValidREDF(result));
	return 0;
}

const char *getLastError(void)
{
	return errorBuffer;
}

void setLastError(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(errorBuffer, MAXERRORLEN-1, fmt, ap);
	errorBuffer[MAXERRORLEN-1] = '\0';
	va_end(ap);
}

