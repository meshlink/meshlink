#ifndef meshlink_wincompat_h
#define meshlink_wincompat_h

// This file and its companion wincompat.c provide some Posix interfaces to
// Windows APIs so the rest of the code can keep using them.

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

#endif // meshlink_wincompat_h
