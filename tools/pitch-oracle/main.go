package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/pitch"
)

type vector struct {
	tInt  int
	tFrac int
	seed  int
}

func main() {
	outPath := flag.String("out", "tests/fixtures/pitch_oracle_vectors.h", "output C header path")
	flag.Parse()

	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "pitch-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "pitch-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []vector{
		{tInt: 60, tFrac: 0, seed: 37},
		{tInt: 60, tFrac: -1, seed: 37},
		{tInt: 60, tFrac: 1, seed: 37},
		{tInt: 20, tFrac: 0, seed: 73},
		{tInt: 20, tFrac: 1, seed: 73},
		{tInt: 39, tFrac: 0, seed: 101},
		{tInt: 143, tFrac: 0, seed: 17},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/pitch. */")
	fmt.Fprintln(f, "#ifndef TEST_PITCH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_PITCH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "#define PITCH_ORACLE_PAST_EXC_LEN 250")
	fmt.Fprintln(f, "typedef struct pitch_oracle_vector {")
	fmt.Fprintln(f, "    int t_int;")
	fmt.Fprintln(f, "    int t_frac;")
	fmt.Fprintln(f, "    int16_t past_exc[PITCH_ORACLE_PAST_EXC_LEN];")
	fmt.Fprintln(f, "    int16_t v[40];")
	fmt.Fprintln(f, "} pitch_oracle_vector;")
	fmt.Fprintf(f, "#define PITCH_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const pitch_oracle_vector PITCH_ORACLE_VECTORS[PITCH_ORACLE_VECTOR_COUNT] = {")
	for _, tc := range cases {
		var past [250]int16
		var v [40]int16
		for i := range past {
			past[i] = int16((i*tc.seed)%5000 - 2500)
		}
		pitch.AdaptiveCodebook(tc.tInt, tc.tFrac, past[:], &v)
		fmt.Fprintf(f, "    {%d, %d, ", tc.tInt, tc.tFrac)
		emitArray(f, past[:])
		fmt.Fprint(f, ", ")
		emitArray(f, v[:])
		fmt.Fprintln(f, "},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func emitArray(f *os.File, vals []int16) {
	fmt.Fprint(f, "{")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
}
