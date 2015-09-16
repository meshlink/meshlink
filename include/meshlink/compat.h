#ifndef meshlink_compat_h
#define meshlink_compat_h

#ifndef _MSC_VER
  #include <unistd.h>
#endif

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
#endif

#ifdef _WIN32
  #ifdef MESHLINK_DLLEXPORT
    #define MESHLINK_API __declspec(dllexport)
  #elif MESHLINK_DLLIMPORT
    #define MESHLINK_API __declspec(dllimport)
  #else
    #define MESHLINK_API
  #endif
#else
  #define MESHLINK_API
#endif

#if !defined(PRINT_SIZE_T)
  #ifdef _WIN32
    #define PRINT_SIZE_T "%Iu"
    #define PRINT_SSIZE_T "%Id"
  #else
    #define PRINT_SIZE_T "%zu"
    #define PRINT_SSIZE_T "%zd"
  #endif
#endif

#ifdef _MSC_VER
  // VS2012 and up has no ssize_t defined, before it was defined as unsigned int
  #ifndef _SSIZE_T
    #define _SSIZE_T
    typedef signed int        ssize_t;
  #endif
#endif

// windows doesn't support shared libraries exporting thread local variables,
// only for static linkage it's supported
// #ifdef _MSC_VER
// #define __thread __declspec(thread)
// #endif

#endif // meshlink_compat_h
