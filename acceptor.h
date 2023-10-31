#pragma once

#include "config.h"
#include <netinet/in.h>
#include<pthread.h>
#include"connection.h"
#include "worker.h"

struct vcd_acceptor_return {
	pthread_t thread;

	int acceptor_fd_copy;
};

struct vcd_acceptor_args {
	struct vcd_config* config;
	struct vcd_worker_handle* worker_handles;
};

extern struct vcd_acceptor_return start_acceptor(struct vcd_acceptor_args args);
