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
#define SIZE sizeof(struct sockaddr_in)

/* Разбор целого числа из строки с проверкой; -1 при ошибке */
static long parse_num(const char *s) {
  char *end;
  long v = strtol(s, &end, 10);
  if (*s == '\0' || *end != '\0' || v <= 0) return -1;
  return v;
}

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s --ip <server ip> [--port <port>] [--bufsize <bytes>]\n",
          prog);
}

int main(int argc, char *argv[]) {
  char *ip = NULL;
  long port = 10050;   /* значения по умолчанию — как в исходном коде */
  long bufsize = 100;

  static struct option options[] = {{"ip", required_argument, 0, 'i'},
                                    {"port", required_argument, 0, 'p'},
                                    {"bufsize", required_argument, 0, 'b'},
                                    {0, 0, 0, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "i:p:b:", options, NULL)) != -1) {
    switch (c) {
      case 'i': ip = optarg; break;
      case 'p': port = parse_num(optarg); break;
      case 'b': bufsize = parse_num(optarg); break;
      default: usage(argv[0]); exit(1);
    }
  }
  if (ip == NULL || port <= 0 || port > 65535 || bufsize <= 0) {
    usage(argv[0]);
    exit(1);
  }

  char *buf = malloc(bufsize);
  if (buf == NULL) {
    perror("malloc");
    exit(1);
  }

  /* 1. Создаём TCP-сокет (SOCK_STREAM) */
  int fd;
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creating");
    exit(1);
  }

  /* 2. Заполняем адрес сервера: семейство, IP, порт (в сетевом порядке байт) */
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, SIZE);
  servaddr.sin_family = AF_INET;
  if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
    fprintf(stderr, "bad address: %s\n", ip);
    exit(1);
  }
  servaddr.sin_port = htons((unsigned short)port);

  /* 3. Устанавливаем соединение (тройное рукопожатие TCP) */
  if (connect(fd, (SADDR *)&servaddr, SIZE) < 0) {
    perror("connect");
    exit(1);
  }

  /* 4. Читаем stdin порциями по bufsize байт и отправляем в сокет */
  write(1, "Input message to send\n", 22);
  int nread;
  while ((nread = read(0, buf, bufsize)) > 0) {
    if (write(fd, buf, nread) < 0) {
      perror("write");
      exit(1);
    }
  }

  /* 5. Ctrl+D (EOF) — закрываем соединение */
  close(fd);
  free(buf);
  exit(0);
}