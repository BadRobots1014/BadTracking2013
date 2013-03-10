#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int socket_open(char* hostname, char* port_str);
void socket_write(int socket, char* buffer, int length);
void socket_write_float(int socket, float f);
void socket_release(int socket);

#endif
