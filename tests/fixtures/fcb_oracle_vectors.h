/* Generated from /home/exedev/g729/internal/fcb. */
#ifndef TEST_FCB_ORACLE_VECTORS_H
#define TEST_FCB_ORACLE_VECTORS_H
#include <stdint.h>
typedef struct fcb_oracle_vector {
    uint16_t positions;
    uint8_t signs;
    int t;
    int16_t beta_q14;
    int16_t code[40];
} fcb_oracle_vector;
#define FCB_ORACLE_VECTOR_COUNT 5
static const fcb_oracle_vector FCB_ORACLE_VECTORS[FCB_ORACLE_VECTOR_COUNT] = {
    {0, 15, 40, 0, {8191, 8191, 8191, 8191, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {1340, 9, 40, 0, {0, 0, 0, 0, 0, 0, 0, 0, 8191, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8191, 0, -8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -8192, 0, 0, 0}},
    {0, 1, 20, 8192, {8191, -8192, -8192, -8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, -4096, -4096, -4096, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {4660, 10, 25, 10000, {0, 0, -8192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -8192, 0, 0, 0, 8191, 0, 0, -5000, 0, 0, 0, 8191, 0, 0, 0, 0, 0, 0, 0, 0}},
    {8191, 0, 5, 13017, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -8192, -8192, -8192, 0, -8192}},
};
#endif
