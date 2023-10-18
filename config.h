#pragma once

#include "arena.h"
#include <stdio.h>

struct vcd_config {
  struct {
    // port to accept connections on
    short port;

    // IP we listen on
    char *ip;

    // minimum of 1, maximum of 1024
    short num_workers;
  } server;

  struct {
    /// number of incoming connections in the queue allowed
    int max_incoming;

    /// number of active connections allowed
    int max_connections;
  } connections;

  // internal data
  struct vcd_arena arena;
  char *_fdata;
  void *_table;
};

extern struct vcd_config config_parse_path(const char *path);
extern void config_destroy(struct vcd_config *config);
extern void config_dump(FILE *file, struct vcd_config *conf);
