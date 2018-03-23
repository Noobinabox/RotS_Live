/* platdef.h */

#ifndef PLATDEF_H
#define PLATDEF_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>

#define SocketType int

typedef signed short int sh_int;
typedef unsigned short int ush_int;

typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned char ubyte;

#define COPY_COMMAND "cp"

#endif /* PLATDEF_H */
