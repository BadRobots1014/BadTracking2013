#include "config.h"

#ifdef WINDOWS

#include "network.h"

void socket_release(socket_t* socket) {
	SOCKET int_sock = socket->internal.int_socket;
	closesocket(int_sock);
	free(socket);
}

//Will only allow you to pass IP addresses, not hostnames
socket_t* socket_open(char* hostname, char* port_str) {
	SOCKET net_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(net_socket == INVALID_SOCKET) {
		return NULL;
	}

	int port = atoi(port_str);

	sockaddr_in sock_info;
	sock_info.sin_family = AF_INET;
	sock_info.sin_addr.s_addr = inet_addr(hostname);
	sock_info.sin_port = htons(port);
	int r = connect(net_socket, (SOCKADDR*)&sock_info, sizeof(sock_info));
	if(r == SOCKET_ERROR) {
		return NULL;
	}

	socket_t* result = (socket_t*)malloc(sizeof(socket_t));
	result->internal.int_socket = socket;
	return result;
}

int socket_write(socket_t* socket, char* buffer, int length) {
	SOCKET int_socket = socket->internal.int_socket;
	return send(int_socket, buffer, length, 0);
}
int socket_write_float(socket_t* socket, float f) {
	//convert to Big-Endian (DataInputStream in java expects it)
	//<insert stupid assumption here>
	float result;
	char* buffer = (char*)&f;
	char* result_buffer = (char*)&result;

	result_buffer[0] = buffer[3];
	result_buffer[1] = buffer[2];
	result_buffer[2] = buffer[1];
	result_buffer[3] = buffer[0];

	return socket_write(socket, (char*)&result, sizeof(result));
}

#endif
