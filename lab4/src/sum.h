#ifndef SUM_H
#define SUM_H

struct SumArgs {
  int *array;
  int begin;
  int end;
};

long long Sum(const struct SumArgs *args);

#endif