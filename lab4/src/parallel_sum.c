
//   ./psum --threads_num 4 --seed 42 --array_size 50000000


#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <getopt.h>
#include <pthread.h>

#include "sum.h"
#include "utils.h"

// Cчитает сумму своей части массива
void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;

  long long *result = malloc(sizeof(long long));
  if (result != NULL) {
    *result = Sum(sum_args);
  }

  return result;
}

int main(int argc, char **argv) {
  int threads_num = -1;
  int array_size = -1;
  int seed = -1;

  static struct option options[] = {
      {"threads_num", required_argument, 0, 0},
      {"seed", required_argument, 0, 0},
      {"array_size", required_argument, 0, 0},
      {0, 0, 0, 0}};

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1) {
      break;
    }

    if (c != 0) {
      fprintf(stderr, "Unknown or incomplete option\n");
      return 1;
    }

    switch (option_index) {
      case 0:
        threads_num = atoi(optarg);
        break;
      case 1:
        seed = atoi(optarg);
        break;
      case 2:
        array_size = atoi(optarg);
        break;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
    return 1;
  }

  if (threads_num <= 0 || seed < 0 || array_size <= 0) {
    fprintf(stderr,
            "Usage: %s --threads_num NUM --seed NUM --array_size NUM\n",
            argv[0]);
    return 1;
  }

  // Потоков не больше, чем элементов
  if (threads_num > array_size) {
    threads_num = array_size;
  }

  // Генерация массива (не входит в замер времени)
  int *array = malloc(sizeof(int) * (size_t)array_size);
  if (array == NULL) {
    perror("malloc");
    return 1;
  }

  GenerateArray(array, (unsigned int)array_size, (unsigned int)seed);

  pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)threads_num);
  struct SumArgs *args = malloc(sizeof(struct SumArgs) * (size_t)threads_num);

  if (threads == NULL || args == NULL) {
    perror("malloc");
    free(threads);
    free(args);
    free(array);
    return 1;
  }

  struct timespec start_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Создание потоков, каждый получает свою часть массива
  for (int i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = (int)((long long)i * array_size / threads_num);
    args[i].end = (int)((long long)(i + 1) * array_size / threads_num);

    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      fprintf(stderr, "Error: pthread_create failed!\n");

      
      for (int j = 0; j < i; j++) {
        void *ret;
        pthread_join(threads[j], &ret);
        free(ret);
      }

      free(threads);
      free(args);
      free(array);
      return 1;
    }
  }

  // Ожидание потоков и сложение их результатов
  long long total_sum = 0;
  bool error = false;

  for (int i = 0; i < threads_num; i++) {
    void *ret = NULL;
    pthread_join(threads[i], &ret);

    if (ret == NULL) {
      error = true;
    } else {
      total_sum += *(long long *)ret;
      free(ret);
    }
  }

  // Конец замера времени
  struct timespec finish_time;
  clock_gettime(CLOCK_MONOTONIC, &finish_time);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (finish_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

  free(threads);
  free(args);
  free(array);

  if (error) {
    fprintf(stderr, "Error: a thread failed to return its result\n");
    return 1;
  }

  printf("Total: %lld\n", total_sum);
  printf("Elapsed time: %f ms\n", elapsed_time);
  return 0;
}