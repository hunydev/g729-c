package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/lsp"
)

func main() {
	outPath := flag.String("out", "tests/fixtures/lsp_oracle_vectors.h", "output C header path")
	flag.Parse()

	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "lsp-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "lsp-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []lsp.Indices{
		{L0: 0, L1: 0, L2: 0, L3: 0},
		{L0: 1, L1: 42, L2: 11, L3: 3},
		{L0: 0, L1: 10, L2: 5, L3: 7},
		{L0: 1, L1: 127, L2: 31, L3: 31},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/lsp. */")
	fmt.Fprintln(f, "#ifndef TEST_LSP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_LSP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct lsp_oracle_vector {")
	fmt.Fprintln(f, "    uint8_t l0, l1, l2, l3;")
	fmt.Fprintln(f, "    int16_t sf1[11];")
	fmt.Fprintln(f, "    int16_t sf2[11];")
	fmt.Fprintln(f, "} lsp_oracle_vector;")
	fmt.Fprintf(f, "#define LSP_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const lsp_oracle_vector LSP_ORACLE_VECTORS[LSP_ORACLE_VECTOR_COUNT] = {")

	var dec lsp.Decoder
	for _, idx := range cases {
		sf1, sf2 := dec.Decode(idx)
		fmt.Fprintf(f, "    {%d, %d, %d, %d,\n", idx.L0, idx.L1, idx.L2, idx.L3)
		emitArray(f, sf1, true)
		emitArray(f, sf2, false)
		fmt.Fprintln(f, "    },")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func emitArray(f *os.File, vals [11]int16, comma bool) {
	fmt.Fprintf(f, "     {")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	if comma {
		fmt.Fprintln(f, "},")
	} else {
		fmt.Fprintln(f, "}")
	}
}
