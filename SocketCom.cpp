/*
SocketCom

Copyright (c) 2017 r01hee

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include "SocketCom.h"

#ifdef _SOCKETCOM_WIN32_

#include <MSTcpIp.h>
#include <ws2tcpip.h>
#pragma comment (lib, "ws2_32.lib")

#define errno WSAGetLastError()
#define SOCKETCOM_EINTR WSAEINTR
#define SOCKETCOM_EINPROGRESS  WSAEINPROGRESS

#else

#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#define SOCKETCOM_EINTR EINTR
#define SOCKETCOM_EINPROGRESS  EINPROGRESS

#endif

#include <string.h>

#ifdef SOCKETCOM_NDEBUG
#define PERROR(str) do{}while(0)
#else
#define PERROR(str) perror(str)
#endif

int SocketCom_ResolveHostname(const char *hostname, char *ip_str)
{
  int res;
  struct addrinfo hints, *resolved_addr;
  struct in_addr addr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;

  addr.s_addr = inet_addr(hostname);
  if (addr.s_addr != INADDR_NONE) {
    strcpy(ip_str, hostname);
    return SOCKETCOM_SUCCESS;
  }

  res = getaddrinfo(hostname, NULL, &hints, &resolved_addr);
  if (res != 0) {
    return SOCKETCOM_ERROR_SLAVEHOSTNAME;
  }

  addr.s_addr = ((struct sockaddr_in *)(resolved_addr->ai_addr))->sin_addr.s_addr;
  strcpy(ip_str, inet_ntoa(addr));
  freeaddrinfo(resolved_addr);

  return SOCKETCOM_SUCCESS;
}

#ifdef SOCKETCOM_USE_SETKEEPALIVE
int SocketCom_SetKeepAlive(const SocketCom *sock, int idle, int interval, int count)
{
  int res;

#ifdef _SOCKETCOM_WIN32_
  DWORD dwBytesRet=0;
  struct tcp_keepalive alive;
  alive.onoff = TRUE;
  alive.keepalivetime = idle * 1000;
  alive.keepaliveinterval = interval * 1000;
  // cannot set KEEPCNT option on windows
  res = WSAIoctl(sock->fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, &dwBytesRet, NULL, NULL);
  if (res != 0) {
    PERROR("WSAIoctrl() in SocketCom_SetKeepAlive()");
    return SOCKETCOM_ERROR_SETKEEPALIVE;
  }
#else
  int option = 1;
  res = setsockopt(sock->fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&option, sizeof(option) );
  if (res != 0) {
    PERROR("setsockopt(SO_KEEPALIVE) in SocketCom_SetKeepAlive()");
    return SOCKETCOM_ERROR_SETKEEPALIVE;
  }
  option = idle;
  res = setsockopt(sock->fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&option, sizeof(option) );
  if (res != 0) {
    PERROR("setsockopt(TCP_KEEPIDLE) in SocketCom_SetKeepAlive()");
    return SOCKETCOM_ERROR_SETKEEPALIVE;
  }
  option = interval;
  res = setsockopt(sock->fd, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&option, sizeof(option) );
  if (res != 0) {
    PERROR("setsockopt(TCP_KEEPINTVL) in SocketCom_SetKeepAlive()");
    return SOCKETCOM_ERROR_SETKEEPALIVE;
  }
  option = count;
  res = setsockopt(sock->fd, IPPROTO_TCP, TCP_KEEPCNT, (void*)&option, sizeof(option) );
  if (res != 0) {
    PERROR("setsockopt(TCP_KEEPCNT) in SocketCom_SetKeepAlive()");
    return SOCKETCOM_ERROR_SETKEEPALIVE;
  }
#endif

  return SOCKETCOM_SUCCESS;
}
#endif

int SocketCom_SetReuseaddr(SocketCom *sock)
{
  int res;
#ifdef _SOCKETCOM_WIN32_
  const char on = 1;
#else
  int on = 1;
#endif

  res = setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (res < 0) {
    PERROR("SocketCom_SetReuseaddr()");
    return SOCKETCOM_ERROR_SETREUSEADDR;
  }

  return SOCKETCOM_SUCCESS;
}

int SocketCom_Startup(void)
{
#ifdef _SOCKETCOM_WIN32_
  WSADATA wsaData;

  int res = WSAStartup(MAKEWORD(2,0), &wsaData);
  if (res != 0) {
    return SOCKETCOM_ERROR_STARTUP;
  }
#endif

  return SOCKETCOM_SUCCESS;
}

void SocketCom_Init(SocketCom *sock)
{
  sock->status = SOCKETCOM_STATE_NONE;
}

int SocketCom_Create(SocketCom* sock)
{
  if (sock->status & SOCKETCOM_STATE_CREATED) {
    return SOCKETCOM_ERROR_ALREADY_CREATED;
  }

  sock->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock->fd == SOCKET_ERROR) {
    PERROR("SocketCom_Create()");
    return SOCKETCOM_ERROR_CREATE;
  }

  sock->status |= SOCKETCOM_STATE_CREATED;
  return SOCKETCOM_SUCCESS;
}

int SocketCom_Dispose(SocketCom* sock)
{
  if (!(sock->status & SOCKETCOM_STATE_CREATED)) {
    return SOCKETCOM_ERROR_ALREADY_CLOSED;
  }

  int res;
#ifdef _SOCKETCOM_WIN32_
  res = closesocket(sock->fd);
#else
  res = close(sock->fd);
#endif
  if (res < 0) {
    PERROR("SocketCom_Dispose()");
    return SOCKETCOM_ERROR_CLOSE;
  }

  sock->status = SOCKETCOM_STATE_HAS_ADDR;
  return SOCKETCOM_SUCCESS;
}

int SocketCom_Close(SocketCom* sock)
{
  if (!(sock->status & SOCKETCOM_STATE_CREATED)) {
    return SOCKETCOM_ERROR_ALREADY_CLOSED;
  }

  int res;
#ifdef _SOCKETCOM_WIN32_
  res = shutdown(sock->fd, SD_SEND);
#else
  res = shutdown(sock->fd, SHUT_WR);
#endif
  if (res < 0) {
    PERROR("SocketCom_Close()");
    return SOCKETCOM_ERROR_CLOSE;
  }

  char tmp;
  while (1) {
    res = SocketCom_Recv(sock, &tmp, sizeof(tmp), NULL);
    if (res == SOCKETCOM_ERROR_DISCONNECTED) {
      break;
    }
    if (res != SOCKETCOM_SUCCESS) {
      return res;
    }
  }

  return SocketCom_Dispose(sock);
}

int SocketCom_Listen(SocketCom* sock, u_short port)
{
  int res;

  if (!(sock->status & SOCKETCOM_STATE_CREATED)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }

  memset(&sock->addr, 0, sizeof(struct sockaddr_in));
  sock->addr.sin_family = AF_INET;
  sock->addr.sin_port = htons(port);
  sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  res = bind(sock->fd, (struct sockaddr *)&sock->addr, sizeof(struct sockaddr_in));
  if (res < 0) {
    PERROR("bind() in SocketCom_Listen()");
    return SOCKETCOM_ERROR_BIND;
  }

  res = listen(sock->fd, 5);
  if (res < 0) {
    PERROR("listen() in SocketCom_Listen()");
    return SOCKETCOM_ERROR_LISTEN;
  }

  sock->status |= SOCKETCOM_STATE_SERVER;

  return SOCKETCOM_SUCCESS;
}

int SocketCom_Accept(SocketCom* sock, SocketCom *connectedSock)
{
  if (!(sock->status & SOCKETCOM_STATE_SERVER)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }

  socklen_t addrlen = sizeof(struct sockaddr_in);
  connectedSock->fd = accept(sock->fd, (struct sockaddr *)&connectedSock->addr, &addrlen);
  if (connectedSock->fd == SOCKET_ERROR) {
    PERROR("accept() in SocketCom_Accept()");
    return SOCKETCOM_ERROR_ACCEPT;
  }

  connectedSock->status |= SOCKETCOM_STATE_CLIENT | SOCKETCOM_STATE_HAS_ADDR;

  return SOCKETCOM_SUCCESS;
}

int SocketCom_SetAddrin(SocketCom* sock, const sockaddr_in *addr)
{
  if (sock->status & (SOCKETCOM_STATE_SERVER|SOCKETCOM_STATE_CLIENT)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }
  sock->addr = *addr;
  sock->status |= SOCKETCOM_STATE_HAS_ADDR;

  return SOCKETCOM_SUCCESS;
}

int SocketCom_SetAddr(SocketCom* sock, const char *ip, u_short port)
{
  sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);

  return SocketCom_SetAddrin(sock, &addr);
}

int SocketCom_Connect(SocketCom* sock)
{
  int res;
  if (!(sock->status & (SOCKETCOM_STATE_CREATED|SOCKETCOM_STATE_HAS_ADDR))) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }
  res = connect(sock->fd, (struct sockaddr*)&sock->addr, sizeof(struct sockaddr_in));
  if (res != 0) {
    PERROR("SocketCom_Connect()");
    return SOCKETCOM_ERROR_CONNECT;
  }

  sock->status |= SOCKETCOM_STATE_CLIENT;
  return SOCKETCOM_SUCCESS;

}

int SocketCom_ConnectTo(SocketCom* sock, const char *ip, u_short port)
{
  int res;

  res = SocketCom_SetAddr(sock, ip, port);
  if (res != SOCKETCOM_SUCCESS) {
    return res;
  }

  res = SocketCom_Connect(sock);
  if (res != SOCKETCOM_SUCCESS) {
    return res;
  }

  return SOCKETCOM_SUCCESS;
}

int SocketCom_ConnectWithTimeout(SocketCom* sock, unsigned long timeout_msec)
{
  int res;

  if (!(sock->status & (SOCKETCOM_STATE_CREATED|SOCKETCOM_STATE_HAS_ADDR))) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }

#ifdef _SOCKETCOM_WIN32_
  DWORD dwres;
  WSAEVENT hEvent;
  WSANETWORKEVENTS events;

  // create event
  hEvent = WSACreateEvent();
  if (hEvent == WSA_INVALID_EVENT) {
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_CREATEEVENT;
  }

  // to event-type (become non-blocking)
  res = WSAEventSelect(sock->fd, hEvent, FD_CONNECT);
  if (res == SOCKET_ERROR) {
    WSACloseEvent(hEvent);
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_EVENTSELECT;
  }

  // connect
  res = connect(sock->fd, (struct sockaddr*)&sock->addr, sizeof(struct sockaddr_in));
  if (res == SOCKET_ERROR) {
    res = WSAGetLastError();
    if (res != WSAEWOULDBLOCK) {
      WSAEventSelect(sock->fd, NULL, 0);
      WSACloseEvent(hEvent);
      return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_CONNECT;
    }
  }

  // wait for event occurs
  dwres = WSAWaitForMultipleEvents(1, &hEvent, FALSE, timeout_msec, FALSE);
  if (dwres != WSA_WAIT_EVENT_0) {
    //error if dwret is not WSA_WAIT_EVENT_0
    //timeout if dwret is WSA_WAIT_TIMEOUT
    WSAEventSelect(sock->fd, NULL, 0);
    WSACloseEvent(hEvent);
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_WAITFORMULTIPLEEVENTS;
  }

  // what event occurs
  res = WSAEnumNetworkEvents(sock->fd, hEvent, &events);
  if (res == SOCKET_ERROR) {
    WSAEventSelect(sock->fd, NULL, 0);
    WSACloseEvent(hEvent);
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_ENUMNETWORKEVENTS;
  }

  //success if events.iErrorCode[FD_CONNECT_BIT] is 0
  if ( !((events.lNetworkEvents & FD_CONNECT) && events.iErrorCode[FD_CONNECT_BIT] == 0) ) {
    WSAEventSelect(sock->fd, NULL, 0);
    WSACloseEvent(hEvent);
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_EVENTERROR;
  }

  // close event
  WSAEventSelect(sock->fd, NULL, 0);
  WSACloseEvent(hEvent);

  // restore to blocking socket
  u_long flag = 0;
  if(ioctlsocket(sock->fd, FIONBIO, &flag) == SOCKET_ERROR) {
    return SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_IOCTL;
  }

  sock->status |= SOCKETCOM_STATE_CLIENT;
  return SOCKETCOM_SUCCESS;

#else

  int error;
  socklen_t error_len;

  // set socket nonblocking
  res = fcntl(sock->fd, F_SETFL, O_NONBLOCK);
  if(res < 0) {
    PERROR("fcntl set nonbolocking in SocketCom_ConnectWithTimeout()");
    return SOCKETCOM_ERROR_FCNTL;
  }

  // connect with timeout
  res = connect(sock->fd, (struct sockaddr*)&sock->addr, sizeof(struct sockaddr_in));
  if (res < 0) {
    if (errno != SOCKETCOM_EINPROGRESS) {
      PERROR("connect in SocketCom_ConnectWithTimeout()");
      return SOCKETCOM_ERROR_CONNECT;
    }

    struct timeval tv;
    tv.tv_sec = timeout_msec / 1000;
    tv.tv_usec = (timeout_msec * 1000) - (tv.tv_sec * 1000 * 1000);

    while (1) {
      fd_set set;
      FD_ZERO(&set);
      FD_SET(sock->fd , &set);
      res = select(sock->fd+1, NULL, &set, NULL, &tv);
      if (res <= 0 || !FD_ISSET(sock->fd, &set)) {
        if (errno == SOCKETCOM_EINTR) {
          //retry
          continue;
        }
        // error or timeout
        PERROR("select in SocketCom_ConnectWithTimeout()");
        return SOCKETCOM_ERROR_SELECT;
      }

      error_len = sizeof(error);
      if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &error, &error_len) != 0) {
        PERROR("getsockopt in SocketCom_ConnectWithTimeout()");
        return SOCKETCOM_ERROR_GETSOCKOPT;
      }
      if (error != 0) {
        errno = error;
        PERROR("SO_ERROR in SocketCom_ConnectWithTimeout()");
        return SOCKETCOM_ERROR_CONNECT;
      }

      break;
    }
  }

  // set blocking socket
  res = fcntl(sock->fd, F_SETFL, 0);
  if(res < 0) {
    PERROR("fcntl set bolocking in SocketCom_ConnectWithTimeout()");
    return SOCKETCOM_ERROR_FCNTL;
  }

  sock->status |= SOCKETCOM_STATE_CLIENT;
  return SOCKETCOM_SUCCESS;
#endif
}

int SocketCom_ConnectToWithTimeout(SocketCom* sock, const char *ip, u_short port, unsigned long timeout_msec)
{
  int res;

  res = SocketCom_SetAddr(sock, ip, port);
  if (res != SOCKETCOM_SUCCESS) {
    return res;
  }

  return SocketCom_ConnectWithTimeout(sock, timeout_msec);
}

int SocketCom_RecvExWithTimeout(SocketCom* sock, void *buf, int bufLen, int *recvLen, int flags, long timeout_sec, long timeout_usec)
{
  int res;
  res	= SocketCom_WaitForRecvableWithTimeout(sock, timeout_sec, timeout_usec);
  if (res != SOCKETCOM_SUCCESS) {
    return res;
  }

  return  SocketCom_RecvEx(sock, buf, bufLen, recvLen, flags);
}

int SocketCom_RecvWithTimeout(SocketCom* sock, void *buf, int bufLen, int *recvLen, long timeout_sec, long timeout_usec)
{
  return SocketCom_RecvExWithTimeout(sock, buf, bufLen, recvLen, 0, timeout_sec, timeout_usec);
}

int SocketCom_WaitForRecvablesWithTimeout(SocketCom* socks[], int* socks_len, long timeout_sec, long timeout_usec)
{
  int res;
  int i;
  struct timeval tv;

#ifdef _SOCKETCOM_WIN32_
  SOCKET fd_max;
#else
  int fd_max;
#endif

  fd_max = socks[0]->fd;
  fd_set fds;
  FD_ZERO(&fds);
  for (i = 0; i < *socks_len; i++) {
    FD_SET(socks[i]->fd, &fds);
    if (socks[i]->fd > fd_max) {
      fd_max = socks[i]->fd;
    }
  }

  tv.tv_sec = timeout_sec;
  tv.tv_usec = timeout_usec;

  while (1) {
    if (timeout_sec >= 0 && timeout_usec >= 0) {
      res = select(fd_max+1, &fds, NULL, NULL, &tv);
    } else {
      res = select(fd_max+1, &fds, NULL, NULL, NULL);
    }
    if (res > 0) { //success
      break;
    }
    if (res == 0) {
      return SOCKETCOM_ERROR_TIMEOUT_SELECT;
    }
    if (errno == SOCKETCOM_EINTR) {
      continue;
    }
    // error
    return SOCKETCOM_ERROR_SELECT;
  }

  int count = 0;
  for (i = 0; i < *socks_len; i++) {
    if (FD_ISSET(socks[i]->fd, &fds)) {
      if (count != i) {
        SocketCom *tmp;
        tmp = socks[count];
        socks[count] = socks[i];
        socks[i] = tmp;
      }
      count++;
    }
  }

  *socks_len = res;

  return SOCKETCOM_SUCCESS;
}

int SocketCom_SelectRecvables(SocketCom* socks[], int* socks_len)
{
  return SocketCom_WaitForRecvablesWithTimeout(socks, socks_len, 0, 0);
}

int SocketCom_WaitForRecvables(SocketCom* socks[], int* socks_len)
{
  return SocketCom_WaitForRecvablesWithTimeout(socks, socks_len, -1, -1);
}

int SocketCom_WaitForRecvableWithTimeout(SocketCom* sock, long timeout_sec, long timeout_usec)
{
  int socks_len = 1;
  return SocketCom_WaitForRecvablesWithTimeout(&sock, &socks_len, timeout_sec, timeout_usec);
}

int SocketCom_WaitForRecvable(SocketCom* sock)
{
  return SocketCom_WaitForRecvableWithTimeout(sock, -1, -1);
}

int SocketCom_IsRecvable(SocketCom* sock)
{
  int res = SocketCom_WaitForRecvableWithTimeout(sock, 0, 0);
  return (res == SOCKETCOM_SUCCESS) ? 1 : 0;
}

int SocketCom_RecvEx(SocketCom* sock, void* buf, int bufLen, int *recvLen, int flags)
{
  int _recvLen;

  _recvLen = recv(sock->fd, (char *)buf, bufLen, flags);
  if (_recvLen == 0) {
    return SOCKETCOM_ERROR_DISCONNECTED;
  } else if (_recvLen < 0) {
    PERROR("SocketCom_RecvEx()");
    return SOCKETCOM_ERROR_RECV;
  }

  if (recvLen != NULL) {
    *recvLen = _recvLen;
  }
  return SOCKETCOM_SUCCESS;
}

int SocketCom_Recv(SocketCom* sock, void* buf, int bufLen, int *recvLen)
{
  return SocketCom_RecvEx(sock, buf, bufLen, recvLen, 0);
}

int SocketCom_RecvAll(SocketCom* sock, void* buf, int recvLen)
{
  return SocketCom_RecvEx(sock, buf, recvLen, NULL, MSG_WAITALL);
}

int SocketCom_Send(SocketCom* sock, const void *buf, int bufLen)
{
  int size;

  size = send(sock->fd, (const char *)buf, bufLen, 0);
  if (size != bufLen) {
    PERROR("SocketCom_Send()");
    return SOCKETCOM_ERROR_SEND;
  }

  return SOCKETCOM_SUCCESS;
}

int inline SocketCom_IsClient(const SocketCom* sock)
{
  return (sock->status & SOCKETCOM_STATE_CLIENT)?1:0;
}

int inline SocketCom_IsServer(const SocketCom* sock)
{
  return (sock->status & SOCKETCOM_STATE_SERVER)?1:0;
}

int SocketCom_HasAddr(const SocketCom* sock)
{
  return sock->status & SOCKETCOM_STATE_HAS_ADDR;
}

int inline SocketCom_IsReady(const SocketCom* sock)
{
  return (sock->status & (SOCKETCOM_STATE_CLIENT|SOCKETCOM_STATE_SERVER))?1:0;
}

int SocketCom_GetAddrin(const SocketCom *sock, sockaddr_in *addrin)
{
  if (!(sock->status & SOCKETCOM_STATE_HAS_ADDR)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }

  *addrin = sock->addr;

  return SOCKETCOM_SUCCESS;
}

int SocketCom_GetPort(const SocketCom *sock, u_short *port)
{
  if (!(sock->status & SOCKETCOM_STATE_HAS_ADDR)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }
  *port = ntohs(sock->addr.sin_port);

  return SOCKETCOM_SUCCESS;
}

int SocketCom_GetIpStr(SocketCom* sock, char* ipstr)
{
  if (!(sock->status & SOCKETCOM_STATE_HAS_ADDR)) {
    return SOCKETCOM_ERROR_ILLEGAL_SOCK;
  }

  strcpy(ipstr, inet_ntoa(sock->addr.sin_addr));

  return SOCKETCOM_SUCCESS;
}

int SocketCom_SetBlockingSocket(SocketCom* sock)
{
  int res;
#ifdef _SOCKETCOM_WIN32_
  u_long val = 0;
  res = ioctlsocket(sock->fd, FIONBIO, &val);
#else
  int val = 0;
  res = ioctl(sock->fd, FIONBIO, &val);
#endif
  if (res != 0) {
    PERROR("ioctl set bolocking");
    return SOCKETCOM_ERROR_FCNTL;
  }
  return SOCKETCOM_SUCCESS;
}

int SocketCom_SetNonBlockingSocket(SocketCom* sock)
{
  int res;
#ifdef _SOCKETCOM_WIN32_
  u_long val = 1;
  res = ioctlsocket(sock->fd, FIONBIO, &val);
#else
  int val = 1;
  res = ioctl(sock->fd, FIONBIO, &val);
#endif
  if (res != 0) {
    PERROR("ioctl set nonbolocking");
    return SOCKETCOM_ERROR_FCNTL;
  }
  return SOCKETCOM_SUCCESS;
}

int SocketCom_Equal(const SocketCom* sock1, const SocketCom* sock2)
{
  return sock1->fd == sock2->fd;
}
