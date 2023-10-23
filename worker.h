#pragma once

#include"config.h"

#include"pthread.h"
#include<pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

struct vcd_worker_handle {
	pthread_mutex_t* num_cons_mutex;
	const int* num_cons;
	int* num_incoming_cons;
	int con_msg_rx;
	int new_con_tx;

	pthread_t thread;
};

extern struct vcd_worker_handle worker_spawn(const struct vcd_config* config, int id);
extern void vcd_worker_cleanup(struct vcd_worker_handle* handle);
