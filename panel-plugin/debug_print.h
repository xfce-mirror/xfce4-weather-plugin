#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

#ifdef DEBUG
  #include <stdio.h>
  #define DEBUG_PRINT(s, i) fprintf(stderr, s, i);
#else
  #define DEBUG_PRINT(s, i) do {} while(0);
#endif

#endif
