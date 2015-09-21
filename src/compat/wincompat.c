#include "wincompat.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <assert.h>

// copied from catta pipe implementation
int meshlink_pipe(int pipefd[2])
{
    int lsock = (int)INVALID_SOCKET;
    struct sockaddr_in laddr;
    socklen_t laddrlen = sizeof(laddr);

    pipefd[0] = pipefd[1] = (int)INVALID_SOCKET;

    // bind a listening socket to a TCP port on localhost
    laddr.sin_family = AF_INET;
    laddr.sin_port = 0;
    laddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if((lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR)
        goto fail;
    if(bind(lsock, (struct sockaddr *)&laddr, sizeof(laddr)) == SOCKET_ERROR)
        goto fail;
    if(listen(lsock, 1) == SOCKET_ERROR)
        goto fail;

    // determine which address (i.e. port) we got bound to
    if(getsockname(lsock, (struct sockaddr *)&laddr, &laddrlen) == SOCKET_ERROR)
        goto fail;
    assert(laddrlen == sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // connect and accept
    if((pipefd[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR)
        goto fail;
    if(connect(pipefd[0], (const struct sockaddr *)&laddr, sizeof(laddr)) == SOCKET_ERROR)
        goto fail;
    if((pipefd[1] = accept(lsock, NULL, NULL)) == SOCKET_ERROR)
        goto fail;

    // close the listener
    closesocket(lsock);

    return 0;

fail:
    closesocket(pipefd[0]);
    closesocket(lsock);
    return -1;
}
