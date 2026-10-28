#ifndef MULTMODULO_H
#define MULTMODULO_H

#include <stdint.h>

/* (a * b) % mod без переполнения */
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

#endif