#ifndef meshlink_internal_compat_h
#define meshlink_internal_compat_h

#ifdef _WIN32
  #include "wincompat.h"
#else
  #define meshlink_pipe pipe
  #define meshlink_closepipe close
  #define meshlink_writepipe write
  #define meshlink_readpipe read
#endif

#ifndef PRINT_SIZE_T
  #ifdef _WIN32
    #define PRINT_SIZE_T "%Iu"
    #define PRINT_SSIZE_T "%Id"
  #else
    #define PRINT_SIZE_T "%zu"
    #define PRINT_SSIZE_T "%zd"
  #endif
#endif

#endif // meshlink_internal_compat_h
