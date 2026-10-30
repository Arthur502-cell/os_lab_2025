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

static void Usage(const char *prog) {
  fprintf(stderr, "Usage: %s --port 20001 --bufsize 1024\n", prog);
}

int main(int argc, char *argv[]) {
  int port = -1;    /* было: #define SERV_PORT 20001 */
  int bufsize = -1; /* было: #define BUFSIZE 1024 */

  static struct option options[] = {{"port", required_argument, 0, 'p'},
                                    {"bufsize", required_argument, 0, 'b'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
    case 'p': port = atoi(optarg); break;
    case 'b': bufsize = atoi(optarg); break;
    default: Usage(argv[0]); exit(1);
    }
  }

  if (port <= 0 || port > 65535 || bufsize <= 0) {
    Usage(argv[0]);
    exit(1);
  }

  const socklen_t kSlen = sizeof(struct sockaddr_in); /* было: #define SLEN */
  int sockfd, n;
  char ipadr[16];
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;

  char *mesg = malloc(bufsize + 1); /* +1 под завершающий '\0' */
  if (mesg == NULL) {
    perror("malloc");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket problem");
    exit(1);
  }

  memset(&servaddr, 0, kSlen);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&servaddr, kSlen) < 0) {
    perror("bind problem");
    exit(1);
  }
  printf("UDP SERVER starts at %d...\n", port);
  fflush(stdout);

  while (1) {
    socklen_t len = kSlen;

    if ((n = recvfrom(sockfd, mesg, bufsize, 0, (struct sockaddr *)&cliaddr,
                      &len)) < 0) {
      perror("recvfrom");
      exit(1);
    }
    mesg[n] = 0;

    printf("REQUEST %s      FROM %s : %d\n", mesg,
           inet_ntop(AF_INET, (void *)&cliaddr.sin_addr.s_addr, ipadr, 16),
           ntohs(cliaddr.sin_port));
    fflush(stdout);

    if (sendto(sockfd, mesg, n, 0, (struct sockaddr *)&cliaddr, len) < 0) {
      perror("sendto");
      exit(1);
    }
  }
}