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

#ifndef _WIN32
  #define closesocket(s) close(s)
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

#ifdef _MSC_VER
  // VS2012 and up has no ssize_t defined, before it was defined as unsigned int
  #ifndef _SSIZE_T_DEFINED
    #define _SSIZE_T_DEFINED
    #undef ssize_t
	#ifdef _WIN64
      typedef signed __int64  ssize_t;
    #else
      typedef signed int      ssize_t;
    #endif
  #endif
#endif

#endif // meshlink_compat_h
