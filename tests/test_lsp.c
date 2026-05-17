#include "g729_lsp.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/lsp_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int lp_equal(const int16_t a[11], const int16_t b[11]) {
    int i;
    for (i = 0; i < 11; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int16_t lsp[10] = {31000, 28000, 24000, 20000, 15000,
                       10000, 5000, -1000, -10000, -20000};
    int16_t zero_lsp[10] = {0};
    int16_t a[11];
    int16_t sf1[11];
    int16_t sf2[11];
    int16_t sf1b[11];
    g729_lsp_decoder dec;
    g729_lsp_decoder fresh;
    g729_lsp_indices idx;
    int i;

    CHECK(g729_lsp_lsf_to_lsp(0) >= 32700);
    CHECK(g729_lsp_lsf_to_lsp(12868) >= -100);
    CHECK(g729_lsp_lsf_to_lsp(12868) <= 100);
    CHECK(g729_lsp_lsf_to_lsp(25700) <= -32000);
    {
        int16_t prev = 32767;
        int16_t lsfs[] = {1000, 4000, 8000, 12000, 16000, 20000, 24000};
        for (i = 0; i < (int)(sizeof(lsfs) / sizeof(lsfs[0])); ++i) {
            int16_t q = g729_lsp_lsf_to_lsp(lsfs[i]);
            CHECK(q <= prev);
            prev = q;
        }
    }

    memset(a, 0, sizeof(a));
    g729_lsp_lsp_to_lp(lsp, a);
    CHECK(a[0] == 4096);

    memset(a, 0, sizeof(a));
    g729_lsp_lsp_to_lp(zero_lsp, a);
    for (i = 1; i < 11; i += 2) {
        CHECK(a[i] == 0);
    }

    memset(&dec, 0, sizeof(dec));
    idx.l0 = 0;
    idx.l1 = 0;
    idx.l2 = 0;
    idx.l3 = 0;
    memset(sf1, 0, sizeof(sf1));
    memset(sf2, 0, sizeof(sf2));
    g729_lsp_decode(&dec, idx, sf1, sf2);
    CHECK(sf1[0] == 4096);
    CHECK(sf2[0] == 4096);
    {
        int nonzero = 0;
        for (i = 1; i < 11; ++i) {
            if (sf1[i] != 0 || sf2[i] != 0) {
                nonzero = 1;
                break;
            }
        }
        CHECK(nonzero);
    }

    memset(&dec, 0, sizeof(dec));
    idx.l0 = 0;
    idx.l1 = 10;
    idx.l2 = 5;
    idx.l3 = 7;
    g729_lsp_decode(&dec, idx, sf1, sf2);
    memset(&fresh, 0, sizeof(fresh));
    idx.l0 = 1;
    g729_lsp_decode(&fresh, idx, sf1b, sf2);
    CHECK(!lp_equal(sf1, sf1b));

    memset(&dec, 0, sizeof(dec));
    idx.l0 = 1;
    idx.l1 = 42;
    idx.l2 = 11;
    idx.l3 = 3;
    g729_lsp_decode(&dec, idx, sf1, sf2);
    g729_lsp_decoder_reset(&dec);
    memset(&fresh, 0, sizeof(fresh));
    idx.l0 = 0;
    idx.l1 = 0;
    idx.l2 = 0;
    idx.l3 = 0;
    g729_lsp_decode(&dec, idx, sf1, sf2);
    g729_lsp_decode(&fresh, idx, sf1b, a);
    CHECK(lp_equal(sf1, sf1b));

    memset(&dec, 0, sizeof(dec));
    for (i = 0; i < LSP_ORACLE_VECTOR_COUNT; ++i) {
        idx.l0 = LSP_ORACLE_VECTORS[i].l0;
        idx.l1 = LSP_ORACLE_VECTORS[i].l1;
        idx.l2 = LSP_ORACLE_VECTORS[i].l2;
        idx.l3 = LSP_ORACLE_VECTORS[i].l3;
        g729_lsp_decode(&dec, idx, sf1, sf2);
        CHECK(lp_equal(sf1, LSP_ORACLE_VECTORS[i].sf1));
        CHECK(lp_equal(sf2, LSP_ORACLE_VECTORS[i].sf2));
    }

    return 0;
}
