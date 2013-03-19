#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <stdio.h>

#ifndef WINDOWS

typedef struct {} SOCKET;

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#else

#include <windows.h>
#include <winsock2.h>

#endif

typedef struct {
	union {
		int int_fd;
		SOCKET int_socket;
	} internal;
} socket_t ;

socket_t* socket_open(char* hostname, char* port_str);
void      socket_write(socket_t* socket, char* buffer, int length);
void      socket_write_float(socket_t* socket, float f);
void      socket_release(socket_t* socket);

#endif
