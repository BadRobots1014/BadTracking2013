#ifndef NETWORK_H
#define NETWORK_H

#include "config.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef UNIX

typedef struct {} SOCKET;

#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
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
} socket_t;

socket_t* socket_create(int socket_type);
socket_t* socket_open(char* hostname, char* port_str);
int       socket_write(socket_t* socket, char* buffer, int length);
int       socket_write_float(socket_t* socket, float f);
//int       socket_is_connected(socket_t* socket);
void      socket_release(socket_t* socket);

int   socket_read(socket_t* socket, char* buffer, int length);
float socket_read_float(socket_t* socket);

void      socket_bind(socket_t* socket, char* port_str, int backlog);
socket_t* socket_accept(socket_t* server_socket);
#endif
