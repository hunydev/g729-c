/* Generated from /home/exedev/g729/internal/lsp. */
#ifndef TEST_LSP_ORACLE_VECTORS_H
#define TEST_LSP_ORACLE_VECTORS_H
#include <stdint.h>
typedef struct lsp_oracle_vector {
    uint8_t l0, l1, l2, l3;
    int16_t sf1[11];
    int16_t sf2[11];
} lsp_oracle_vector;
#define LSP_ORACLE_VECTOR_COUNT 4
static const lsp_oracle_vector LSP_ORACLE_VECTORS[LSP_ORACLE_VECTOR_COUNT] = {
    {0, 0, 0, 0,
     {4096, -2043, 2344, -2000, 2154, -1495, 1658, -735, 736, -345, -8},
     {4096, -337, -1007, -861, -124, -110, 740, 296, 221, -224, -363}
    },
    {1, 42, 11, 3,
     {4096, -2266, -1129, -1371, 1163, 743, 510, -510, 33, -282, 181},
     {4096, -4196, -648, -1944, 2736, 1468, 342, -1444, -93, -629, 725}
    },
    {0, 10, 5, 7,
     {4096, -3769, -830, -2365, 3200, 841, 818, -1578, -46, -584, 704},
     {4096, -3343, -998, -2732, 3523, 348, 1188, -1655, -8, -540, 682}
    },
    {1, 127, 31, 31,
     {4096, -4634, 980, -2810, 3993, -1758, 2155, -1827, 530, -1295, 1147},
     {4096, -5926, 3188, -3489, 4840, -4065, 3531, -2349, 1355, -2223, 1611}
    },
};
#endif
