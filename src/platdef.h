/* platdef.h */

#ifndef PLATDEF_H
#define PLATDEF_H

#if defined(__linux__) || defined(__unix__)

#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int SocketType;
#define COPY_COMMAND "cp"

#define ZERO_MEMORY(x, y) bzero(x, y)

#endif

#ifdef __bsdi__

#include <sys/param.h>

#endif

#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

typedef int SocketType;
#define COPY_COMMAND "xcopy"

#define ZERO_MEMORY(x, y) ZeroMemory(x, y)

#undef max
#undef min

#endif


#include <sys/types.h>
#include <time.h>


typedef signed short int sh_int;
typedef unsigned short int ush_int;

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned char ubyte;

#endif /* PLATDEF_H */
