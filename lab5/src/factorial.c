/*
 * factorial.c
 * Параллельное вычисление k! mod mod.
 *
 * Сборка:  gcc factorial.c -o factorial -pthread
 * Запуск:  ./factorial -k 10 --pnum=4 --mod=10
 */
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long long ull;

struct FactorialArgs {
  ull begin; /* начало диапазона (включительно) */
  ull end;   /* конец диапазона (включительно)  */
};

ull mod = 0;
ull result = 1; /* общий результат для всех потоков */
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

/* (a * b) % m без переполнения: промежуточное произведение в 128 битах */
ull MulMod(ull a, ull b, ull m) {
  return (ull)(((unsigned __int128)a * b) % m);
}

void *ThreadFactorial(void *arg) {
  struct FactorialArgs *fa = (struct FactorialArgs *)arg;

  /* Локальное произведение своего диапазона — без блокировки */
  ull local = 1 % mod;
  for (ull i = fa->begin; i <= fa->end; i++)
    local = MulMod(local, i % mod, mod);

  /* Критическая секция: домножаем общий результат */
  pthread_mutex_lock(&mut);
  result = MulMod(result, local, mod);
  pthread_mutex_unlock(&mut);
}

static void Usage(const char *prog) {
  fprintf(stderr, "Usage: %s -k <num> --pnum=<num> --mod=<num>\n", prog);
}

int main(int argc, char **argv) {
  long long k = -1;
  long long pnum = -1;
  long long m = -1;

  static struct option options[] = {
      {"k", required_argument, 0, 'k'},
      {"pnum", required_argument, 0, 'p'},
      {"mod", required_argument, 0, 'm'},
      {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "k:", options, NULL)) != -1) {
    switch (c) {
    case 'k': k = atoll(optarg); break;
    case 'p': pnum = atoll(optarg); break;
    case 'm': m = atoll(optarg); break;
    default: Usage(argv[0]); return 1;
    }
  }

  if (k < 0 || pnum <= 0 || m <= 0) {
    fprintf(stderr, "Ошибка: нужно k >= 0, pnum > 0, mod > 0\n");
    Usage(argv[0]);
    return 1;
  }

  mod = (ull)m;
  result = 1 % mod;

  /* 0! = 1, перемножать нечего */
  if (k == 0) {
    printf("%lld! mod %lld = %llu\n", k, m, result);
    return 0;
  }

  /* Потоков больше, чем чисел, не нужно */
  if (pnum > k) pnum = k;

  pthread_t *threads = malloc(sizeof(pthread_t) * pnum);
  struct FactorialArgs *args = malloc(sizeof(struct FactorialArgs) * pnum);
  if (!threads || !args) {
    perror("malloc");
    return 1;
  }

  /* Делим числа 1..k на pnum почти равных частей */
  ull chunk = (ull)k / pnum;
  ull rest = (ull)k % pnum;
  ull start = 1;
  for (long long i = 0; i < pnum; i++) {
    ull len = chunk + ((ull)i < rest ? 1 : 0);
    args[i].begin = start;
    args[i].end = start + len - 1;
    start += len;

    if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]) != 0) {
      perror("pthread_create");
      return 1;
    }
  }

  for (long long i = 0; i < pnum; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("pthread_join");
      return 1;
    }
  }

  printf("%lld! mod %lld = %llu\n", k, m, result);

  free(threads);
  free(args);
  pthread_mutex_destroy(&mut);
  return 0;
}