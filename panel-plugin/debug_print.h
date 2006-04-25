#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H
#include <libxfce4util/libxfce4util.h>
#ifdef DEBUG
  #include <stdio.h>
  #define DEBUG_PRINT(s, i) fprintf(stderr, s, i);
  #define DEBUG_PUTS(s) fputs(s, stderr)
#else
  #define DEBUG_PRINT(s, i) do {} while(0);
  #define DEBUG_PUTS(s) do {} while(0);

#endif

#endif
