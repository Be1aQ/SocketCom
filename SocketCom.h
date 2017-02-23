/*
SocketCom

Copyright (c) 2017 r01hee

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#ifndef __SOCKETCOM_H__
#define __SOCKETCOM_H__

//#define SOCKETCOM_USE_SETKEEPALIVE
#define SOCKETCOM_NDEBUG

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#define _SOCKETCOM_WIN32_
#else
#define _SOCKETCOM_POSIX_
#endif

#ifdef _SOCKETCOM_WIN32_

#include <winsock2.h>

typedef int socklen_t;
#else

#include <netinet/in.h>

#define SOCKET_ERROR (-1)

#endif


enum SOCKETCOM_STATE {
  SOCKETCOM_STATE_NONE = 0x00,
  SOCKETCOM_STATE_CREATED = 0x01,
  SOCKETCOM_STATE_HAS_ADDR = 0x02,
  SOCKETCOM_STATE_SERVER = 0x04,
  SOCKETCOM_STATE_CLIENT = 0x08,
};


enum SOCKETCOM_ERROR {
  SOCKETCOM_SUCCESS = 0,
  SOCKETCOM_ERROR_TIMEOUT = 1,
  SOCKETCOM_ERROR_WOULDBLOCK = 2,

  SOCKETCOM_ERROR_ILLEGAL_SOCK = 4,
  SOCKETCOM_ERROR_ALREADY_CREATED = 8, 
  SOCKETCOM_ERROR_CREATE = 12,
  SOCKETCOM_ERROR_ALREADY_CLOSED = 16,
  SOCKETCOM_ERROR_CLOSE = 20,
  SOCKETCOM_ERROR_BIND = 24,
  SOCKETCOM_ERROR_LISTEN = 28,
  SOCKETCOM_ERROR_ACCEPT = 32,
  SOCKETCOM_ERROR_CONNECT = 36,
  SOCKETCOM_ERROR_TIMEOUT_CONNECT = 37, // SOCKETCOM_ERROR_CONNECT | SOCKETCOM_ERROR_TIMEOUT
  SOCKETCOM_ERROR_DISCONNECTED = 40,
  SOCKETCOM_ERROR_RECV = 44,
  SOCKETCOM_ERROR_SEND = 48,
  SOCKETCOM_ERROR_SETKEEPALIVE = 52,
  SOCKETCOM_ERROR_SETREUSEADDR = 56,

  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_CREATEEVENT = 60,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_EVENTSELECT = 64,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_CONNECT = 68,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_WAITFORMULTIPLEEVENTS = 72,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_ENUMNETWORKEVENTS = 76,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_EVENTERROR = 80,
  SOCKETCOM_ERROR_CONNECTWITHTIMEOUT_IOCTL = 84,

  SOCKETCOM_ERROR_SELECT = 88,
  SOCKETCOM_ERROR_TIMEOUT_SELECT = 89, //SOCKETCOM_ERROR_SELECT | SOCKETCOM_ERROR_TIMEOUT

  SOCKETCOM_ERROR_SLAVEHOSTNAME = 92,

  SOCKETCOM_ERROR_STARTUP = 96,

  SOCKETCOM_ERROR_FCNTL = 100,

  SOCKETCOM_ERROR_IOCTL = 104,

  SOCKETCOM_ERROR_GETSOCKOPT = 108,
};

#define SOCKETCOM_IPV4_STR_SIZE 16

#define SOCKETCOM_INITIALIZER {0}

/**
 *  @attention  before to use struct SocketCom, initialize by (1) SocketCom sock = SOCKETCOM_INITIALIZER; or (2) SocketCom_Init(&sock);
 */
typedef struct SocketCom {
  int status;
#ifdef _SOCKETCOM_WIN32_
  SOCKET fd;
#else
  int fd;
#endif
  sockaddr_in addr;
} SocketCom;

/**
 * start up SocketCom
 * at the beginning of the program, call this function once
 *
 * @retval SOCKETCOM_SUCCESS success
 * @retval !=SOCKETCOM_SUCCESS error
 * @retval SOCKETCOM_ERROR_STARTUP error on WSAStartup; this error occur on only WIN32
 */
int SocketCom_Startup(void);

#ifdef SOCKETCOM_USE_SETKEEPALIVE
/**
 *  set Keep Alive
 *
 *  @param[in] sock sock
 *  @param[in] idle The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes
 *  @param[in] interval The time (in seconds) between individual keepalive probes
 *  @param[in] count The maximum number of keepalive probes TCP should send before dropping the connection
 *  @retval SOCKETCOM_SUCCESS success
 *  @retval !=SOCKETCOM_SUCCESS error
 *
 *  @attention 'count' is ignored on WIN32
 */
int SocketCom_SetKeepAlive(SocketCom *sock,int idle,int interval,int count);
#endif

/**
 *  initialize struct SocketCom
 */
void SocketCom_Init(SocketCom *sock);

/**
 * create socket
 *
 * @param sock[in/out] sock
 *
 * @retval ==SOCKETCOM_SUCCESS success
 * @retval !=SOCKETCOM_SUCCESS error
 */
int SocketCom_Create(SocketCom *sock);

/**
 *  close connection after checking FIN and clearing buffer
 *
 *  @param sock[in/out] closed sock
 *
 *  @retval 0 success
 *  @retval !=0 error
 */
int SocketCom_Close(SocketCom* sock);

/**
 *  Close(Dispose) connection immediately
 *
 *  @param sock[in/out] closed sock
 *
 *  @retval ==SOCKETCOM_SUCCESS success
 *  @retval !=SOCKETCOM_SUCCESS error
 */
int SocketCom_Dispose(SocketCom* sock);

int SocketCom_Listen(SocketCom *sock,u_short port);
int SocketCom_Accept(SocketCom *sock,SocketCom *connectedSock);
int SocketCom_SetAddrin(SocketCom* sock, const sockaddr_in *addr);
int SocketCom_SetAddr(SocketCom* sock, const char *ip, u_short port);

/**
 * set flag ON SO_REUSEADDR
 *
 *  @param[in] sock sock
 */
int SocketCom_SetReuseaddr(SocketCom *sock);

int SocketCom_Connect(SocketCom *sock);

int SocketCom_ConnectTo(SocketCom *sock, const char *ip, u_short port);

/**
 *  this function connects the sock to the specified address by SetAddr() or SetAddrin()
 *  but if it cannot connect and time-out, it returns
 *
 *  @param[in/out] sock sock
 *  @param[in] timeout_msec timeout(in milli second)
 *  @retval SOCKETCOM_SUCCESS success
 *  @retval !=SOCKETCOM_SUCESS error or timeout
 */
int SocketCom_ConnectWithTimeout(SocketCom *sock, unsigned long timeout_msec);

/**
 *  this function connects the sock to the specified address
 *  but if it cannot connect and time-out, it returns
 *
 *  @param[in/out] sock sock
 *  @param[in] ipaddr IP address to connect to
 *  @param[in] port port to connect to
 *  @param[in] timeout_msec timeout(in milli second)
 *  @retval SOCKETCOM_SUCCESS success
 *  @retval !=SOCKETCOM_SUCESS error or timeout
 */
int SocketCom_ConnectToWithTimeout(SocketCom* sock, const char *ip, u_short port, unsigned long timeout_msec);

int SocketCom_RecvExWithTimeout(SocketCom *sock,void *buf,int bufLen,int *recvLen,int flags,long timeout_sec,long timeout_usec);
int SocketCom_RecvWithTimeout(SocketCom *sock,void *buf,int bufLen,int *recvLen,long timeout_sec,long timeout_usec);
int SocketCom_WaitForRecvablesWithTimeout(SocketCom *socks[],int *socks_len,long timeout_sec,long timeout_usec);

/**
 *  select sockets which are receivable
 *  this function select sockets readying to receive (non-blocking fucntion)
 *
 *  @socks[in/out] give list of socks, return list of only socks which are possible to recv
 *  @socks_len[in/out] give length of socks, return length of socks which are possible to recv
 */
int SocketCom_SelectRecvables(SocketCom *socks[],int *socks_len);
int SocketCom_WaitForRecvables(SocketCom *socks[],int *socks_len);

/**
 *  select sockets readying to receive, unless timeout
 *  this function is blocking in default
 *
 *  if give timeout_sec = 0 and timeout_usec = 0, non blockging(non wait); same as SocketCom_SelectRecvable()
 *  if give timeout_sec = -1 or timeout_usec = -1, unlimited timeout(forever, wait until at least one socket receivable); same as SocketCom_SelectRecvable()
 *
 *  @socks[in/out] give list(array) of socks, return list of only socks which are possible to recv
 *  @socks_len[in/out] give length of socks, return length of socks which are possible to recv
 *  @timeout_sec[in] timeout(in second)
 *  @timeout_usec[in] timeout(in micro second)
 */
int SocketCom_WaitForRecvableWithTimeout(SocketCom* sock, long timeout_sec, long timeout_usec);

/**
 *  select sockets which are ready to receive
 *  this function forever wait until at least one socket receivable (blocking function)
 *
 *  @socks[in/out] give list of socks, return list of only socks which are possible to recv
 *  @socks_len[in/out] give length of socks, return length of socks which are possible to recv
 */
int SocketCom_WaitForRecvable(SocketCom *sock);

/**
 *  This function returns true if sock is ready to receive (receive-able=recvable)
 *
 * @param[in] sock sock
 * @retval !=0(true) sock is ready to receive
 * @retval ==0(false) sock is not ready to receive
 */
int SocketCom_IsRecvable(SocketCom *sock);
int SocketCom_RecvEx(SocketCom *sock,void *buf,int bufLen,int *recvLen,int flags);
int SocketCom_Recv(SocketCom *sock,void *buf,int bufLen,int *recvLen);
int SocketCom_RecvAll(SocketCom *sock,void *buf,int recvLen);
int SocketCom_Send(SocketCom *sock,const void *buf,int bufLen);
int SocketCom_IsClient(const SocketCom* sock);
int SocketCom_IsServer(const SocketCom* sock);
int SocketCom_HasAddr(const SocketCom* sock);
int SocketCom_GetAddrin(const SocketCom *sock, sockaddr_in *addrin);
int SocketCom_IsReady(const SocketCom* sock);
int SocketCom_GetPort(const SocketCom *sock, u_short *port);
int SocketCom_GetIpStr(SocketCom *sock,char *ipstr);

/**
 * compare SocketCom
 *
 * @retval true(!=0) equal
 * @retval false(==0) not equal
 */
int SocketCom_Equal(const SocketCom* sock1, const SocketCom* sock2);

/**
 * resolve host name(obtain IP such as 11.22.33.44 from host name such as xxxxx.com)
 * if you pass IP address at hostname, return same IP
 *
 * e.g.1: hostname:"xxxxx.com"    ->  ip_str:"11.22.33.44"
 * e.g.2: hostname:"11.22.33.44"  ->  ip_str:"11.22.33.44"
 *
 * @param[in] hostname host name such as xxxxx.com
 * @param[out] ip_str resolved IP address such as 11.22.33.44
 *
 * @retval SOCKETCOM_SUCCESS success
 * @retval !=SOCKETCOM_SUCCESS error
 * @retval SOCKETCOM_ERROR_SLAVEHOSTNAME error on resolving host name
 *
 * @attension if this optaions multiple IP addresses, returns first one
 */
int SocketCom_ResolveHostname(const char *hostname, char *ip_str);

#endif

