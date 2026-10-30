#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr

static long parse_num(const char *s) {
  char *end;
  long v = strtol(s, &end, 10);
  if (*s == '\0' || *end != '\0' || v <= 0) return -1;
  return v;
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s [--port <port>] [--bufsize <bytes>]\n", prog);
}

int main(int argc, char *argv[]) {
  const size_t kSize = sizeof(struct sockaddr_in);
  long port = 10050;
  long bufsize = 100;

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

  char *buf = malloc(bufsize);
  if (buf == NULL) {
    perror("malloc");
    exit(1);
  }

  /* 1. Слушающий TCP-сокет */
  int lfd, cfd, nread;
  if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  /* Позволяет сразу перезапустить сервер на том же порту после Ctrl+C
     (иначе bind какое-то время выдаёт "Address already in use") */
  int opt = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  /* 2. Привязываем сокет ко всем интерфейсам (INADDR_ANY) и заданному порту */
  struct sockaddr_in servaddr, cliaddr;
  memset(&servaddr, 0, kSize);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((unsigned short)port);

  if (bind(lfd, (SADDR *)&servaddr, kSize) < 0) {
    perror("bind");
    exit(1);
  }

  /* 3. Переводим в режим ожидания, очередь до 5 непринятых соединений */
  if (listen(lfd, 5) < 0) {
    perror("listen");
    exit(1);
  }
  printf("TCP server listening on port %ld, bufsize %ld\n", port, bufsize);
  fflush(stdout);

  /* 4. Бесконечный цикл: принимаем клиентов по одному */
  while (1) {
    socklen_t clilen = kSize;

    if ((cfd = accept(lfd, (SADDR *)&cliaddr, &clilen)) < 0) {
      perror("accept");
      exit(1);
    }
    printf("connection established\n");
    fflush(stdout);

    /* Всё, что прислал клиент, печатаем в stdout, пока он не закроет соединение */
    while ((nread = read(cfd, buf, bufsize)) > 0) {
      write(1, buf, nread);
    }

    if (nread == -1) {
      perror("read");
      exit(1);
    }
    close(cfd);
  }
}