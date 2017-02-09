#ifndef meshlink_internal_wincompat_h
#define meshlink_internal_wincompat_h

// Windows lacks pipe. It has an equivalent CreatePipe but we really need
// something to give to WSAPoll, so we fake it with a local TCP socket. (ugh)
int meshlink_pipe(int pipefd[2]);

// pipe(socket)-specific read/write/close equivalents
#define meshlink_closepipe closesocket
#define meshlink_writepipe(s,buf,len) send(s, buf, len, 0)
#define meshlink_readpipe(s,buf,len) recv(s, buf, len, 0)

#include <winsock2.h>
// Windows lacks poll, but WSAPoll is good enough for us.
#define poll(fds, nfds, timeout) WSAPoll(fds, nfds, timeout)

#endif // meshlink_internal_wincompat_h
