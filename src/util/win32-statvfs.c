/*
  Copyright (C) 2022 Ángel Ruiz Fernández

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

#include <sigutils/util/win32-statvfs.h>

#include <stdint.h>
#include <windows.h>

/* A bit adhoc */
int
statvfs(const char *restrict path, struct statvfs *restrict buf)
{
  DWORD SectorsPerCluster = 0;
  DWORD BytesPerSector = 0;
  DWORD NumberOfFreeClusters = 0;
  DWORD TotalNumberOfClusters = 0;
  int r = GetDiskFreeSpaceA(
      path,
      &SectorsPerCluster,
      &BytesPerSector,
      &NumberOfFreeClusters,
      &TotalNumberOfClusters);

  buf->f_frsize = BytesPerSector * SectorsPerCluster;
  buf->f_bsize = buf->f_frsize;

  buf->f_blocks = TotalNumberOfClusters;
  buf->f_bavail = NumberOfFreeClusters;

  return r;
}
