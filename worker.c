#include <bits/pthreadtypes.h>
#include <errno.h>
#include <pthread.h>

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "connection.h"
#include "log.h"
#include "worker.h"

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

  // list of all _client_ connections, along with some state
  // for asynchronous socket IO
  struct worker_connection* connections;

  // list of polled file-descriptors
  // does not map 1-1 with `connections`
  // contains both client connection fd,
  //   the incoming connection socket from the acceptor
  struct pollfd* pollfds;

  pthread_mutex_t* num_cons_mutex;
  int* num_cons;
  int* num_incoming_cons;

  int con_msg_tx;
  int new_con_rx;

  int id;
};

// returns the index into the pollfd array
static inline int
worker_get_pollfd_by_fd(struct worker* worker, int fd)
{
  const int num_cons = *worker->num_cons;
  for (int i = 0; i < num_cons; i++)
    if (worker->pollfds[i + 1].fd == fd)
      return i + 1;

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
  VC_LOG_ERR_CONT("worker id: %i, num_cons: %i", worker->id, *worker->num_cons);
  exit(1);
}

/// only removes a socket from the internal lists, doesn't disconnect
/// if one wants to disconnect a connection, call worker_disconnect
static void
worker_remove_sock(struct worker* worker, int fd)
{
  // printf("removing socket\n");
  const int num_cons = *worker->num_cons;

  const int pidx = worker_get_pollfd_by_fd(worker, fd);
  const int cidx = worker_get_con_by_fd(worker, fd);

  struct worker_connection* con = &worker->connections[cidx];
  if (con->data_buf)
    free(con->data_buf);

  worker->pollfds[pidx] = worker->pollfds[num_cons];
  worker->connections[cidx] = worker->connections[num_cons - 1];

  *worker->num_cons -= 1;
}

static void
worker_disconnect(struct worker* worker, int fd)
{
  // printf("disconnecting socket\n");
  worker_remove_sock(worker, fd);
  close(fd);
}

static void
worker_seek_logic(struct worker* worker, struct worker_connection* con)
{
  const int rb = recv(con->connection.sockfd, con->len_buf, 2, MSG_DONTWAIT);

  if (rb <= 0) {
    VC_LOG_ERR("client disconnected while in seek stage");
    if (rb < 0)
      VC_LOG_ERR_CONT("ERRNO %i: %s", errno, strerror(errno));
    worker_disconnect(worker, con->connection.sockfd);
    return;
  }

  con->len_buf_len += rb;

  // if the connection has successfully told us
  // how many bytes it will transmit in the next message,
  // we set it to message read mode
  if (con->len_buf_len == 2) {
    con->len_buf_len = 0;
    con->mode = CON_READING;

    printf("WORKER %i | %i BYTES OF DATA\n",
           worker->id,
           *(unsigned short*)con->len_buf);

    con->data_buf = malloc(*(unsigned short*)con->len_buf);
  }
}

static void
worker_read_logic(struct worker* worker, struct worker_connection* con)
{
  const int req = *(unsigned short*)con->len_buf - con->data_buf_len;
  const int rb = recv(con->connection.sockfd,
                      con->data_buf + con->data_buf_len,
                      req,
                      MSG_DONTWAIT);

  if (rb <= 0) {
    VC_LOG_ERR("client disconnected while in read stage");
    worker_remove_sock(worker, con->connection.sockfd);
    return;
  }

  con->data_buf_len += rb;
  if (con->data_buf_len == *(unsigned short*)con->len_buf) {
    unsigned short reply_len = con->data_buf_len;
    send(con->connection.sockfd, &reply_len, sizeof(unsigned short), 0);
    send(con->connection.sockfd, con->data_buf, con->data_buf_len, 0);

    con->data_buf_len = 0;
    free(con->data_buf);
    con->data_buf = NULL;

    con->mode = CON_SEEKING;

    worker_disconnect(worker, con->connection.sockfd);
  }
}

static struct worker_connection
work_con_from_vcd_con(struct vcd_connection con)
{
  struct worker_connection wc = {
    .connection = con,
    .mode = CON_SEEKING,
    .len_buf = { 0, 0 },
    .len_buf_len = 0,
    .data_buf = NULL,
    .data_buf_len = 0,
  };

  return wc;
}

static void
worker_add_new(struct worker* worker, struct vcd_connection con)
{
  printf("ID: %i | new incoming connection\n", worker->id);
  pthread_mutex_lock(worker->num_cons_mutex);
  const int num_cons = *worker->num_cons;
  worker->connections[num_cons] = work_con_from_vcd_con(con);
  worker->pollfds[num_cons + 1].fd = con.sockfd;
  worker->pollfds[num_cons + 1].events = POLLIN;
  *worker->num_cons += 1;
  *worker->num_incoming_cons -= 1;
  pthread_mutex_unlock(worker->num_cons_mutex);
}

static void*
worker_logic(void* vp_args)
{
  struct worker worker = *(struct worker*)vp_args;
  free(vp_args);

  printf("WORKER %i | spawned\n", worker.id);

  for (;;) {
    // guarantee that we are not dereferencing the atomic every time
    const int num_cons = *worker.num_cons;

    if (poll(worker.pollfds, num_cons + 1, -1) < 0) {
      VC_LOG_ERR("poll err");
      VC_LOG_ERR_CONT("id: %i what: %s", worker.id, strerror(errno));
      exit(1);
    }

    //   // printf("WORKER %i: num cons: %i\n", worker.id, *worker.num_cons);

    for (int i = 0; i < num_cons; i++) {
      struct pollfd* pollfd = &worker.pollfds[i + 1];

      // if this socket doesn't have anything on it, continue
      if (!(pollfd->revents & POLLIN))
        continue;

      const int con_index = worker_get_con_by_fd(&worker, pollfd->fd);
      struct worker_connection* con = &worker.connections[con_index];

      switch (con->mode) {
        case CON_SEEKING:
          worker_seek_logic(&worker, con);
          break;

        case CON_READING:
          worker_read_logic(&worker, con);
          break;

        default:
          VC_LOG_ERR("fell out of switch(con->mode)");
      }
    }

    if (worker.pollfds[0].revents & POLLERR) {
      VC_LOG_ERR("newconfd pollerr'd");
      raise(SIGTRAP);
    }

    if (worker.pollfds[0].revents & POLLIN) {
      struct vcd_connection new;
      if (!vcd_connection_deserialize(worker.pollfds[0].fd, &new)) {
        VC_LOG_ERR("failed to get incoming connection from boss as worker");
        VC_LOG_ERR_CONT("ERRNO %i: %s", errno, strerror(errno));
        raise(SIGTRAP);
      }

      worker_add_new(&worker, new);
    }
  }

  return NULL;
}

extern struct vcd_worker_handle
worker_spawn(const struct vcd_config* config, int id)
{
  const int max_cons =
    config->connections.max_connections / config->server.num_workers;

  int new_con[2], con_msg[2];
  pipe(new_con);
  pipe(con_msg);

  struct worker worker = {
    .config = config,

    .connections = malloc(sizeof(struct worker_connection) * max_cons),
    .pollfds = malloc(sizeof(struct pollfd) * (max_cons + 1)),

    .num_cons = malloc(sizeof(int)),
    .num_incoming_cons = malloc(sizeof(int)),
    .num_cons_mutex = malloc(sizeof(pthread_mutex_t)),

    .con_msg_tx = con_msg[1],
    .new_con_rx = new_con[0],

    .id = id,
  };

  *worker.num_cons = 0;
  *worker.num_incoming_cons = 0;
  if (pthread_mutex_init(worker.num_cons_mutex, NULL) != 0)
    VC_LOG_ERR("failed to create pthread mutex");

  // alongside listening to socket FD's,
  // we need to also listen to the new_con_rx pipe
  // so we also add 1 to the poll(...) call (see below)
  worker.pollfds[0].events = POLLIN;
  worker.pollfds[0].fd = worker.new_con_rx;

  struct worker* wptr = malloc(sizeof(struct worker));
  *wptr = worker;

  pthread_t thread;
  pthread_create(&thread, NULL, worker_logic, (void*)wptr);

  struct vcd_worker_handle handle;
  handle.num_cons = worker.num_cons;
  handle.num_incoming_cons = worker.num_incoming_cons;
  handle.num_cons_mutex = worker.num_cons_mutex;
  handle.con_msg_rx = con_msg[0];
  handle.new_con_tx = new_con[1];
  handle.thread = thread;

  return handle;
}

extern void
vcd_worker_cleanup(struct vcd_worker_handle* handle)
{
  free((void*)handle->num_cons);
  pthread_mutex_destroy(handle->num_cons_mutex);
  free(handle->num_cons_mutex);
  pthread_cancel(handle->thread);
}
