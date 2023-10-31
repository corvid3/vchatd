#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "acceptor.h"
#include "connection.h"
#include "log.h"
#include "worker.h"

struct acceptor_logic_args
{
  struct vcd_acceptor_args _args;
  int fd;
  int incoming_pipe_tx;
};

#define NO_WORKERS_AVAIABLE -1

static int
find_load_balanced_worker(struct vcd_config* conf,
                          struct vcd_worker_handle* handles,
                          int num_workers)
{
  const int max_cons_per_worker =
    conf->connections.max_connections / conf->server.num_workers;

  int workerfd, numworker = max_cons_per_worker;

  for (int i = 0; i < num_workers; i++) {
    struct vcd_worker_handle const handle = handles[i];

    pthread_mutex_lock(handle.num_cons_mutex);
    int num_cons_comb = *handle.num_cons + *handle.num_incoming_cons;

    if (num_cons_comb < max_cons_per_worker && num_cons_comb < numworker) {
      numworker = num_cons_comb;
      workerfd = handle.new_con_tx;
    }

    pthread_mutex_unlock(handle.num_cons_mutex);

    if (numworker == 0)
      break;
  }

  if (numworker == max_cons_per_worker)
    return NO_WORKERS_AVAIABLE;

  return workerfd;
}

// safely cancellable
static void*
acceptor_logic(void* vp_args)
{
  struct acceptor_logic_args args = *(struct acceptor_logic_args*)vp_args;
  free(vp_args);

  const int max_wq = args._args.config->connections.max_incoming;
  struct vcd_connection* waitqueue =
    calloc(max_wq, sizeof(struct vcd_connection));
  int wq_len = 0;

  struct pollfd fd = { .fd = args.fd, .events = POLLIN };

  for (;;) {
    const int pval = poll(&fd, 1, 1000);

    if (pval < 0) {
      printf("acceptor socket closed unexpectedly early\n");
      exit(1);
    }

    if (pval == 0) {
      // check the waiting queue
      int i, wfd;

      for (i = 0; i < wq_len; i++) {

        wfd = find_load_balanced_worker(args._args.config,
                                        args._args.worker_handles,
                                        args._args.config->server.num_workers);

        if (wfd == !NO_WORKERS_AVAIABLE)
          break;

        if (!vcd_connection_serialize(wfd, waitqueue[i]))
          VC_LOG_ERR("failed to serialize connection to worker");
      }

      for (int j = 0; j < wq_len; j++)
        waitqueue[j] = waitqueue[j + i];

      wq_len -= i;
      continue;
    }

    if (!(fd.revents && POLLIN))
      continue;

    int newfd;
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    if ((newfd = accept(fd.fd, &addr, &addrlen)) < 0) {
      VC_LOG_ERR("acceptor accept error");
      VC_LOG_ERR_CONT("ERR %i: %s", errno, strerror(errno));
      continue;
    }

    if (addr.sa_family != AF_INET) {
      printf("incoming connection is not ipv4, closing\n");
      close(newfd);
    }

    struct sockaddr_in* inaddr = (struct sockaddr_in*)&addr;
    // printf("new incoming connection from %s\n", inet_ntoa(inaddr->sin_addr));
    // fflush(stdout);

    struct vcd_connection con = {
      .addr = inaddr->sin_addr.s_addr,
      .sockfd = newfd,
    };

    int worker =
      find_load_balanced_worker(args._args.config,
                                args._args.worker_handles,
                                args._args.config->server.num_workers);

    // if there are no current workers available, push into the waiting queue
    if (worker == NO_WORKERS_AVAIABLE)
      waitqueue[wq_len++] = con;
    else {
      if (!vcd_connection_serialize(worker, con)) {
        VC_LOG_ERR("failed to send incoming connection to boss as acceptor");
        raise(SIGTERM);
      }
    }
  }
}

extern struct vcd_acceptor_return
start_acceptor(struct vcd_acceptor_args args)
{
  int infd;
  struct sockaddr_in sockaddr;

  if ((infd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "failed to create acceptor socket\n");
    exit(1);
  }

  if ((sockaddr.sin_addr.s_addr = inet_addr(args.config->server.ip)) ==
      INADDR_NONE) {
    fprintf(stderr, "invalid ip is given in config\n");
    exit(1);
  }

  sockaddr.sin_port = args.config->server.port;
  sockaddr.sin_family = AF_INET;

  int t = 1;
  setsockopt(infd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof t);

  if (bind(infd, (struct sockaddr*)&sockaddr, sizeof sockaddr) < 0) {
    fprintf(stderr, "failed to bind acceptor socket to ip & port\n");
    exit(1);
  }

  if (listen(infd, args.config->connections.max_incoming) < 0) {
    fprintf(stderr, "failed to listen acceptor infd\n");
    exit(1);
  }

  int inpipe[2];
  pipe(inpipe);

  struct acceptor_logic_args* targs =
    malloc(sizeof(struct acceptor_logic_args));
  targs->incoming_pipe_tx = inpipe[1];
  targs->fd = infd;
  targs->_args = args;

  pthread_t thread;
  pthread_create(&thread, NULL, acceptor_logic, targs);

  struct vcd_acceptor_return ret = {
    .thread = thread,
    .acceptor_fd_copy = infd,
  };

  return ret;
}
