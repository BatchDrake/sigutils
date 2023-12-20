//
// Created by happycactus on 20/06/23.
//

#include <sigutils/sigutils.h>
#include <sigutils/version.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
  su_lib_init();

  printf("Testing sigutils version: %08x ABI: %d\n", SIGUTILS_VERSION, SIGUTILS_ABI_VERSION);

  return 0;
}