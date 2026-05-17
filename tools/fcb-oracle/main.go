package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/fcb"
)

type vector struct {
	positions uint16
	signs     uint8
	t         int
	beta      int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/fcb_oracle_vectors.h", "output C header path")
	flag.Parse()

	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "fcb-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "fcb-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []vector{
		{positions: 0, signs: 0x0F, t: 40, beta: 0},
		{positions: 0x053C, signs: 0x09, t: 40, beta: 0},
		{positions: 0, signs: 0x01, t: 20, beta: 8192},
		{positions: 0x1234, signs: 0x0A, t: 25, beta: 10000},
		{positions: 0x1FFF, signs: 0x00, t: 5, beta: 13017},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/fcb. */")
	fmt.Fprintln(f, "#ifndef TEST_FCB_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_FCB_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct fcb_oracle_vector {")
	fmt.Fprintln(f, "    uint16_t positions;")
	fmt.Fprintln(f, "    uint8_t signs;")
	fmt.Fprintln(f, "    int t;")
	fmt.Fprintln(f, "    int16_t beta_q14;")
	fmt.Fprintln(f, "    int16_t code[40];")
	fmt.Fprintln(f, "} fcb_oracle_vector;")
	fmt.Fprintf(f, "#define FCB_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const fcb_oracle_vector FCB_ORACLE_VECTORS[FCB_ORACLE_VECTOR_COUNT] = {")
	for _, tc := range cases {
		var code [40]int16
		fcb.Decode(fcb.Indices{Positions: tc.positions, Signs: tc.signs}, tc.t, tc.beta, &code)
		fmt.Fprintf(f, "    {%d, %d, %d, %d, {", tc.positions, tc.signs, tc.t, tc.beta)
		for i, v := range code {
			fmt.Fprintf(f, "%d", v)
			if i != len(code)-1 {
				fmt.Fprint(f, ", ")
			}
		}
		fmt.Fprintln(f, "}},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}
