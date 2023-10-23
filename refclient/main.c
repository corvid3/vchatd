#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

int
main(void)
{
  int fd;
  struct sockaddr_in addr;
  int addrlen = sizeof addr;

  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = 8088;
  addr.sin_family = AF_INET;

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "failed to create socket\n");
    exit(1);
  }

  if (connect(fd, (const struct sockaddr*)&addr, addrlen) < 0) {
    fprintf(stderr, "failed to connect to server\n");
    exit(1);
  }

  printf("connected\n");
  fflush(stdout);

// 63kb of info
#define KILO 1024
#define BUFSIZE (63 * KILO)
  char* buf = malloc(BUFSIZE + 1);
  time_t now;
  time(&now);
  srand(now);
  for (int i = 0; i < BUFSIZE; i++)
    buf[i] = rand() % 0xFE + 1;
  buf[BUFSIZE] = 0;
  unsigned short buf_len = strlen(buf);

  send(fd, &buf_len, sizeof buf_len, MSG_WAITALL);
  send(fd, buf, buf_len, MSG_WAITALL);

  char* new_buf = NULL;
  unsigned short incoming_buf = 0;

  // printf("in: %i\n", incoming_buf);
  recv(fd, &incoming_buf, sizeof incoming_buf, MSG_WAITALL);
  new_buf = calloc(sizeof incoming_buf + 1, 1);
  recv(fd, new_buf, incoming_buf, MSG_WAITALL);

  // printf("got response!\n\t%s\n", new_buf);

  free(new_buf);
}
