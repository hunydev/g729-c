package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/postfilter"
)

type call struct {
	a [11]int16
	t int
	s [40]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/postfilter_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "postfilter-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "postfilter-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []call{
		{a: identityA(), t: 60},
		{a: identityA(), t: 60, s: pulse(0, 9000)},
		{a: mildA(), t: 61, s: ramp(-900, 53)},
		{a: voicedA(), t: 142, s: alternating(2600, -2100)},
		{a: mildA(), t: 17, s: centered(384, -19)},
		{a: identityA(), t: 80, s: pulse(23, -14000)},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/postfilter. */")
	fmt.Fprintln(f, "#ifndef TEST_POSTFILTER_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_POSTFILTER_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct postfilter_oracle_vector {")
	fmt.Fprintln(f, "    int16_t a[11];")
	fmt.Fprintln(f, "    int t_int;")
	fmt.Fprintln(f, "    int16_t s[40];")
	fmt.Fprintln(f, "    int16_t out[40];")
	fmt.Fprintln(f, "    int16_t past_s[10];")
	fmt.Fprintln(f, "    int16_t past_residual[183];")
	fmt.Fprintln(f, "    int16_t past_synth_post[10];")
	fmt.Fprintln(f, "    int16_t past_tilt_input;")
	fmt.Fprintln(f, "    int32_t agc_gain_prev;")
	fmt.Fprintln(f, "    int initialized;")
	fmt.Fprintln(f, "} postfilter_oracle_vector;")
	fmt.Fprintf(f, "#define POSTFILTER_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const postfilter_oracle_vector POSTFILTER_ORACLE_VECTORS[POSTFILTER_ORACLE_VECTOR_COUNT] = {")
	var pf postfilter.Postfilter
	for _, tc := range cases {
		taps := pf.FilterWithTaps(&tc.a, tc.t, &tc.s)
		fmt.Fprintln(f, "    {")
		fmt.Fprint(f, "     ")
		emitArray16(f, tc.a[:])
		fmt.Fprintf(f, ",\n     %d,\n", tc.t)
		fmt.Fprint(f, "     ")
		emitArray16(f, tc.s[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, taps.Output[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, taps.PastSAfter[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, taps.PastResidualAfter[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, taps.PastSynthPostAfter[:])
		fmt.Fprintf(f, ",\n     %d,\n     %d,\n     %d\n",
			taps.PastTiltInputAfter,
			taps.AGCGainAfterQ24,
			boolInt(taps.InitializedAfter))
		fmt.Fprintln(f, "    },")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func boolInt(v bool) int {
	if v {
		return 1
	}
	return 0
}

func emitArray16(f *os.File, vals []int16) {
	fmt.Fprint(f, "{")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
}

func identityA() [11]int16 {
	return [11]int16{4096}
}

func mildA() [11]int16 {
	return [11]int16{4096, 1300, -620, 270, -120, 55, -25, 11, -5, 2, -1}
}

func voicedA() [11]int16 {
	return [11]int16{4096, -2600, 1750, -930, 460, -210, 96, -43, 19, -8, 3}
}

func pulse(pos int, amp int16) [40]int16 {
	var out [40]int16
	out[pos] = amp
	return out
}

func ramp(start, step int) [40]int16 {
	var out [40]int16
	for i := range out {
		out[i] = int16(start + i*step)
	}
	return out
}

func centered(scale, offset int) [40]int16 {
	var out [40]int16
	for i := range out {
		out[i] = int16((i + offset) * scale)
	}
	return out
}

func alternating(a, b int16) [40]int16 {
	var out [40]int16
	for i := range out {
		if i%2 == 0 {
			out[i] = a
		} else {
			out[i] = b
		}
	}
	return out
}
