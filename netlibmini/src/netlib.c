#include <netlib.h>

#include <errno.h>
#include <unistd.h>

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /* WINDOWS */

#ifdef UNIX
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif /* UNIX */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef FREEBSD_SENDFILE
#include <sys/types.h>
#include <sys/uio.h>
#endif /* FREEBSD_SENDFILE */

#ifdef DEBUG
#define log_printf(...)                                     \
  printf("%s:%d:%s(): ", __FILE__, __LINE__, __FUNCTION__); \
  printf(__VA_ARGS__);                                      \
  fflush(stdout)
#else
#define log_printf(...)
#endif /* DEBUG */

#ifdef WINDOWS
#define MIN_API 2
#define MAX_API 2

#define Read(f, b, c) recv(f, b, c, 0)
#define Write(f, b, c) send(f, b, c, 0)
#define FileRead read
#define FileWrite write
#else
#define Read read
#define Write write
#define FileRead read
#define FileWrite write
#endif /* WINDOWS */

#define READ_BUFF_SIZE 4096
#define MISC_BUFF_SIZE 256

#ifndef min
#define min(a, b) (a) < (b) ? (a) : (b)
#endif /* min */
#ifndef max
#define max(a, b) (a) > (b) ? (a) : (b)
#endif /* min */

int Initialize() {
#ifdef WINDOWS
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(MIN_API, MAX_API);
  if (WSAStartup(wVersionRequested, &wsaData) == 0) {
    if (LOBYTE(wsaData.wVersion) == MIN_API &&
        HIBYTE(wsaData.wVersion) == MAX_API) {
      log_printf("WinSock Initialized: %d.%d\n", MIN_API, MAX_API);
      return NET_OK;
    }
    WSACleanup();
  }
  return NET_ERROR;
#endif /* WINDOWS */
  log_printf("Sockets Initialized\n");
  return NET_OK;
}

int32 Skipn(fd in, uint32 n) {
  int32 ntotread = 0;
  int32 nread; 
  int8 c[READ_BUFF_SIZE];
  while (ntotread < n) {
    nread = Read(in, c, min(n - ntotread, READ_BUFF_SIZE));
    if (nread <= 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
    ntotread += nread;
  }
  return ntotread;
}

int32 Relayn(file_fd in, fd out, uint32 n) {
  int32 ntotread;
  int32 nread;
  int32 nwritten;
  int8 c[READ_BUFF_SIZE];

#ifdef FREEBSD_SENDFILE
  if (n > 0) {
    if (sendfile(in, out, 0, n, &ntotread, 0) < 0) {
      if (errno != EINVAL)
        return NET_ERROR;
    } else
      return ntotread;
  } else if (n < 0) {
    if (sendfile(in, out, 0, 0, &ntotread, 0) < 0) {
      if (errno != EINVAL)
        return NET_ERROR;
    } else
      return ntotread;
  }
#endif /* FREEBSD_SENDFILE */

  ntotread = 0;
  while (n < 0 || ntotread < n) {
    nread = FileRead(
        in, c, n < 0 ? READ_BUFF_SIZE : min(READ_BUFF_SIZE, n - ntotread));
    if (nread <= 0) {
      if (errno == EINTR)
        continue;
      if (n < 0)
        return ntotread;
      return NET_ERROR;
    }
    nwritten = Write(out, c, nread);
    while (nwritten < nread) {
      if (errno == EINTR) {
        nwritten = Write(out, c, nread);
        continue;
      }
      return NET_ERROR;
    }
    ntotread += nread;
  }
  return ntotread;
}

int32 Readn(fd in, void* buff, uint32 n) {
  int32 ntotread = 0;
  int32 nread;
  int8* vbuff = buff;
  while (ntotread < n) {
    nread = recv(in, vbuff, n - ntotread, 0);
    if (nread <= 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
    ntotread += nread;
    vbuff += nread;
  }
  return ntotread;
}

int32 Writen(fd out, void* buff, uint32 n) {
  int32 ntotwritten = 0;
  int32 nwritten;
  int8* vbuff = buff;
  while (ntotwritten < n) {
    nwritten = send(out, vbuff, n - ntotwritten, 0);
    if (nwritten <= 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
    ntotwritten += nwritten;
    vbuff += nwritten;
  }
  return ntotwritten;
}

int32 Readline(fd in, void* buff, uint32 maxsize) {
  int32 nread = 0;
  int32 ntotread = 0;
  int8* cbuff = buff;
  int8 c = 0;
  maxsize -= sizeof(int8);
  if (maxsize < 0)
    return ntotread;
  while (ntotread < maxsize) {
    nread = Read(in, &c, sizeof(int8));
    if (nread < 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
    if (nread < sizeof(int8)) {
      ntotread += nread;
      break;
    }
    if (!c || c == '\n' || c == '\r')
      break;
    *cbuff = c;
    cbuff += nread;
    ntotread += nread;
  }
  while (c && c != '\n' && c != '\r') {
    nread = Read(in, &c, sizeof(int8));
    if (nread < 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
  }
#ifdef NET_CR
  while (c && c != '\n') {
    nread = Read(in, &c, sizeof(int8));
    if (nread < 0) {
      if (errno == EINTR)
        continue;
      return NET_ERROR;
    }
  }
#endif /* NET_CR */
  *(cbuff++) = 0;
  return ntotread;
}

int32 Writeline(fd out, void* buff) {
  int32 nwritten = 0;
  int32 ntotwritten = 0;
  int32 length = 0;
#ifdef NET_CR
  int8 pterm = '\r';
#endif /* NET_CR */
  int8 term = '\n';
  int8* cbuff = buff;
  for (; *cbuff && *cbuff != '\n' && *cbuff != '\r'; ++length, ++cbuff)
    ;
  nwritten = Writen(out, buff, length * sizeof(int8));
  if (nwritten < length * sizeof(int8))
    return NET_ERROR;
  ntotwritten += nwritten;
#ifdef NET_CR
  nwritten = Writen(out, &pterm, sizeof(int8));
  if (nwritten < sizeof(int8))
    return NET_ERROR;
  ntotwritten += nwritten;
#endif /* NET_CR */
  nwritten = Writen(out, &term, sizeof(int8));
  if (nwritten < sizeof(int8))
    return NET_ERROR;
  ntotwritten += nwritten;
  return ntotwritten;
}

int32 Socket(fd* in, int32 addrfamily, int32 socktype, int32 protocol) {
  if ((*in = socket(addrfamily, socktype, protocol)) < 0)
    return NET_ERROR;
  return NET_OK;
}

int32 Close(fd* in) {
  close(*in);
  *in = -1;
  return NET_OK;
}

static struct addrinfo* getAddrResults(char* ip,
                                       char* service,
                                       int32 flags,
                                       int32 socktype,
                                       int32 protocol) {
  int status;
  struct addrinfo hints, *results = NULL;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = flags;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = socktype;
  hints.ai_protocol = protocol;
  hints.ai_addrlen = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  status = getaddrinfo(ip, service, &hints, &results);
  if (status) {
    results = NULL;
  }
  return results;
}

static int32 freeAddrResults(struct addrinfo* results) {
  freeaddrinfo(results);
  return NET_OK;
}

static void getNameResults(char* ip,
                           char* service,
                           struct sockaddr* addr,
                           uint32 size) {
  int status;
  char ipname[MISC_BUFF_SIZE] = {0}, servicename[MISC_BUFF_SIZE] = {0};
  status = getnameinfo(addr, size, ipname, MISC_BUFF_SIZE, servicename,
                       MISC_BUFF_SIZE, 0);
  if (ip)
    *ip = 0;
  if (service)
    *service = 0;
  if (!status) {
    if (ip)
      strncpy(ip, ipname, MISC_BUFF_SIZE);
    if (service)
      strncpy(service, servicename, MISC_BUFF_SIZE);
  }
}

static int32 ConnectImpl(fd* in,
                         char* ip,
                         char* service,
                         int32 socktype,
                         int32 protocol) {
  struct addrinfo *results, *rp;

  results = getAddrResults(ip, service, 0, socktype, protocol);
  if (results == NULL)
    return NET_ERROR;

  for (rp = results; rp; rp = rp->ai_next) {
    int status = Socket(in, rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (status == NET_OK) {
      if ((rp->ai_socktype != SOCK_DGRAM) && (rp->ai_protocol != IPPROTO_UDP)) {
        if (connect(*in, rp->ai_addr, rp->ai_addrlen) < 0)
          break;
        Close(in);
      } else
        break;
    }
  }

  freeAddrResults(results);
  results = NULL;

  if (rp == NULL)
    return NET_ERROR;

  return NET_OK;
}

static int32 BindImpl(fd* bindin,
                      char* ip,
                      char* service,
                      int32 socktype,
                      int32 protocol) {
  struct addrinfo *results, *rp;

  results = getAddrResults(ip, service, AI_PASSIVE, socktype, protocol);
  if (results == NULL)
    return NET_ERROR;

  for (rp = results; rp; rp = rp->ai_next) {
    int status;
    status = Socket(bindin, rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (status == NET_OK) {
      int opt = 1;
      setsockopt(*bindin, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                 sizeof(opt));
      if (bind(*bindin, rp->ai_addr, rp->ai_addrlen) >= 0)
        break;
      Close(bindin);
    }
  }

  freeAddrResults(results);
  results = NULL;

  if (rp == NULL)
    return NET_ERROR;

  if (protocol == IPPROTO_TCP || socktype == SOCK_STREAM)
    if (listen(*bindin, NET_BACKLOG) != 0) {
      Close(bindin);
      return NET_ERROR;
    }

  return NET_OK;
}

int32 Connect(fd* in, char* ip, char* service) {
  return ConnectImpl(in, ip, service, SOCK_STREAM, IPPROTO_TCP);
}

int32 Bind(fd* bindin, char* ip, char* service) {
  return BindImpl(bindin, ip, service, SOCK_STREAM, IPPROTO_TCP);
}

int32 ConnectUDP(fd* in, char* ip, char* service) {
  return ConnectImpl(in, ip, service, SOCK_DGRAM, IPPROTO_UDP);
}

int32 BindUDP(fd* bindin, char* ip, char* service) {
  return BindImpl(bindin, ip, service, SOCK_DGRAM, IPPROTO_UDP);
}

int32 Accept(fd* bindin, fd* acceptin, char* ip, char* service) {
  struct sockaddr caddr;
  uint32 caddrs = sizeof(caddr);

  *acceptin = accept(*bindin, (struct sockaddr*)&caddr, &caddrs);
  if (*acceptin < 0) {
    return NET_ERROR;
  }

  getNameResults(ip, service, &caddr, caddrs);
  return NET_OK;
}

int32 RecvFrom(fd in, void* buff, uint32 maxsize, char* ip, char* service) {
  int32 nread = 0;
  struct sockaddr caddr;
  uint32 caddrs = 0;

  nread = recvfrom(in, buff, maxsize, 0, (struct sockaddr*)&caddr, &caddrs);
  if (nread < 0)
    return NET_ERROR;

  getNameResults(ip, service, &caddr, caddrs);
  return nread;
}

int32 SendTo(fd out, void* buff, uint32 maxsize, char* ip, char* service) {
  int32 nwritten = 0;
  struct addrinfo *results, *rp;

  results = getAddrResults(ip, service, 0, SOCK_DGRAM, IPPROTO_UDP);
  if (results == NULL)
    return NET_ERROR;

  for (rp = results; rp; rp = rp->ai_next) {
    nwritten = sendto(out, buff, maxsize, 0, rp->ai_addr, rp->ai_addrlen);
    if (nwritten >= 0)
      break;
  }

  freeAddrResults(results);
  results = NULL;

  if (rp == NULL)
    return NET_ERROR;

  if (nwritten < 0)
    return NET_ERROR;

  return nwritten;
}
