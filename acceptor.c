#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "acceptor.h"
#include "connection.h"

struct acceptor_logic_args
{
  int fd;
  int incoming_pipe_tx;
};

// safely cancellable
static void*
acceptor_logic(void* vp_args)
{
  struct acceptor_logic_args args = *(struct acceptor_logic_args*)vp_args;
  free(vp_args);

  struct pollfd fd = { .fd = args.fd, .events = POLLIN };

  for (;;) {
    if (poll(&fd, 1, -1) < 0) {
      printf("acceptor socket closed unexpectedly early\n");
      exit(1);
    }

    if (!(fd.revents && POLLIN))
      continue;

    int newfd;
    struct sockaddr addr;
    socklen_t addrlen;

    if ((newfd = accept(fd.fd, &addr, &addrlen)) < 0) {
      printf("some error occured while accepting, continuing\n");
      continue;
    }

    if (addr.sa_family != AF_INET) {
      close(newfd);
    }

    struct sockaddr_in* inaddr = (struct sockaddr_in*)&addr;
    printf("new incoming connection from %s\n", inet_ntoa(inaddr->sin_addr));
    fflush(stdout);

    struct vcd_connection con = {
      .addr = inaddr->sin_addr.s_addr,
      .sockfd = newfd,
    };

    vcd_connection_serialize(args.incoming_pipe_tx, con);
  }
}

extern struct vcd_acceptor_return
start_acceptor(struct vcd_config* config)
{
  int infd;
  struct sockaddr_in sockaddr;

  if ((infd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "failed to create acceptor socket\n");
    exit(1);
  }

  if ((sockaddr.sin_addr.s_addr = inet_addr(config->server.ip)) ==
      INADDR_NONE) {
    fprintf(stderr, "invalid ip is given in config\n");
    exit(1);
  }

  sockaddr.sin_port = config->server.port;
  sockaddr.sin_family = AF_INET;

  if (bind(infd, (struct sockaddr*)&sockaddr, sizeof sockaddr) < 0) {
    fprintf(stderr, "failed to bind acceptor socket to ip & port\n");
    exit(1);
  }

  if (listen(infd, config->connections.max_incoming) < 0) {
    fprintf(stderr, "failed to listen acceptor infd\n");
    exit(1);
  }

  int inpipe[2];
  pipe(inpipe);

  struct acceptor_logic_args* args = malloc(sizeof(struct acceptor_logic_args));
  args->incoming_pipe_tx = inpipe[1];
  args->fd = infd;

  pthread_t thread;
  pthread_create(&thread, NULL, acceptor_logic, args);

  struct vcd_acceptor_return ret = {
    .incoming_pipe_rx = inpipe[0],
    .thread = thread,
  };

  return ret;
}
