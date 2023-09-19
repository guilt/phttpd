#ifndef NETLIB_H_
#define NETLIB_H_

#define NET_ERROR -1
#define NET_OK 0

#define NET_BACKLOG 16

typedef int int32;
typedef long long int64;
typedef short int16;
typedef char int8;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef unsigned short uint16;
typedef unsigned char uint8;

typedef int file_fd;
typedef int fd;
typedef int64 intlen;

int32 Initialize();

intlen Readn(fd in, void *buf, intlen n);
intlen Skipn(fd in, intlen n);
intlen Writen(fd out, void *buf, intlen n);
intlen Relayn(file_fd in, fd out, intlen n);

intlen Readline(fd in, void *buf, intlen maxsize);
intlen Writeline(fd in, void *buf);

intlen RecvFrom(fd in, void *buf , intlen maxsize, char *ip, char *service);
intlen SendTo(fd in, void *buf, intlen maxsize, char *ip, char *service);

int32 Socket(fd *in, int32 addrfamily, int32 socktype, int32 protocol);
int32 Close(fd *in);

int32 Accept(fd *bindin, fd *acceptin, char *ip, char *service);
int32 Connect(fd *in, char *, char *);
int32 Bind(fd *bindin, char *ip, char *service);

int32 ConnectUDP(fd *in, char *ip, char *service);
int32 BindUDP(fd *bindin, char *ip, char *service);

#endif /* NETLIB_H_ */
