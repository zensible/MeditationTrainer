#include <assert.h>
#include <openedf.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pctimer.h>
#include <ctype.h>
#include <nsutil.h>
#include <nsnet.h>

const char *helpText =
"writeedf   v 0.34 by Rudi Cilibrasi\n"
"\n"
"Usage:  writeedf [options] <filename>\n"
"\n"
"        -p port          Port number to use (default 8336)\n"
"        -n hostname      Host name of the NeuroCaster server to connect to\n"
"                         (this defaults to 'localhost')\n"
"        -s <num>         Only record this many seconds.\n"
"                         (default is to record until interrupted)\n"
"        -e <intnum>      Integer ID specifying which EEG to log (default: 0)\n"
"The filename specifies the new EDF file to create.\n"
;

#define MINLINELENGTH 4
#define DELIMS " \r\n"

struct Options {
	char hostname[MAXLEN];
	unsigned short port;
	char filename[MAXLEN];
	int eegNum;
	int isFilenameSet;
	int isLimittedTime;
	double seconds;
};

struct EDFDecodedConfig cfg;
static struct OutputBuffer ob;
struct InputBuffer ib;
char lineBuf[MAXLEN];
int linePos = 0;
int blockCounter = 0;

unsigned short **sampleBuffers; // [MAXCHANNELS][MAXSAMPLES]
unsigned int sampleCount;

struct Options opts;

void resetSampleBuffers()
{
	sampleCount = 0;
}

void initSampleBuffers()
{
	int i;
	sampleBuffers = calloc(sizeof(unsigned short *), cfg.hdr.dataRecordChannels);
	for (i = 0; i < cfg.hdr.dataRecordChannels; ++i)
		sampleBuffers[i] = calloc(cfg.chan[0].sampleCount, sizeof(unsigned short));
	resetSampleBuffers();
}

void writeBuffers()
{
	int i;
	FILE *fp;
	fp = fopen(opts.filename, "a");
	assert(fp != NULL && "Cannot open file!");
	fseek(fp, 0, SEEK_END);
	for (i = 0; i < cfg.hdr.dataRecordChannels; ++i)
		fwrite(sampleBuffers[i], sizeof(unsigned short), sampleCount, fp);
	fclose(fp);
	rprintf("Wrote block %d\n", blockCounter);
	blockCounter += 1;
	resetSampleBuffers();
}

void handleSamples(int packetCounter, int channels, int *samples)
{
	static int lastPacketCounter = 0;
	int i;
	if (lastPacketCounter + 1 != packetCounter && packetCounter != 0 && lastPacketCounter != 0) {
		rprintf("May have missed packet: got packetCounters %d and %d\n", lastPacketCounter, packetCounter);
	}
	lastPacketCounter = packetCounter;
	for (i = 0; i < channels; ++i)
		sampleBuffers[i][sampleCount] = samples[i];
	sampleCount += 1;
	if (sampleCount == cfg.chan[0].sampleCount) {
		writeBuffers();
	}
}

int isANumber(const char *str) {
	int i;
	for (i = 0; str[i]; ++i)
		if (!isdigit(str[i]))
			return 0;
	return 1;
}

void serverDied(void)
{
	rprintf("Server died!\n");
	exit(1);
}

int main(int argc, char **argv)
{
	char EDFPacket[MAXHEADERLEN];
	char cmdbuf[80];
	int EDFLen = MAXHEADERLEN;
	FILE *fp;
	fd_set toread;
	int i;
	double t0;
	sock_t sock_fd;
	int retval;
	strcpy(opts.hostname, DEFAULTHOST);
	opts.port = DEFAULTPORT;
	for (i = 1; i < argc; ++i) {
		char *opt = argv[i];
		if (opt[0] == '-') {
			switch (opt[1]) {
				case 'h':
					printf("%s", helpText);
					exit(0);
					break;
				case 's':
					opts.seconds = atof(argv[i+1]);
					opts.isLimittedTime = 1;
					i += 1;
					break;
				case 'e':
					opts.eegNum = atoi(argv[i+1]);
					i += 1;
					break;
				case 'n':
					strcpy(opts.hostname, argv[i+1]);
					i += 1;
					break;
				case 'p':
					opts.port = atoi(argv[i+1]);
					i += 1;
					break;
				}
			}
		else {
			if (opts.isFilenameSet) {
				fprintf(stderr, "Error: only one edf file allowed: %s and %s\n",
						opts.filename, argv[i]);
				exit(1);
			}
			strcpy(opts.filename, argv[i]);
			opts.isFilenameSet = 1;
		}
	}
	rinitNetworking();
	if (!opts.isFilenameSet) {
		fprintf(stderr, "Must supply the filename of the EDF to write.\n");
		exit(1);
	}
	fp = fopen(opts.filename, "wb");
	assert(fp != NULL && "Cannot open file!");

	sock_fd = rsocket();
	if (sock_fd < 0) {
		perror("socket");
		rexit(1);
	}

	retval = rconnectName(sock_fd, opts.hostname, opts.port);
	if (retval != 0) {
		rprintf("connect error\n");
		rexit(1);
	}

	rprintf("Socket connected.\n");
	fflush(stdout);

	writeString(sock_fd, "display\n", &ob);
	getOK(sock_fd, &ib);
	rprintf("Finished display, doing getheader.\n");
	sprintf(cmdbuf, "getheader %d\n", opts.eegNum);
	writeString(sock_fd, cmdbuf, &ob);
	getOK(sock_fd, &ib);
	if (isEOF(sock_fd, &ib))
		serverDied();
	EDFLen = readline(sock_fd, EDFPacket, sizeof(EDFPacket), &ib);
	rprintf("Got EDF Header <%s>\n", EDFPacket);
	fwrite(EDFPacket, 1, EDFLen, fp);
	fclose(fp);
	readEDFString(&cfg, EDFPacket, EDFLen);
	initSampleBuffers();
	sprintf(cmdbuf, "watch %d\n", opts.eegNum);
	writeString(sock_fd, cmdbuf, &ob);
	getOK(sock_fd, &ib);
	t0 = pctimer();
	FD_ZERO(&toread);
	FD_SET(sock_fd, &toread);
	for (;;) {
		for (;;) {
			char *cur;
			int vals[MAXCHANNELS + 5];
			int curParam = 0;
			int devNum, packetCounter=0, channels=0, *samples;
			rselect(sock_fd+1, &toread, NULL, NULL);
			linePos = readline(sock_fd, lineBuf, sizeof(EDFPacket), &ib);
			if (isEOF(sock_fd, &ib))
				break;
			if (linePos < MINLINELENGTH)
				continue;
			if (lineBuf[0] != '!')
				continue;
			for (cur = strtok(lineBuf, DELIMS); cur ; cur = strtok(NULL, DELIMS)) {
				if (isANumber(cur))
					vals[curParam++] = atoi(cur);
			}
			// <devicenum> <packetcounter> <channels> data0 data1 data2 ...
			if (curParam < 3)
				continue;
			devNum = vals[0];
			packetCounter = vals[1];
			channels = vals[2];
			samples = vals + 3;
			handleSamples(packetCounter, channels, samples);
//				for (i = 0; i < channels; ++i) {
//					rprintf(" %d", samples[i]);
//				}
		}
		rsleep(20);
	}
	fclose(fp);
	return 0;
}
