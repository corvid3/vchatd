#include "tomlc99/toml.h"

#include "arena.h"
#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR_BUF_LEN 1024
#define DEFAULT_MAX_CONS 4096
#define DEFAULT_MAX_INCOM 256
#define DEFAULT_NUM_WORKERS 12

static void
config_inner_parse_server(toml_table_t* server_table, struct vcd_config* conf)
{
  if (!server_table) {
    fprintf(stderr, "no [server] table in config toml\n");
    exit(1);
  }

  toml_datum_t t_port = toml_int_in(server_table, "port");

  if (!t_port.ok) {
    fprintf(stderr, "no port declared under server table in config\n");
    exit(1);
  } else
    conf->server.port = t_port.u.i;

  toml_datum_t t_ip = toml_string_in(server_table, "ip");
  if (!t_ip.ok) {
    printf("no ip declared under [server], using default of 127.0.0.1 (*)\n");
    char* f = calloc(strlen("127.0.0.1"), 1);
    strcpy(f, "127.0.0.1");
    conf->server.ip = f;
  } else {
    conf->server.ip = t_ip.u.s;
  }

  toml_datum_t t_num_workers = toml_int_in(server_table, "num_workers");
  if (!t_num_workers.ok) {
    printf("no num_workers declared under [server], using default of %i\n",
           DEFAULT_NUM_WORKERS);
    conf->server.num_workers = DEFAULT_NUM_WORKERS;
  } else {
    conf->server.num_workers = t_num_workers.u.i;
  }
}

static void
config_inner_parse_connections(toml_table_t* cons_table,
                               struct vcd_config* conf)
{
  if (!cons_table) {
    fprintf(stderr, "no [connections] table in config toml, using defaults\n");
    conf->connections.max_connections = DEFAULT_MAX_CONS;
    conf->connections.max_incoming = DEFAULT_MAX_INCOM;
    return;
  }

  toml_datum_t max_cons = toml_int_in(cons_table, "max_cons");
  if (!max_cons.ok) {
    fprintf(
      stderr, "no [max_cons] value, using default of %i\n", DEFAULT_MAX_CONS);

    conf->connections.max_connections = DEFAULT_MAX_CONS;
  } else
    conf->connections.max_connections = max_cons.u.i;

  toml_datum_t max_incom = toml_int_in(cons_table, "max_incom");
  if (!max_incom.ok) {
    fprintf(
      stderr, "no [max_incom] value, using default of %i\n", DEFAULT_MAX_INCOM);

    conf->connections.max_incoming = DEFAULT_MAX_INCOM;
  } else
    conf->connections.max_incoming = max_incom.u.i;
}

static struct vcd_config
config_parse(FILE* file)
{
  char err_buf[ERR_BUF_LEN];

  struct vcd_config conf = { 0 };

  toml_table_t* t_table = toml_parse_file(file, err_buf, ERR_BUF_LEN);

  toml_table_t* t_server = toml_table_in(t_table, "server");
  config_inner_parse_server(t_server, &conf);

  toml_table_t* t_connections = toml_table_in(t_table, "connections");
  config_inner_parse_connections(t_connections, &conf);

  return conf;
}

extern struct vcd_config
config_parse_path(const char* path)
{
  FILE* file = fopen(path, "r");

  if (!file) {
    printf(
      "failed to open config file at path: %s\n\t%s\n", path, strerror(errno));
  }

  struct vcd_config conf = config_parse(file);
  fclose(file);

  return conf;
}

extern void
config_destroy(struct vcd_config* config)
{
  toml_free(config->_table);
  free(config->server.ip);
}

extern void
config_dump(FILE* file, struct vcd_config* conf)
{
  fprintf(file,
          "========================\n"
          "  dumping config file\n"
          "========================\n"
          "[server]\n"
          "\tport: %i\n"
          "\tip: %s\n"
          "\tnum_workers: %i\n"
          "[connections]\n"
          "\tmax_incoming: %i\n"
          "\tmax_connections: %i\n",
          conf->server.port,
          conf->server.ip,
          conf->server.num_workers,
          conf->connections.max_incoming,
          conf->connections.max_connections);
}
