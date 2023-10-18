#include <bits/pthreadtypes.h>
#include <errno.h>
#include <pthread.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

#include "config.h"
#include "connection.h"
#include "log.h"
#include "worker.h"

struct worker_args
{
  const struct vcd_config* config;

  pthread_mutex_t* num_cons_mutex;
  int* num_cons;
  bool* accepting_cons;

  // pipe<struct vcd_message*>
  int con_msg_tx;

  // pipe<struct vcd_connection*>
  int new_con_rx;

  int id;
};

// stores state alongside a vcd_connection
struct worker_connection
{
  struct vcd_connection connection;

  enum
  {
    CON_SEEKING,
    CON_READING,
  } mode;

  char len_buf[2];
  int len_buf_len;

  char* data_buf;
  int data_buf_len;
};

// data stored by worker thread for stateful logic
struct worker
{
  const struct vcd_config* config;

  struct worker_connection* connections;
  struct pollfd* pollfds;

  pthread_mutex_t* num_cons_mutex;
  int* num_cons;
  bool* accepting_cons;

  int con_msg_tx;
  int new_con_rx;

  int id;
};

static struct worker
worker_init(void* vp_args)
{
  struct worker_args* args = (struct worker_args*)vp_args;
  printf("OUTER %i %i\n", args->id, *args->num_cons);

  const int max_cons = args->config->connections.max_connections;

  const int* num_cons = args->num_cons;
  printf("ASDF: ID: %i | %i\n", args->id, *num_cons);

  struct worker worker = {
    .config = args->config,

    .connections = malloc(sizeof(struct worker_connection) * max_cons),
    .pollfds = malloc(sizeof(struct pollfd) * (max_cons + 1)),

    .num_cons = args->num_cons,
    .accepting_cons = args->accepting_cons,
    .num_cons_mutex = args->num_cons_mutex,

    .con_msg_tx = args->con_msg_tx,
    .new_con_rx = args->new_con_rx,

    .id = args->id,
  };

  // alongside listening to socket FD's,
  // we need to also listen to the new_con_rx pipe
  // so we also add 1 to the poll(...) call (see below)
  worker.pollfds[0].events = POLLIN;
  worker.pollfds[0].fd = worker.new_con_rx;

  printf("ASDF: ID: %i | %i\n", worker.id, *num_cons);

  return worker;
}

// returns the index into the pollfd array
static inline int
worker_get_pollfd_by_fd(struct worker* worker, int fd)
{
  const int num_cons = *worker->num_cons;
  for (int i = 0; i < num_cons; i++)
    if (worker->pollfds[i].fd == fd)
      return i;

  VC_LOG_ERR("fell out");
  exit(1);
}

static inline int
worker_get_con_by_fd(struct worker* worker, int fd)
{
  const int num_cons = *worker->num_cons;
  for (int i = 0; i < num_cons; i++)
    if (worker->connections[i].connection.sockfd == fd)
      return i;

  VC_LOG_ERR("fell out");
  exit(1);
}

/// only removes a socket from the internal lists, doesn't disconnect
/// if one wants to disconnect a connection, call worker_disconnect
static void
worker_remove_sock(struct worker* worker, int fd)
{
  printf("removing socket\n");
  const int num_cons = *worker->num_cons;

  const int pidx = worker_get_pollfd_by_fd(worker, fd);
  const int cidx = worker_get_con_by_fd(worker, fd);

  worker->pollfds[pidx] = worker->pollfds[num_cons - 1];
  worker->connections[cidx] = worker->connections[num_cons - 1];

  *worker->num_cons -= 1;
}

static void
worker_disconnect(struct worker* worker, int fd)
{
  printf("disconnecting socket\n");
  close(fd);
  worker_remove_sock(worker, fd);
}

static void
worker_seek_logic(struct worker* worker, struct worker_connection* con)
{
  const int rb = read(con->connection.sockfd, con->len_buf, 2);

  if (rb <= 0) {
    worker_remove_sock(worker, con->connection.sockfd);
    return;
  }

  con->len_buf_len += rb;

  // if the connection has successfully told us
  // how many bytes it will transmit in the next message,
  // we set it to message read mode
  if (con->len_buf_len == 2) {
    con->len_buf_len = 0;
    con->mode = CON_READING;

    con->data_buf = malloc(*(unsigned short*)con->len_buf);
  }
}

static void
worker_read_logic(struct worker* worker, struct worker_connection* con)
{
  const int req = con->data_buf_len - *(unsigned short*)con->len_buf;
  const int rb =
    read(con->connection.sockfd, con->data_buf + con->data_buf_len, req);

  if (rb <= 0) {
    worker_remove_sock(worker, con->connection.sockfd);
    return;
  }

  con->data_buf_len += rb;
  if (con->data_buf_len == *(unsigned short*)con->len_buf) {
    con->data_buf_len = 0;

    printf("GOT MESSAGE: %s\n", con->data_buf);
    write(
      con->connection.sockfd, &con->data_buf_len, sizeof(con->data_buf_len));
    write(con->connection.sockfd, con->data_buf, con->data_buf_len);

    worker_disconnect(worker, con->connection.sockfd);

    free(con->data_buf);
  }
}

static void
worker_add_new(struct worker* worker, struct vcd_connection con)
{
  printf("adding socket\n");
  pthread_mutex_lock(worker->num_cons_mutex);
  worker->connections[*worker->num_cons].connection = con;
  worker->pollfds[*worker->num_cons + 1].fd = con.sockfd;
  worker->pollfds[*worker->num_cons + 1].events = POLLIN;
  *worker->num_cons += 1;
  *worker->accepting_cons = true;
  pthread_mutex_unlock(worker->num_cons_mutex);
}

static void*
worker_logic(void* vp_args)
{
  const struct worker_args* t = vp_args;
  printf("id: %i NUM_CONS: %i \n",
         ((struct worker_args*)vp_args)->id,
         *((struct worker_args*)vp_args)->num_cons);
  printf("OUTER %i %i\n", t->id, *t->num_cons);
  struct worker worker = worker_init(vp_args);
  free(vp_args);

  for (;;) {
    // guarantee that we are not dereferencing the atomic every time
    const int num_cons = *worker.num_cons;
    printf("id: %i NUM_CONS: %i @ %p \n",
           ((struct worker_args*)vp_args)->id,
           num_cons,
           (void*)worker.num_cons);

    if (poll(worker.pollfds, num_cons + 1, -1) < 0) {
      VC_LOG_ERR("poll err");
      VC_LOG_ERR_CONT("id: %i what: %s", worker.id, strerror(errno));
      exit(1);
    }

    if (worker.pollfds[0].revents & POLLIN) {
      struct vcd_connection new =
        vcd_connection_deserialize(worker.pollfds[0].fd);
      worker_add_new(&worker, new);
    }

    for (int i = 0; i < num_cons; i++) {
      struct pollfd* pollfd = &worker.pollfds[i + i];

      // if this socket doesn't have anything on it, continue
      if (!(pollfd->revents & POLLIN))
        continue;

      struct worker_connection* con;

      // search for the connection that is
      for (int i = 0; i < num_cons; i++)
        if (worker.connections[i].connection.sockfd == pollfd->fd)
          con = &worker.connections[i];

      switch (con->mode) {
        case CON_SEEKING:
          worker_seek_logic(&worker, con);
          break;

        case CON_READING:
          worker_read_logic(&worker, con);
          break;
      }
    }
  }

  return NULL;
}

extern struct vcd_worker_handle
worker_spawn(const struct vcd_config* config, int id)
{
  int* num_cons = malloc(sizeof(int));
  bool* accepting_cons = malloc(sizeof(bool));

  if (!num_cons)
    VC_LOG_ERR("num cons is null");
  if (!accepting_cons)
    VC_LOG_ERR("accepting cons is null");

  *num_cons = 0;
  *accepting_cons = true;

  int msgpipe[2], newpipe[2];
  pipe(msgpipe);
  pipe(newpipe);

  pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mutex, NULL);

  struct worker_args args = {
    .config = config,
    .num_cons_mutex = mutex,
    .num_cons = num_cons,
    .accepting_cons = accepting_cons,
    .new_con_rx = newpipe[0],
    .con_msg_tx = msgpipe[1],
    .id = id,
  };

  struct worker_args* args_ptr = malloc(sizeof(struct worker_args));
  *args_ptr = args;

  pthread_t thread;
  pthread_create(&thread, NULL, worker_logic, (void*)args_ptr);

  struct vcd_worker_handle handle = {
    .num_cons = num_cons,
    .accepting = accepting_cons,
    .num_cons_mutex = mutex,
    .new_con_tx = newpipe[1],
    .con_msg_rx = msgpipe[0],
    .thread = thread,
  };

  return handle;
}

extern void
vcd_worker_cleanup(struct vcd_worker_handle* handle)
{
  free(handle->num_cons);
  pthread_mutex_destroy(handle->num_cons_mutex);
  free(handle->num_cons_mutex);
}
