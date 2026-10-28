#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <pthread.h>

#include "multmodulo.h"

struct Server {
  char ip[255];
  int port;
};

/* Задание для одного потока клиента: какой сервер и какой кусок чисел */
struct ServerTask {
  struct Server *server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result; /* сюда поток положит ответ сервера */
  bool ok;         /* удалось ли получить ответ */
};

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0 || end == str || *end != '\0')
    return false;

  *val = i;
  return true;
}

/* Читает файл со строками вида ip:port.
   Возвращает массив серверов, количество кладёт в *count */
struct Server *ReadServers(const char *path, unsigned int *count) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    perror("fopen");
    return NULL;
  }

  unsigned int capacity = 4;
  unsigned int n = 0;
  struct Server *servers = malloc(sizeof(struct Server) * capacity);
  char line[300];

  while (fgets(line, sizeof(line), f) != NULL) {
    line[strcspn(line, "\r\n")] = '\0'; /* убираем перевод строки */
    if (line[0] == '\0')
      continue; /* пустые строки пропускаем */

    char *colon = strrchr(line, ':');
    if (colon == NULL) {
      fprintf(stderr, "Bad line in servers file: %s\n", line);
      continue;
    }
    *colon = '\0';

    if (n == capacity) {
      capacity *= 2;
      servers = realloc(servers, sizeof(struct Server) * capacity);
    }
    strncpy(servers[n].ip, line, sizeof(servers[n].ip) - 1);
    servers[n].ip[sizeof(servers[n].ip) - 1] = '\0';
    servers[n].port = atoi(colon + 1);
    n++;
  }

  fclose(f);
  *count = n;
  return servers;
}

/* Работа с одним сервером. Выполняется в отдельном потоке,
   поэтому все серверы считают свои куски одновременно */
void *ServerThread(void *arg) {
  struct ServerTask *t = (struct ServerTask *)arg;
  t->ok = false;

  struct hostent *hostname = gethostbyname(t->server->ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", t->server->ip);
    return NULL;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(t->server->port);
  memcpy(&server.sin_addr.s_addr, hostname->h_addr, hostname->h_length);

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", t->server->ip,
            t->server->port);
    close(sck);
    return NULL;
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &t->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &t->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &t->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send failed\n");
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), MSG_WAITALL) !=
      (ssize_t)sizeof(response)) {
    fprintf(stderr, "Recieve failed\n");
    close(sck);
    return NULL;
  }

  memcpy(&t->result, response, sizeof(uint64_t));
  t->ok = true;
  printf("%s:%d  [%" PRIu64 "..%" PRIu64 "] -> %" PRIu64 "\n", t->server->ip,
         t->server->port, t->begin, t->end, t->result);

  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers[255] = {'\0'}; /* 255 — ограничение длины имени файла в Linux */

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) {
          fprintf(stderr, "k must be a non-negative number\n");
          return 1;
        }
        break;
      case 1:
        if (!ConvertStringToUI64(optarg, &mod) || mod == 0) {
          fprintf(stderr, "mod must be a positive number\n");
          return 1;
        }
        break;
      case 2:
        strncpy(servers, optarg, sizeof(servers) - 1);
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == (uint64_t)-1 || mod == (uint64_t)-1 || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  /* 0! = 1 — серверы не нужны */
  if (k == 0) {
    printf("answer: %" PRIu64 "\n", 1 % mod);
    return 0;
  }

  unsigned int servers_num = 0;
  struct Server *to = ReadServers(servers, &servers_num);
  if (to == NULL)
    return 1;
  if (servers_num == 0) {
    fprintf(stderr, "No servers in file %s\n", servers);
    free(to);
    return 1;
  }

  /* Серверов больше, чем чисел — лишним работы нет */
  if (servers_num > k)
    servers_num = (unsigned int)k;

  /* Делим 1..k на servers_num кусков и для каждого сервера
     запускаем отдельный поток */
  pthread_t threads[servers_num];
  struct ServerTask tasks[servers_num];

  uint64_t chunk = k / servers_num;
  uint64_t rest = k % servers_num;
  uint64_t start = 1;

  for (unsigned int i = 0; i < servers_num; i++) {
    uint64_t len = chunk + (i < rest ? 1 : 0);
    tasks[i].server = &to[i];
    tasks[i].begin = start;
    tasks[i].end = start + len - 1;
    tasks[i].mod = mod;
    tasks[i].result = 0;
    tasks[i].ok = false;
    start += len;

    if (pthread_create(&threads[i], NULL, ServerThread, &tasks[i]) != 0) {
      perror("pthread_create");
      return 1;
    }
  }

  /* Ждём все потоки и объединяем результаты */
  uint64_t answer = 1 % mod;
  bool all_ok = true;
  for (unsigned int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
    if (!tasks[i].ok)
      all_ok = false;
    else
      answer = MultModulo(answer, tasks[i].result, mod);
  }

  free(to);

  if (!all_ok) {
    fprintf(stderr, "Some servers failed, answer is incomplete\n");
    return 1;
  }

  printf("answer: %" PRIu64 "\n", answer);
  return 0;
}