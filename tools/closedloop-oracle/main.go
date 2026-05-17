package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/fixed"
	"github.com/hunydev/g729/internal/lpc"
	"github.com/hunydev/g729/internal/lsp"
	"github.com/hunydev/g729/internal/pitch/closedloop"
)

const vectorCount = 8

type vector struct {
	subframe    int
	centre      int16
	intT1       int16
	aHat        [lpc.LPCOrder + 1]int16
	speech      [closedloop.SubframeLen]int16
	residualMem [10]int16
	swMem       [10]int16
	exc         [193]int16
	residual    [closedloop.SubframeLen]int16
	x           [closedloop.SubframeLen]int16
	h           [closedloop.SubframeLen]int16
	xb          [closedloop.SubframeLen]int16
	tmin        int16
	tmax        int16
	searchLag   int16
	searchRN    int32
	refinedLag  int16
	refinedFrac int8
	v           [closedloop.SubframeLen]int16
	y           [closedloop.SubframeLen]int16
	gp          int16
	p0          uint8
	p1          uint8
	p2          uint8
}

func main() {
	outPath := flag.String("out", "tests/fixtures/closedloop_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "closedloop-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "closedloop-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	vectors := generate()
	emitHeader(f, vectors)
}

func generate() []vector {
	cases := []struct {
		subframe int
		centre   int16
		intT1    int16
	}{
		{0, 20, 0},
		{0, 38, 0},
		{0, 84, 0},
		{0, 104, 0},
		{1, 20, 20},
		{1, 51, 51},
		{1, 104, 104},
		{1, 139, 139},
	}

	indices := []lsp.Indices{
		{L0: 1, L1: 11, L2: 11, L3: 15},
		{L0: 1, L1: 34, L2: 11, L3: 1},
		{L0: 1, L1: 1, L2: 1, L3: 31},
		{L0: 0, L1: 8, L2: 11, L3: 0},
	}
	var dec lsp.Decoder
	var aHats [][lpc.LPCOrder + 1]int16
	for _, idx := range indices {
		sf1, sf2 := dec.Decode(idx)
		aHats = append(aHats, sf1, sf2)
	}

	out := make([]vector, 0, len(cases))
	for i, tc := range cases {
		var v vector
		v.subframe = tc.subframe
		v.centre = tc.centre
		v.intT1 = tc.intT1
		v.aHat = aHats[i%len(aHats)]
		for n := 0; n < closedloop.SubframeLen; n++ {
			v.speech[n] = signal(i, n, 1500, 43, 19)
		}
		for n := 0; n < 10; n++ {
			v.residualMem[n] = signal(i+2, n, 900, 17, 11)
			v.swMem[n] = signal(i+4, n, 700, 23, 7)
		}
		for n := range v.exc {
			v.exc[n] = signal(i+6, n, 2200, 57, 29)
		}

		lpResidualSubframe(&v.speech, &v.aHat, &v.residualMem, &v.residual)
		closedloop.TargetSignal(&v.aHat, &v.residual, &v.swMem, &v.x)
		closedloop.ImpulseResponse(&v.aHat, &v.h)
		closedloop.BackwardFilter(&v.x, &v.h, &v.xb)
		if v.subframe == 0 {
			v.tmin, v.tmax = closedloop.Subframe1Window(v.centre)
		} else {
			v.tmin, v.tmax = closedloop.Subframe2Window(v.centre)
		}
		v.searchLag, v.searchRN = closedloop.SearchInteger(&v.xb, v.exc[:], v.centre, v.subframe)
		if v.subframe == 0 {
			v.refinedLag, v.refinedFrac = closedloop.RefineFractionSubframe1(&v.xb, v.exc[:], v.searchLag)
			v.p1 = closedloop.EncodeP1(v.refinedLag, v.refinedFrac)
			v.p0 = closedloop.EncodeP0(v.p1)
		} else {
			v.refinedLag, v.refinedFrac = closedloop.RefineFractionSubframe2(&v.xb, v.exc[:], v.searchLag, v.intT1)
			v.p2 = closedloop.EncodeP2(v.refinedLag, v.refinedFrac, v.tmin)
		}
		closedloop.AdaptiveVector(v.exc[:], v.refinedLag, v.refinedFrac, &v.v)
		v.gp = closedloop.GpAndY(&v.x, &v.v, &v.h, &v.y)
		out = append(out, v)
	}
	return out
}

func lpResidualSubframe(s *[closedloop.SubframeLen]int16, aHat *[lpc.LPCOrder + 1]int16, mem *[10]int16, r *[closedloop.SubframeLen]int16) {
	for n := 0; n < closedloop.SubframeLen; n++ {
		acc := fixed.LMult(s[n], aHat[0])
		for i := 1; i <= lpc.LPCOrder; i++ {
			var sni int16
			if n-i >= 0 {
				sni = s[n-i]
			} else {
				sni = mem[10+n-i]
			}
			acc = fixed.LMac(acc, aHat[i], sni)
		}
		r[n] = fixed.Round(fixed.LShl(acc, 3))
	}
}

func signal(seed, n, amp, p1, p2 int) int16 {
	x := float64(seed*closedloop.SubframeLen + n)
	v := float64(amp)*math.Sin(2*math.Pi*x/float64(p1)) +
		float64(amp/3)*math.Cos(2*math.Pi*x/float64(p2))
	v *= 0.35 + 0.65*math.Sin(2*math.Pi*(x+float64(seed))/160)
	return int16(math.Round(v))
}

func emitHeader(f *os.File, vectors []vector) {
	fmt.Fprintln(f, "/* Generated from /home/exedev/g729 encoder closed-loop pitch package. */")
	fmt.Fprintln(f, "#ifndef TEST_CLOSEDLOOP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_CLOSEDLOOP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct closedloop_oracle_vector {")
	fmt.Fprintln(f, "    int subframe;")
	fmt.Fprintln(f, "    int16_t centre;")
	fmt.Fprintln(f, "    int16_t int_t1;")
	fmt.Fprintln(f, "    int16_t a_hat[11];")
	fmt.Fprintln(f, "    int16_t speech[40];")
	fmt.Fprintln(f, "    int16_t residual_mem[10];")
	fmt.Fprintln(f, "    int16_t sw_mem[10];")
	fmt.Fprintln(f, "    int16_t exc[193];")
	fmt.Fprintln(f, "    int16_t residual[40];")
	fmt.Fprintln(f, "    int16_t x[40];")
	fmt.Fprintln(f, "    int16_t h[40];")
	fmt.Fprintln(f, "    int16_t xb[40];")
	fmt.Fprintln(f, "    int16_t tmin;")
	fmt.Fprintln(f, "    int16_t tmax;")
	fmt.Fprintln(f, "    int16_t search_lag;")
	fmt.Fprintln(f, "    int32_t search_rn;")
	fmt.Fprintln(f, "    int16_t refined_lag;")
	fmt.Fprintln(f, "    int8_t refined_frac;")
	fmt.Fprintln(f, "    int16_t v[40];")
	fmt.Fprintln(f, "    int16_t y[40];")
	fmt.Fprintln(f, "    int16_t gp;")
	fmt.Fprintln(f, "    uint8_t p0;")
	fmt.Fprintln(f, "    uint8_t p1;")
	fmt.Fprintln(f, "    uint8_t p2;")
	fmt.Fprintln(f, "} closedloop_oracle_vector;")
	fmt.Fprintf(f, "#define CLOSEDLOOP_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "static const closedloop_oracle_vector CLOSEDLOOP_ORACLE_VECTORS[CLOSEDLOOP_ORACLE_VECTOR_COUNT] = {")
	for _, v := range vectors {
		fmt.Fprintln(f, "    {")
		fmt.Fprintf(f, "     %d, %d, %d,\n     ", v.subframe, v.centre, v.intT1)
		emitArray(f, v.aHat[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.speech[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.residualMem[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.swMem[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.exc[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.residual[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.x[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.h[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.xb[:])
		fmt.Fprintf(f, ",\n     %d, %d, %d, %d, %d, %d,\n     ",
			v.tmin, v.tmax, v.searchLag, v.searchRN, v.refinedLag, v.refinedFrac)
		emitArray(f, v.v[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, v.y[:])
		fmt.Fprintf(f, ",\n     %d, %d, %d, %d\n    },\n", v.gp, v.p0, v.p1, v.p2)
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
