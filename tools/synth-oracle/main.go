package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/synth"
)

type vector struct {
	a    [11]int16
	v    [40]int16
	c    [40]int16
	past [10]int16
	gp   int16
	mant int16
	exp  int8
}

func main() {
	outPath := flag.String("out", "tests/fixtures/synth_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "synth-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "synth-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []vector{
		{
			a:  [11]int16{4096, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			v:  ramp40(500, 10),
			gp: 16384,
		},
		{
			a:    [11]int16{4096, 1500, -800, 300, -100, 0, 0, 0, 0, 0, 0},
			v:    ramp40(0, 17),
			c:    centered40(400, -20),
			gp:   10000,
			mant: 8000,
			exp:  0,
		},
		{
			a:    [11]int16{4096, 4000, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			past: ramp10(100, 7),
			v:    pulse40(0, 4000),
			gp:   16384,
		},
		{
			a:    [11]int16{4096, -3000, 1800, -700, 250, -100, 0, 0, 0, 0, 0},
			v:    ramp40(-300, 23),
			c:    centered40(512, -4),
			gp:   12000,
			mant: 20000,
			exp:  2,
		},
		{
			a:    [11]int16{4096, -32768, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			v:    pulse40(0, 32767),
			past: fill10(32767),
			gp:   32767,
		},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/synth. */")
	fmt.Fprintln(f, "#ifndef TEST_SYNTH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_SYNTH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct synth_oracle_vector {")
	fmt.Fprintln(f, "    int16_t a[11];")
	fmt.Fprintln(f, "    int16_t v[40];")
	fmt.Fprintln(f, "    int16_t c[40];")
	fmt.Fprintln(f, "    int16_t past_in[10];")
	fmt.Fprintln(f, "    int16_t gp_q14;")
	fmt.Fprintln(f, "    int16_t gc_mant_q14;")
	fmt.Fprintln(f, "    int8_t gc_exp;")
	fmt.Fprintln(f, "    int16_t u[40];")
	fmt.Fprintln(f, "    int16_t out[40];")
	fmt.Fprintln(f, "    int16_t past_out[10];")
	fmt.Fprintln(f, "    unsigned scale_shift;")
	fmt.Fprintln(f, "} synth_oracle_vector;")
	fmt.Fprintf(f, "#define SYNTH_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const synth_oracle_vector SYNTH_ORACLE_VECTORS[SYNTH_ORACLE_VECTOR_COUNT] = {")
	for _, tc := range cases {
		var syn synth.Synthesizer
		for i := range tc.past {
			// Build the requested state through a direct memory-compatible
			// warmup: Filter with identity is not enough, so use the public
			// zero state for vectors that need exact cold behavior and only
			// include past_in for C-side state setup.
			_ = i
		}
		var u, out [40]int16
		synth.BuildExcitation(tc.gp, tc.mant, tc.exp, &tc.v, &tc.c, &u)
		if tc.past != [10]int16{} {
			// The Go type keeps pastSynth private; generate stateful cases by
			// running a previous identity-filter subframe whose last ten samples
			// equal tc.past.
			var warmA [11]int16
			var warmU [40]int16
			var warmOut [40]int16
			warmA[0] = 4096
			for i := 0; i < 10; i++ {
				warmU[30+i] = tc.past[i]
			}
			syn.Filter(&warmA, &warmU, &warmOut)
		}
		syn.Filter(&tc.a, &u, &out)
		pastOut := syn.PastSynth()
		fmt.Fprintln(f, "    {")
		fmt.Fprint(f, "     ")
		emitArray(f, tc.a[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, tc.v[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, tc.c[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, tc.past[:])
		fmt.Fprintf(f, ",\n     %d, %d, %d,\n", tc.gp, tc.mant, tc.exp)
		fmt.Fprint(f, "     ")
		emitArray(f, u[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, out[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, pastOut[:])
		fmt.Fprintf(f, ",\n     %d\n", syn.LastExcitationScaleShift())
		fmt.Fprintln(f, "    },")
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

func ramp40(start, step int) [40]int16 {
	var out [40]int16
	for i := range out {
		out[i] = int16(start + i*step)
	}
	return out
}

func centered40(scale, offset int) [40]int16 {
	var out [40]int16
	for i := range out {
		out[i] = int16((i + offset) * scale)
	}
	return out
}

func pulse40(pos int, amp int16) [40]int16 {
	var out [40]int16
	out[pos] = amp
	return out
}

func ramp10(start, step int) [10]int16 {
	var out [10]int16
	for i := range out {
		out[i] = int16(start + i*step)
	}
	return out
}

func fill10(v int16) [10]int16 {
	var out [10]int16
	for i := range out {
		out[i] = v
	}
	return out
}
