package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/gain"
)

type vector struct {
	ga   uint8
	gb   uint8
	code [40]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/gain_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "gain-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "gain-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []vector{
		{ga: 3, gb: 7, code: pulse(5, 8192)},
		{ga: 0, gb: 0, code: pulse(0, 8192)},
		{ga: 2, gb: 5, code: twoPulses()},
		{ga: 7, gb: 15, code: rampCode()},
		{ga: 1, gb: 2, code: [40]int16{}},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/gain. */")
	fmt.Fprintln(f, "#ifndef TEST_GAIN_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_GAIN_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct gain_oracle_vector {")
	fmt.Fprintln(f, "    uint8_t ga, gb;")
	fmt.Fprintln(f, "    int16_t code[40];")
	fmt.Fprintln(f, "    int16_t gp_q14;")
	fmt.Fprintln(f, "    int16_t gc_mant_q14;")
	fmt.Fprintln(f, "    int8_t gc_exp;")
	fmt.Fprintln(f, "    int16_t past_errors[4];")
	fmt.Fprintln(f, "} gain_oracle_vector;")
	fmt.Fprintf(f, "#define GAIN_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const gain_oracle_vector GAIN_ORACLE_VECTORS[GAIN_ORACLE_VECTOR_COUNT] = {")
	var dec gain.Decoder
	for _, tc := range cases {
		gp, mant, exp := dec.Decode(gain.Indices{GA: tc.ga, GB: tc.gb}, &tc.code)
		past := dec.PredictorErrors()
		fmt.Fprintf(f, "    {%d, %d, ", tc.ga, tc.gb)
		emitArray(f, tc.code[:])
		fmt.Fprintf(f, ", %d, %d, %d, ", gp, mant, exp)
		emitArray(f, past[:])
		fmt.Fprintln(f, "},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func pulse(pos int, amp int16) [40]int16 {
	var c [40]int16
	c[pos] = amp
	return c
}

func twoPulses() [40]int16 {
	var c [40]int16
	c[0] = 8191
	c[20] = -8192
	return c
}

func rampCode() [40]int16 {
	var c [40]int16
	for i := range c {
		c[i] = int16((i%9 - 4) * 512)
	}
	return c
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
