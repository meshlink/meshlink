#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <err.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	static const struct option longopts[] = {
		{"verify", 0, NULL, 'v'},
		{"rate", 1, NULL, 'r'},
		{"fps", 1, NULL, 'f'},
		{"total", 1, NULL, 't'},
		{NULL, 0, NULL, 0},
	};

	int opt;
	bool verify = false;
	float rate = 1e6;
	float fps = 60;
	float total = 1.0 / 0.0;

	while((opt = getopt_long(argc, argv, "vr:f:t:", longopts, &optind)) != -1) {
		switch(opt) {
		case 'v':
			verify = true;
			break;

		case 'r':
			rate = atof(optarg);
			break;

		case 'f':
			fps = atof(optarg);
			break;

		case 't':
			total = atof(optarg);
			break;

		default:
			fprintf(stderr, "Usage: %s [-v] [-r bitrate] [-f frames_per_second]\n", argv[0]);
			return 1;
		}
	}

	size_t framesize = rate / fps / 8;
	framesize &= ~0xf;
	long interval = 1e9 / fps;

	if(!framesize || interval <= 0) {
		err(1, "invalid parameters");
	}

	char *buf = malloc(framesize + 16);

	if(!buf) {
		err(1, "malloc(%zu)", framesize);
	}

	uint64_t counter = 0;
	struct timespec now, next = {0};
	clock_gettime(CLOCK_REALTIME, &now);

	while(total > 0) {
		if(!verify) {
			size_t tosend = framesize;
			char *p = buf;

			memcpy(buf, &now, sizeof(now));

			for(uint64_t *q = (uint64_t *)(buf + sizeof(now)); (char *)q < buf + framesize; q++) {
				*q = counter++;
			}

			while(tosend) {
				ssize_t sent = write(1, p, tosend);

				if(sent <= 0) {
					err(1, "write(1, %p, %zu)", p, tosend);
				}

				tosend -= sent;
				p += sent;
			}

			next = now;
			next.tv_nsec += interval;

			while(next.tv_nsec >= 1000000000) {
				next.tv_nsec -= 1000000000;
				next.tv_sec++;
			}

			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
			now = next;
			total -= framesize;
		} else {
			struct timespec *ts = (struct timespec *)buf;
			size_t toread = sizeof(*ts);
			char *p = buf;

			while(toread) {
				ssize_t result = read(0, p, toread);

				if(result <= 0) {
					err(1, "read(1, %p, %zu)", p, toread);
				}

				toread -= result;
				p += result;
			}

			clock_gettime(CLOCK_REALTIME, &now);

			toread = framesize - sizeof(now);

			while(toread) {
				ssize_t result = read(0, p, toread);

				if(result <= 0) {
					err(1, "read(1, %p, %zu)", p, toread);
				}

				toread -= result;
				p += result;
			}

			clock_gettime(CLOCK_REALTIME, &next);

			for(uint64_t *q = (uint64_t *)(buf + sizeof(now)); (char *)q < buf + framesize; q++) {
				if(*q != counter++) {
					uint64_t offset = (counter - 1) * 8;
					offset += ((counter * 8) / (framesize - sizeof(now))) * sizeof(now);
					err(1, "verification failed at offset %lu", offset);
				}
			}

			float dt1 = now.tv_sec - ts->tv_sec + 1e-9 * (now.tv_nsec - ts->tv_nsec);
			float dt2 = next.tv_sec - now.tv_sec + 1e-9 * (next.tv_nsec - now.tv_nsec);

			fprintf(stderr, "\rDelay: %8.3f ms, burst bandwidth: %8.0f Mbit/s", dt1 * 1e3, (framesize - sizeof(now)) / dt2 * 8 / 1e6);

			total -= framesize;
		}
	}

	if(verify) {
		fprintf(stderr, "\n");
	}
}
