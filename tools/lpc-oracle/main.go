package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/lpc"
)

type vector struct {
	name   string
	speech [lpc.LPCWindowSamples]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/lpc_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "lpc-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "lpc-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	vectors := []vector{
		{name: "silence", speech: signal(silence)},
		{name: "impulse", speech: signal(impulse)},
		{name: "dc_step", speech: signal(dcStep)},
		{name: "speech_like", speech: signal(speechLike)},
		{name: "alternating_clip", speech: signal(alternatingClip)},
		{name: "pseudo_random", speech: signal(pseudoRandom)},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/lpc.Analyzer. */")
	fmt.Fprintln(f, "#ifndef TEST_LPC_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_LPC_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct lpc_oracle_vector {")
	fmt.Fprintln(f, "    const char *name;")
	fmt.Fprintln(f, "    int16_t speech[240];")
	fmt.Fprintln(f, "    int16_t a[11];")
	fmt.Fprintln(f, "} lpc_oracle_vector;")
	fmt.Fprintf(f, "#define LPC_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "static const lpc_oracle_vector LPC_ORACLE_VECTORS[LPC_ORACLE_VECTOR_COUNT] = {")
	var analyzer lpc.Analyzer
	for _, vec := range vectors {
		var a [lpc.LPCOrder + 1]int16
		if err := analyzer.Analyze(&vec.speech, &a); err != nil {
			fmt.Fprintf(os.Stderr, "lpc-oracle: analyze %s: %v\n", vec.name, err)
			os.Exit(1)
		}
		fmt.Fprintf(f, "    {\"%s\",\n     ", vec.name)
		emitArray(f, vec.speech[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, a[:])
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

func signal(fn func(sample int) int16) [lpc.LPCWindowSamples]int16 {
	var out [lpc.LPCWindowSamples]int16
	for i := range out {
		out[i] = fn(i)
	}
	return out
}

func silence(sample int) int16 {
	_ = sample
	return 0
}

func impulse(sample int) int16 {
	if sample == 160 {
		return 24000
	}
	return 0
}

func dcStep(sample int) int16 {
	_ = sample
	return 2000
}

func speechLike(sample int) int16 {
	x := float64(sample)
	env := 0.55 + 0.45*math.Sin(2*math.Pi*x/160)
	v := env * (2100*math.Sin(2*math.Pi*x/53) + 900*math.Sin(2*math.Pi*x/17))
	return int16(math.Round(v))
}

func alternatingClip(sample int) int16 {
	if sample%2 == 0 {
		return 32767
	}
	return -32768
}

func pseudoRandom(sample int) int16 {
	x := uint32(sample + 1)
	x ^= x << 13
	x ^= x >> 17
	x ^= x << 5
	return int16(int32(x%20001) - 10000)
}
