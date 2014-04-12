/*
    logger.c -- logging code
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"

#include "conf.h"
#include "meta.h"
#include "logger.h"
#include "connection.h"
#include "sptps.h"

debug_t debug_level = DEBUG_NOTHING;
static logmode_t logmode = LOGMODE_STDERR;
static pid_t logpid;
#ifdef HAVE_MINGW
static HANDLE loghandle = NULL;
#endif
static const char *logident = NULL;

static void real_logger(int level, int priority, const char *message) {
	char timestr[32] = "";
	static bool suppress = false;

	// Bail out early if there is nothing to do.
	if(suppress)
		return;

	if(level > debug_level || logmode == LOGMODE_NULL)
		return;

	if(level <= debug_level) {
		switch(logmode) {
			case LOGMODE_STDERR:
				fprintf(stderr, "%s\n", message);
				fflush(stderr);
				break;
			case LOGMODE_SYSLOG:
#ifdef HAVE_MINGW
				{
					const char *messages[] = {message};
					ReportEvent(loghandle, priority, 0, 0, NULL, 1, 0, messages, NULL);
				}
#else
#ifdef HAVE_SYSLOG_H
				syslog(priority, "%s", message);
#endif
#endif
				break;
			case LOGMODE_NULL:
				break;
		}
	}
}

void logger(int level, int priority, const char *format, ...) {
	va_list ap;
	char message[1024] = "";

	va_start(ap, format);
	int len = vsnprintf(message, sizeof message, format, ap);
	va_end(ap);

	if(len > 0 && len < sizeof message && message[len - 1] == '\n')
		message[len - 1] = 0;

	real_logger(level, priority, message);
}

static void sptps_logger(sptps_t *s, int s_errno, const char *format, va_list ap) {
	char message[1024] = "";
	int len = vsnprintf(message, sizeof message, format, ap);
	if(len > 0 && len < sizeof message && message[len - 1] == '\n')
		message[len - 1] = 0;

	real_logger(DEBUG_ALWAYS, LOG_ERR, message);
}

void openlogger(const char *ident, logmode_t mode) {
	logident = ident;
	logmode = mode;

	switch(mode) {
		case LOGMODE_STDERR:
			logpid = getpid();
			break;
		case LOGMODE_SYSLOG:
#ifdef HAVE_MINGW
			loghandle = RegisterEventSource(NULL, logident);
			if(!loghandle) {
				fprintf(stderr, "Could not open log handle!");
				logmode = LOGMODE_NULL;
			}
			break;
#else
#ifdef HAVE_SYSLOG_H
			openlog(logident, LOG_CONS | LOG_PID, LOG_DAEMON);
			break;
#endif
#endif
		case LOGMODE_NULL:
			break;
	}

	if(logmode != LOGMODE_NULL)
		sptps_log = sptps_logger;
	else
		sptps_log = sptps_log_quiet;
}

void closelogger(void) {
	switch(logmode) {
		case LOGMODE_SYSLOG:
#ifdef HAVE_MINGW
			DeregisterEventSource(loghandle);
			break;
#else
#ifdef HAVE_SYSLOG_H
			closelog();
			break;
#endif
#endif
		case LOGMODE_NULL:
		case LOGMODE_STDERR:
			break;
			break;
	}
}
