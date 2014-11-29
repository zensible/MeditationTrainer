#include <assert.h>
#include <stdlib.h>
#include <openedf.h>
#include <string.h>
#include <sys/time.h>
#include <pctimer.h>
#include <nsutil.h>
#include <nsnet.h>

const char *helpText =
"readedf   v 0.43 by Rudi Cilibrasi\n"
"\n"
"Usage:  readedf [options] <filename>\n"
"\n"
"        -p port          Port number to use (default 8336)\n"
"        -n hostname      Host name of the NeuroCaster server to connect to\n"
"                         (this defaults to 'localhost')\n"
"        -c numlist       Numlist must be a comma-separated list of integers\n"
"                         that specify what channels to send. The first\n"
"                         channel is numbered 0. The default is all channels.\n"
"        -f               Fast Mode. Send data at maximum speed.\n"
"                         Default behavior sends data at EDF sampling rate.\n"
;

struct Options {
	int isMaxSpeed; // to turn off the gated loop
	int userSpecifiedChannels;
	int channelReadFlags[MAXCHANNELS];  // which channels to use
	char hostname[MAXLEN];
	char filename[MAXLEN];
	int isFilenameSet;
	unsigned short port;
};

struct Options opts;

struct OutputBuffer ob;
struct InputBuffer ib;

#define SPEEDUPFACTOR 4.0

void printHeader(const struct EDFDecodedHeader *hdr)
{
	printf("The data record count is %d\n", hdr->dataRecordCount);
	printf("The data record channels is %d\n", hdr->dataRecordChannels);
	printf("The data record seconds is %f\n", hdr->dataRecordSeconds);
}

void handleSamples(sock_t sock_fd, int packetCounter, int chan, short *vals)
{
	char buf[MAXLEN];
	int bufPos = 0;
	int i;
	bufPos += sprintf(buf+bufPos, "! %d %d", packetCounter, chan);
	for (i = 0; i < chan; ++i)
		bufPos += sprintf(buf+bufPos, " %d", vals[i]);
	bufPos += sprintf(buf+bufPos, "\r\n");
	writen(sock_fd, buf, bufPos, &ob);
//	rprintf("SENT:%s", buf);
}

int main(int argc, char **argv)
{
	char EDFPacket[MAXHEADERLEN];
	double speedup = 1;
	int wantedChannels[MAXCHANNELS];
	int EDFLen = MAXHEADERLEN;
	struct EDFDecodedConfig cfg, normalized, forNC;
	struct EDFInputIterator *edfi;
	FILE *fp;
	int i;
	double t0;
	sock_t sock_fd;
	int retval;
	int chunksize;
	strcpy(opts.hostname, DEFAULTHOST);
	opts.port = DEFAULTPORT;
	for (i = 1; i < argc; ++i) {
		char *opt = argv[i], *cur;
		if (opt[0] == '-') {
			switch (opt[1]) {
				case 'c':
					for (cur = strtok(argv[i+1], ", ");cur;cur=strtok(NULL,", "))
						opts.channelReadFlags[atoi(cur)] = 1;
					opts.userSpecifiedChannels = 1;
					i += 1;
					break;
				case 'h':
					printf("%s", helpText);
					exit(0);
					break;
				case 'f':
					opts.isMaxSpeed = 1;
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
	if (!opts.userSpecifiedChannels) {
		for (i = 0; i < MAXCHANNELS; ++i)
			opts.channelReadFlags[i] = 1;
	}
	if (!opts.isFilenameSet) {
		fprintf(stderr, "Must supply the filename of the EDF to read.\n");
		exit(1);
	}
	fp = fopen(opts.filename, "rb");
	assert(fp != NULL && "Cannot open file!");
	retval = readEDF(fileno(fp), &cfg);
	printHeader(&cfg.hdr);
	makeREDFConfig(&normalized, &cfg);
	forNC = normalized;
	forNC.hdr.dataRecordChannels = 0;
	for (i = 0; i < normalized.hdr.dataRecordChannels; ++i) {
		if (opts.channelReadFlags[i]) {
			memcpy(&forNC.chan[forNC.hdr.dataRecordChannels], &normalized.chan[i], sizeof(normalized.chan[0]));
			wantedChannels[forNC.hdr.dataRecordChannels] = i;
			forNC.hdr.dataRecordChannels += 1;
		}
	}
	if (forNC.hdr.dataRecordChannels == 0) {
		fprintf(stderr, "Must specify some channels to read (e.g. -c 0,1 ).\n");
		exit(1);
	}
		
	setEDFHeaderBytes(&forNC);

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

	writeEDFString(&forNC, EDFPacket, &EDFLen);
	writeString(sock_fd, "eeg\n", &ob);
	writeString(sock_fd, "setheader ", &ob);
	writeBytes(sock_fd, EDFPacket, EDFLen, &ob);
	writeString(sock_fd, "\n", &ob);
	fflush(stdout);
	writeEDF(1, &forNC);
	rprintf("There are %f samples per second\n", getSamplesPerSecond(&forNC, 0));
	chunksize = getDataRecordChunkSize(&cfg);

	printf("The chunk size is %d\n", chunksize);
	printf("The header size is %d\n", cfg.hdr.headerRecordBytes);
	edfi = newEDFInputIterator(&cfg);
	t0 = pctimer();
	i = 0;
	if (opts.isMaxSpeed)
		speedup = SPEEDUPFACTOR;
	for (;;) {
		int retval, j;
		short samples[MAXCHANNELS], tosend[MAXCHANNELS];
		retval = fetchSamples(edfi, samples, fp);
		if (retval != 0) break;
		for (j = 0; j < forNC.hdr.dataRecordChannels; ++j)
			tosend[j] = samples[wantedChannels[j]];
		handleSamples(sock_fd, i%64, forNC.hdr.dataRecordChannels, tosend);
		while (t0 + (double) i / (getSamplesPerSecond(&forNC, 0) * speedup) > pctimer()) {
				rsleep(20);
			}
		i += 1;
		if (i % 1000 == 0) {
			printf("Read %d timesamples and on datarecord %05d:%04d\n", i, edfi->dataRecordNum, edfi->sampleNum);
			fflush(stdout);
		}
		stepEDFInputIterator(edfi);
		getResponseCode(sock_fd, &ib);
	}

	fclose(fp);
	freeEDFInputIterator(edfi);
	return 0;
}
