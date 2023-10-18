#pragma once

#include<netinet/ip.h>
#include <sys/socket.h>

struct vcd_connection {
	int32_t sockfd;
	int32_t addr;
};

/// writes a vcd_connection into a file-descriptor
static inline void vcd_connection_serialize(int fd, struct vcd_connection con) {
	send(fd, &con.sockfd, sizeof(con.sockfd), MSG_WAITALL);
	send(fd, &con.addr, sizeof(con.addr), MSG_WAITALL);
}

/// reads a vcd_connection from a file-descriptor
/// blocking, waits for entire message
static inline struct vcd_connection vcd_connection_deserialize(int fd) {
	struct vcd_connection con;
	recv(fd, &con.addr, sizeof(con.addr), MSG_WAITALL);
	recv(fd, &con.sockfd, sizeof(con.sockfd), MSG_WAITALL);
	return con;
}
