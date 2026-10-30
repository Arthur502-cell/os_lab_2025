#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr
#define SLEN sizeof(struct sockaddr_in)

static long parse_num(const char *s) {
  char *end;
  long v = strtol(s, &end, 10);
  if (*s == '\0' || *end != '\0' || v <= 0) return -1;
  return v;
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s [--port <port>] [--bufsize <bytes>]\n", prog);
}

int main(int argc, char **argv) {
  long port = 20001;
  long bufsize = 1024;

  static struct option options[] = {{"port", required_argument, 0, 'p'},
                                    {"bufsize", required_argument, 0, 'b'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "p:b:", options, NULL)) != -1) {
    switch (c) {
      case 'p': port = parse_num(optarg); break;
      case 'b': bufsize = parse_num(optarg); break;
      default: usage(argv[0]); exit(1);
    }
  }
  if (port <= 0 || port > 65535 || bufsize <= 0) {
    usage(argv[0]);
    exit(1);
  }

  char *mesg = malloc(bufsize + 1);  /* +1 — чтобы mesg[n] = 0 не вылез за буфер */
  char ipadr[INET_ADDRSTRLEN];
  if (mesg == NULL) {
    perror("malloc");
    exit(1);
  }

  /* 1. UDP-сокет */
  int sockfd, n;
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket problem");
    exit(1);
  }

  /* 2. bind на все интерфейсы и заданный порт. listen/accept в UDP нет */
  struct sockaddr_in servaddr, cliaddr;
  memset(&servaddr, 0, SLEN);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((unsigned short)port);

  if (bind(sockfd, (SADDR *)&servaddr, SLEN) < 0) {
    perror("bind problem");
    exit(1);
  }
  printf("SERVER starts on port %ld, bufsize %ld...\n", port, bufsize);
  fflush(stdout);

  /* 3. Принимаем датаграмму, печатаем от кого она, и отправляем её же обратно (эхо) */
  while (1) {
    socklen_t len = SLEN;

    if ((n = recvfrom(sockfd, mesg, bufsize, 0, (SADDR *)&cliaddr, &len)) < 0) {
      perror("recvfrom");
      exit(1);
    }
    mesg[n] = 0;

    printf("REQUEST %s      FROM %s : %d\n", mesg,
           inet_ntop(AF_INET, &cliaddr.sin_addr, ipadr, sizeof(ipadr)),
           ntohs(cliaddr.sin_port));
    fflush(stdout);

    if (sendto(sockfd, mesg, n, 0, (SADDR *)&cliaddr, len) < 0) {
      perror("sendto");
      exit(1);
    }
  }
}