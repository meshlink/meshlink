/*
    tcpdump.c -- Implementation of Black Box Test Execution for meshlink

    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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

#include <unistd.h>
#include <sys/prctl.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include "common_handlers.h"
#include "tcpdump.h"

pid_t tcpdump_start(char *interface) {
	char *argv[] = { "tcpdump", "-i", interface, NULL };
	// child process have a pipe to the parent process when parent process terminates SIGPIPE kills the tcpdump
	int pipes[2];
	assert(pipe(pipes) != -1);
	PRINT_TEST_CASE_MSG("\x1b[32mLaunching TCP Dump ..\x1b[0m\n");

	pid_t tcpdump_pid = fork();

	if(tcpdump_pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGHUP);
		close(pipes[1]);
		// Open log file for TCP Dump
		int fd = open(TCPDUMP_LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		assert(fd != -1);
		close(STDOUT_FILENO);
		assert(dup2(fd, STDOUT_FILENO) != -1);

		// Launch TCPDump with port numbers of sleepy, gateway & relay
		execvp("/usr/sbin/tcpdump", argv);
		perror("execvp ");
		exit(1);
	} else {
		close(pipes[0]);
	}

	return tcpdump_pid;
}

void tcpdump_stop(pid_t tcpdump_pid) {
	PRINT_TEST_CASE_MSG("\n\x1b[32mStopping TCP Dump.\x1b[0m\n");
	assert(!kill(tcpdump_pid, SIGTERM));
}
