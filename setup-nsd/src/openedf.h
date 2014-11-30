#ifndef __OPENEDF_H
#define __OPENEDF_H

#include <stdio.h>

#include <nsnet.h>

#define MAXCHANNELS 32
#define BYTESPERSAMPLE 2
#define MAXHEADERLEN ((MAXCHANNELS+1) * 256)

#pragma pack(1)

struct EDFPackedHeader {
	char dataFormat[8];
	char localPatient[80];
	char localRecorder[80];
	char recordingStartDate[8];
	char recordingStartTime[8];
	char headerRecordBytes[8];
	char manufacturerID[44];
	char dataRecordCount[8];
	char dataRecordSeconds[8];
	char dataRecordChannels[4];
};

struct EDFPackedChannelHeader {
	char label[16];
	char transducer[80];
	char dimUnit[8];
	char physMin[8];
	char physMax[8];
	char digiMin[8];
	char digiMax[8];
	char prefiltering[80];
	char sampleCount[8];
	char reserved[32];
};

#pragma pack(4)

struct EDFDecodedHeader {
	int headerRecordBytes;
	int dataRecordCount;
	int dataRecordChannels;
	char dataFormat[9];
	char localPatient[81];
	char localRecorder[81];
	char recordingStartDate[9];
	char recordingStartTime[9];
	char manufacturerID[45];
	double dataRecordSeconds;
};

struct EDFDecodedChannelHeader {
	int sampleCount;
	double physMin, physMax;
	double digiMin, digiMax;
	char label[17];
	char transducer[81];
	char dimUnit[9];
	char prefiltering[81];
	char reserved[33];
};

struct EDFDecodedConfig {
	struct EDFDecodedHeader hdr;
	struct EDFDecodedChannelHeader chan[MAXCHANNELS];
};

struct EDFInputIterator {
	struct EDFDecodedConfig cfg;
	int dataRecordNum;
	int sampleNum;
	char *dataRecord;
};

struct EDFInputIterator *newEDFInputIterator(const struct EDFDecodedConfig *cfg);
int stepEDFInputIterator(struct EDFInputIterator *edfi);
int fetchSamples(const struct EDFInputIterator *edfi, short *samples, FILE *fp);
void freeEDFInputIterator(struct EDFInputIterator *edfi);

int EDFUnpackInt(const char *inp, int fieldLen);
double EDFUnpackDouble(const char *inp, int fieldLen);
const char *EDFUnpackString(const char *inp, int fieldLen);

int EDFEncodePacket(char *target, const struct EDFDecodedConfig *cfg);

int writeEDF(int fd, const struct EDFDecodedConfig *cfg);
int readEDF(int fd, struct EDFDecodedConfig *cfg);
int writeEDFString(const struct EDFDecodedConfig *cfg, char *buf, int *buflen);
int readEDFString(struct EDFDecodedConfig *cfg, const char *buf, int len);

int setEDFHeaderBytes(struct EDFDecodedConfig *cfg);
int getDataRecordChunkSize(const struct EDFDecodedConfig *cfg);

double getSamplesPerSecond(const struct EDFDecodedConfig *cfg, int whichChan);
double getSecondsPerSample(const struct EDFDecodedConfig *cfg, int whichChan);

/* Realtime EDF (REDF) is a restricted type of EDF for the network */
int isValidREDF(const struct EDFDecodedConfig *cfg);
int makeREDFConfig(struct EDFDecodedConfig *result, const struct EDFDecodedConfig *source);

/* Returns a text description of the last error */
const char *getLastError(void);

#endif

