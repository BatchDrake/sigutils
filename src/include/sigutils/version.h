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

#ifndef _SIGUTILS_VERSION
#define _SIGUTILS_VERSION

#include <sigutils/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Sigutils API uses semantic versioning (see https://semver.org/).
 *
 * Sigutils ABI follows the same strategy as Mongocxx (see
 * http://mongocxx.org/mongocxx-v3/api-abi-versioning/): ABI is a simple
 * scalar that is bumped on an incompatible ABI change (e.g. a function
 * becomes a macro or vice-versa). ABI additions DO NOT increment
 * the ABI number.
 */

/* API version macros */
#define SIGUTILS_VERSION_MAJOR 0
#define SIGUTILS_VERSION_MINOR 3
#define SIGUTILS_VERSION_PATCH 0

/* ABI version macros */
#define SIGUTILS_ABI_VERSION 1

/* Utility macros */
#define __SU_VN(num, shift) ((uint32_t)((uint8_t)(num)) << (shift))

#define SU_VER(major, minor, patch) \
  (__SU_VN(major, 16) | __SU_VN(minor, 8) | __SU_VN(patch, 0))

#define SIGUTILS_VERSION \
  SU_VER(SIGUTILS_VERSION_MAJOR, SIGUTILS_VERSION_MINOR, SIGUTILS_VERSION_PATCH)

#define SIGUTILS_API_VERSION \
  SU_VER(SIGUTILS_VERSION_MAJOR, SIGUTILS_VERSION_MINOR, 0)

#define SIGUTILS_VERSION_STRING     \
  STRINGIFY(SIGUTILS_VERSION_MAJOR) \
  "." STRINGIFY(SIGUTILS_VERSION_MINOR) "." STRINGIFY(SIGUTILS_VERSION_PATCH)

unsigned int sigutils_abi_version(void);
const char *sigutils_api_version(void);
const char *sigutils_pkgversion(void);

void sigutils_abi_check(unsigned int);

#define SIGUTILS_ABI_CHECK() sigutils_abi_check(SIGUTILS_ABI_VERSION)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SIGUTILS_VERSION */
