/*
    run_blackbox_tests.c -- Implementation of Black Box Test Execution for meshlink

    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include "execute_tests.h"
#include "test_cases_channel_conn.h"
#include "../common/containers.h"
#include "../common/common_handlers.h"

#define CMD_LINE_ARG_MESHLINK_ROOT_PATH 1
#define CMD_LINE_ARG_LXC_PATH 2
#define CMD_LINE_ARG_LXC_BRIDGE_NAME 3
#define CMD_LINE_ARG_ETH_IF_NAME 4
#define CMD_LINE_ARG_CHOOSE_ARCH 5

char *meshlink_root_path = NULL;
char *choose_arch = NULL;
int total_tests;

int main(int argc, char *argv[]) {
  /* Set configuration */
  assert(argc >= (CMD_LINE_ARG_CHOOSE_ARCH + 1));
  meshlink_root_path = argv[CMD_LINE_ARG_MESHLINK_ROOT_PATH];
  lxc_path = argv[CMD_LINE_ARG_LXC_PATH];
  lxc_bridge = argv[CMD_LINE_ARG_LXC_BRIDGE_NAME];
  eth_if_name = argv[CMD_LINE_ARG_ETH_IF_NAME];
  choose_arch = argv[CMD_LINE_ARG_CHOOSE_ARCH];

  int failed_tests = 0;

  failed_tests += test_case_channel_conn();

  printf("[ PASSED ] %d test(s).\n", total_tests - failed_tests);
  printf("[ FAILED ] %d test(s).\n", failed_tests);

  return failed_tests;
}
