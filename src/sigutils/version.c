/*

  Copyright (C) 2020 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <sigutils/version.h>

#ifndef SIGUTILS_PKGVERSION
#  define SIGUTILS_PKGVERSION \
    "custom build on " __DATE__ " at " __TIME__ " (" __VERSION__ ")"
#endif /* SIGUTILS_BUILD_STRING */

unsigned int
sigutils_abi_version(void)
{
  return SIGUTILS_ABI_VERSION;
}

const char *
sigutils_api_version(void)
{
  return SIGUTILS_VERSION_STRING;
}

const char *
sigutils_pkgversion(void)
{
  return SIGUTILS_PKGVERSION;
}

void
sigutils_abi_check(unsigned int abi)
{
  if (abi != SIGUTILS_ABI_VERSION) {
    fprintf(stderr, "*** SIGUTILS CRITICAL LIBRARY ERROR ***\n");
    fprintf(
        stderr,
        "Expected ABI version (v%u) is incompatible with current\n",
        abi);
    fprintf(stderr, "sigutils ABI version (v%u).\n\n", (unsigned) SIGUTILS_ABI_VERSION);

    if (abi < SIGUTILS_ABI_VERSION) {
      fprintf(
          stderr,
          "The current sigutils ABI version is too new compared to\n");
      fprintf(stderr, "the version expected by the user software. Please\n");
      fprintf(stderr, "update your software or rebuild it with an updated\n");
      fprintf(stderr, "version of sigutils' development files\n\n");
    } else {
      fprintf(
          stderr,
          "The current sigutils ABI version is too old compared to\n");
      fprintf(
          stderr,
          "the version expected by the user software. This usually\n");
      fprintf(
          stderr,
          "happens when the user software is installed in an older\n");
      fprintf(
          stderr,
          "system without fixing its dependencies. Please verify\n");
      fprintf(stderr, "your installation and try again.\n");
    }

    abort();
  }
}
