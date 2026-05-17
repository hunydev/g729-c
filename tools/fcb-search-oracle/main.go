package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/fcb"
	"github.com/hunydev/g729/internal/fcbsearch"
)

const vectorCount = 5

type vector struct {
	x, y, h        [40]int16
	gp            int16
	intLag        int16
	prevGp        int16
	limit         int
	xPrime        [40]int16
	hSearch       [40]int16
	d             [40]int32
	signs         [40]int16
	dAbs          [40]int32
	phi           [40][40]int32
	fullPositions [4]int8
	fullSum       [2]int64
	thPositions   [4]int8
	thSum         [2]int64
	thEntered     int
	code          [40]int16
	z             [40]int16
	s             uint8
	c             uint16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/fcb_search_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "fcb-search-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "fcb-search-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	emitHeader(f, generate())
}

func generate() []vector {
	out := make([]vector, 0, vectorCount)
	for seed := 0; seed < vectorCount; seed++ {
		var v vector
		v.gp = []int16{0, 4096, 8192, 12000, 19661}[seed]
		v.intLag = []int16{20, 25, 38, 39, 45}[seed]
		v.prevGp = []int16{-2000, 3277, 9000, 13017, 20000}[seed]
		v.limit = []int{0, 20, 64, 180, 512}[seed]
		for n := 0; n < 40; n++ {
			v.x[n] = signal(seed, n, 1800, 37, 17)
			v.y[n] = signal(seed+3, n, 1300, 29, 23)
			v.h[n] = impulseLike(seed, n)
		}

		fcbsearch.AdjustedTarget(&v.x, &v.y, v.gp, &v.xPrime)
		v.hSearch = v.h
		fcb.ApplyPitchEnhancement(&v.hSearch, int(v.intLag), fcb.ClampPitchGainForEnhancement(v.prevGp))
		fcbsearch.CorrelationD(&v.xPrime, &v.hSearch, &v.d)
		fcbsearch.SignsFromD(&v.d, &v.signs, &v.dAbs)
		fcbsearch.PhiPrime(&v.hSearch, &v.signs, &v.phi)
		fcbsearch.SearchDepthFirst(&v.dAbs, &v.phi, &v.fullPositions, &v.fullSum)
		v.thEntered = fcbsearch.SearchDepthFirstThresholdScanEntered(&v.dAbs, &v.phi, &v.thPositions, &v.thSum, v.limit)
		fcbsearch.BuildCode(&v.thPositions, &v.signs, v.intLag, v.prevGp, &v.code)
		fcbsearch.FilterCode(&v.code, &v.h, &v.z)
		v.s = fcbsearch.PackS(&v.thPositions, &v.signs)
		v.c = fcbsearch.PackC(&v.thPositions)
		out = append(out, v)
	}
	return out
}

func signal(seed, n, amp, p1, p2 int) int16 {
	x := float64(seed*40 + n)
	v := float64(amp)*math.Sin(2*math.Pi*x/float64(p1)) +
		float64(amp/2)*math.Cos(2*math.Pi*(x+float64(seed))/float64(p2))
	v *= 0.45 + 0.55*math.Sin(2*math.Pi*(x+float64(seed+5))/160)
	return int16(math.Round(v))
}

func impulseLike(seed, n int) int16 {
	if n == 0 {
		return 4096
	}
	x := float64(seed*40 + n)
	v := 1700 * math.Pow(0.86, float64(n)) * math.Sin(2*math.Pi*x/19)
	return int16(math.Round(v))
}

func emitHeader(f *os.File, vectors []vector) {
	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/fcbsearch. */")
	fmt.Fprintln(f, "#ifndef TEST_FCB_SEARCH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_FCB_SEARCH_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct fcb_search_oracle_vector {")
	fmt.Fprintln(f, "    int16_t x[40];")
	fmt.Fprintln(f, "    int16_t y[40];")
	fmt.Fprintln(f, "    int16_t h[40];")
	fmt.Fprintln(f, "    int16_t gp;")
	fmt.Fprintln(f, "    int16_t int_lag;")
	fmt.Fprintln(f, "    int16_t prev_gp;")
	fmt.Fprintln(f, "    int limit;")
	fmt.Fprintln(f, "    int16_t x_prime[40];")
	fmt.Fprintln(f, "    int16_t h_search[40];")
	fmt.Fprintln(f, "    int32_t d[40];")
	fmt.Fprintln(f, "    int16_t signs[40];")
	fmt.Fprintln(f, "    int32_t d_abs[40];")
	fmt.Fprintln(f, "    int32_t phi[40][40];")
	fmt.Fprintln(f, "    int8_t full_positions[4];")
	fmt.Fprintln(f, "    int64_t full_sum[2];")
	fmt.Fprintln(f, "    int8_t threshold_positions[4];")
	fmt.Fprintln(f, "    int64_t threshold_sum[2];")
	fmt.Fprintln(f, "    int threshold_entered;")
	fmt.Fprintln(f, "    int16_t code[40];")
	fmt.Fprintln(f, "    int16_t z[40];")
	fmt.Fprintln(f, "    uint8_t s;")
	fmt.Fprintln(f, "    uint16_t c;")
	fmt.Fprintln(f, "} fcb_search_oracle_vector;")
	fmt.Fprintf(f, "#define FCB_SEARCH_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "static const fcb_search_oracle_vector FCB_SEARCH_ORACLE_VECTORS[FCB_SEARCH_ORACLE_VECTOR_COUNT] = {")
	for _, v := range vectors {
		fmt.Fprintln(f, "    {")
		emitArray16Line(f, v.x[:])
		emitArray16Line(f, v.y[:])
		emitArray16Line(f, v.h[:])
		fmt.Fprintf(f, "     %d, %d, %d, %d,\n", v.gp, v.intLag, v.prevGp, v.limit)
		emitArray16Line(f, v.xPrime[:])
		emitArray16Line(f, v.hSearch[:])
		emitArray32Line(f, v.d[:])
		emitArray16Line(f, v.signs[:])
		emitArray32Line(f, v.dAbs[:])
		emitMatrix32Line(f, v.phi)
		emitArray8Line(f, v.fullPositions[:])
		emitArray64Line(f, v.fullSum[:])
		emitArray8Line(f, v.thPositions[:])
		emitArray64Line(f, v.thSum[:])
		fmt.Fprintf(f, "     %d,\n", v.thEntered)
		emitArray16Line(f, v.code[:])
		emitArray16Line(f, v.z[:])
		fmt.Fprintf(f, "     %d, %d\n    },\n", v.s, v.c)
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func emitArray16Line(f *os.File, vals []int16) {
	fmt.Fprint(f, "     {")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprintln(f, "},")
}

func emitArray8Line(f *os.File, vals []int8) {
	fmt.Fprint(f, "     {")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprintln(f, "},")
}

func emitArray32Line(f *os.File, vals []int32) {
	fmt.Fprint(f, "     {")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprintln(f, "},")
}

func emitArray64Line(f *os.File, vals []int64) {
	fmt.Fprint(f, "     {")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprintln(f, "},")
}

func emitMatrix32Line(f *os.File, vals [40][40]int32) {
	fmt.Fprintln(f, "     {")
	for i := range vals {
		fmt.Fprint(f, "      {")
		for j, v := range vals[i] {
			fmt.Fprintf(f, "%d", v)
			if j != len(vals[i])-1 {
				fmt.Fprint(f, ", ")
			}
		}
		if i != len(vals)-1 {
			fmt.Fprintln(f, "},")
		} else {
			fmt.Fprintln(f, "}")
		}
	}
	fmt.Fprintln(f, "     },")
}
