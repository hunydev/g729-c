package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/gain"
	"github.com/hunydev/g729/internal/gainquant"
)

const vectorCount = 6

type searchOut struct {
	ga, gb         uint8
	gaBits, gbBits uint8
	gpQ14          int16
	gammaCQ13      int32
}

type reconOut struct {
	gpQ14     int16
	gcMantQ14 int16
	gcExp     int8
}

type vector struct {
	past             [4]int16
	code             [40]int16
	x, y, z          [40]int16
	oldExc           [154]int16
	gpcPredQ12       int32
	targetBits       uint
	tameInputQ14     int16
	predGcQ12        int32
	predGcQ12Wide    int32
	search           searchOut
	targetBitsSearch searchOut
	floatSearch      searchOut
	recon            reconOut
	reconWide        reconOut
	updatedPast      [4]int16
	tameQ14          int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/gain_quant_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "gain-quant-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "gain-quant-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	emitHeader(f, generate())
}

func generate() []vector {
	pastCases := [][4]int16{
		{gain.PastErrorsDefault, gain.PastErrorsDefault, gain.PastErrorsDefault, gain.PastErrorsDefault},
		{179, -980, gain.PastErrorsDefault, -2048},
		{-3200, 640, 1100, -700},
		{3000, 1200, -2500, 500},
		{-9000, -5000, -2500, -1000},
		{9000, 6000, 3000, 0},
	}
	targetBits := []uint{0, 1, 12, 14, 20, 30}
	tameInputs := []int16{12000, 15565, 16000, 19661, -500, 20000}

	out := make([]vector, 0, vectorCount)
	for seed := 0; seed < vectorCount; seed++ {
		var v vector
		v.past = pastCases[seed]
		v.code = codebook(seed)
		v.targetBits = targetBits[seed]
		v.tameInputQ14 = tameInputs[seed]
		for n := 0; n < 40; n++ {
			v.x[n] = signal(seed, n, 2600, 31, 17)
			v.y[n] = signal(seed+2, n, 1800, 27, 19)
			v.z[n] = signal(seed+5, n, 3200, 23, 13)
		}
		for n := 0; n < 154; n++ {
			if seed%2 == 1 {
				v.oldExc[n] = int16(7600 + (n%5)*120)
			} else {
				v.oldExc[n] = signal(seed+7, n, 1700, 47, 29)
			}
		}

		v.predGcQ12 = gainquant.PredictedGcQ12(&v.past, &v.code)
		v.predGcQ12Wide = gainquant.PredictedGcQ12Wide(&v.past, &v.code)
		switch seed {
		case 0:
			v.gpcPredQ12 = 0
		case 1:
			v.gpcPredQ12 = v.predGcQ12
		case 2:
			v.gpcPredQ12 = v.predGcQ12Wide
		case 3:
			v.gpcPredQ12 = v.predGcQ12Wide / 2
		case 4:
			v.gpcPredQ12 = v.predGcQ12Wide + 1234
		default:
			v.gpcPredQ12 = 4096
		}

		ga, gb, gp, gammaC := gainquant.SearchConjugate(&v.x, &v.y, &v.z, v.gpcPredQ12)
		v.search = makeSearchOut(ga, gb, gp, gammaC)
		ga, gb, gp, gammaC = gainquant.SearchConjugatePreselectTargetBits(&v.x, &v.y, &v.z, v.gpcPredQ12, v.targetBits)
		v.targetBitsSearch = makeSearchOut(ga, gb, gp, gammaC)
		ga, gb, gp, gammaC = gainquant.SearchConjugatePreselectFloatCenter(&v.x, &v.y, &v.z, v.gpcPredQ12)
		v.floatSearch = makeSearchOut(ga, gb, gp, gammaC)
		gp, mant, exp := gainquant.Reconstruct(&v.past, &v.code, v.search.ga, v.search.gb)
		v.recon = reconOut{gpQ14: gp, gcMantQ14: mant, gcExp: exp}
		gp, mant, exp = gainquant.ReconstructWide(&v.past, &v.code, v.search.ga, v.search.gb)
		v.reconWide = reconOut{gpQ14: gp, gcMantQ14: mant, gcExp: exp}
		v.updatedPast = v.past
		gainquant.UpdatePastQuaEn(&v.updatedPast, v.search.gammaCQ13)
		v.tameQ14 = gainquant.Tame(v.tameInputQ14, &v.oldExc)

		out = append(out, v)
	}
	return out
}

func makeSearchOut(ga, gb uint8, gpQ14 int16, gammaCQ13 int32) searchOut {
	gaBits, gbBits := gainquant.PackGains(ga, gb)
	return searchOut{
		ga: ga, gb: gb, gaBits: gaBits, gbBits: gbBits,
		gpQ14: gpQ14, gammaCQ13: gammaCQ13,
	}
}

func codebook(seed int) [40]int16 {
	var c [40]int16
	switch seed {
	case 0:
		return c
	case 1:
		c[3] = 8191
		c[17] = -8192
	case 2:
		c[0] = 4096
		c[11] = -4096
		c[24] = 8191
		c[39] = -8192
	case 3:
		for i := range c {
			c[i] = int16((i%7 - 3) * 384)
		}
	case 4:
		c[5] = 8191
		c[6] = 8191
		c[25] = -8192
		c[26] = -8192
	default:
		for i := range c {
			c[i] = int16(((i*seed)%11 - 5) * 512)
		}
	}
	return c
}

func signal(seed, n, amp, p1, p2 int) int16 {
	x := float64(seed*43 + n)
	v := float64(amp)*math.Sin(2*math.Pi*x/float64(p1)) +
		float64(amp/2)*math.Cos(2*math.Pi*(x+float64(seed))/float64(p2))
	v *= 0.55 + 0.45*math.Sin(2*math.Pi*(x+float64(seed+3))/173)
	return int16(math.Round(v))
}

func emitHeader(f *os.File, vectors []vector) {
	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/gainquant. */")
	fmt.Fprintln(f, "#ifndef TEST_GAIN_QUANT_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_GAIN_QUANT_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct gain_quant_search_oracle {")
	fmt.Fprintln(f, "    uint8_t ga, gb, ga_bits, gb_bits;")
	fmt.Fprintln(f, "    int16_t gp_q14;")
	fmt.Fprintln(f, "    int32_t gamma_c_q13;")
	fmt.Fprintln(f, "} gain_quant_search_oracle;")
	fmt.Fprintln(f, "typedef struct gain_quant_recon_oracle {")
	fmt.Fprintln(f, "    int16_t gp_q14;")
	fmt.Fprintln(f, "    int16_t gc_mant_q14;")
	fmt.Fprintln(f, "    int8_t gc_exp;")
	fmt.Fprintln(f, "} gain_quant_recon_oracle;")
	fmt.Fprintln(f, "typedef struct gain_quant_oracle_vector {")
	fmt.Fprintln(f, "    int16_t past[4];")
	fmt.Fprintln(f, "    int16_t code[40];")
	fmt.Fprintln(f, "    int16_t x[40];")
	fmt.Fprintln(f, "    int16_t y[40];")
	fmt.Fprintln(f, "    int16_t z[40];")
	fmt.Fprintln(f, "    int16_t old_exc[154];")
	fmt.Fprintln(f, "    int32_t gpc_pred_q12;")
	fmt.Fprintln(f, "    unsigned target_bits;")
	fmt.Fprintln(f, "    int16_t tame_input_q14;")
	fmt.Fprintln(f, "    int32_t pred_gc_q12;")
	fmt.Fprintln(f, "    int32_t pred_gc_q12_wide;")
	fmt.Fprintln(f, "    gain_quant_search_oracle search;")
	fmt.Fprintln(f, "    gain_quant_search_oracle target_bits_search;")
	fmt.Fprintln(f, "    gain_quant_search_oracle float_search;")
	fmt.Fprintln(f, "    gain_quant_recon_oracle recon;")
	fmt.Fprintln(f, "    gain_quant_recon_oracle recon_wide;")
	fmt.Fprintln(f, "    int16_t updated_past[4];")
	fmt.Fprintln(f, "    int16_t tame_q14;")
	fmt.Fprintln(f, "} gain_quant_oracle_vector;")
	fmt.Fprintf(f, "#define GAIN_QUANT_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "static const gain_quant_oracle_vector GAIN_QUANT_ORACLE_VECTORS[GAIN_QUANT_ORACLE_VECTOR_COUNT] = {")
	for _, v := range vectors {
		fmt.Fprintln(f, "    {")
		emitArray16Line(f, v.past[:])
		emitArray16Line(f, v.code[:])
		emitArray16Line(f, v.x[:])
		emitArray16Line(f, v.y[:])
		emitArray16Line(f, v.z[:])
		emitArray16Line(f, v.oldExc[:])
		fmt.Fprintf(f, "     %d, %d, %d, %d, %d,\n", v.gpcPredQ12, v.targetBits, v.tameInputQ14, v.predGcQ12, v.predGcQ12Wide)
		emitSearch(f, v.search)
		emitSearch(f, v.targetBitsSearch)
		emitSearch(f, v.floatSearch)
		emitRecon(f, v.recon)
		emitRecon(f, v.reconWide)
		emitArray16Line(f, v.updatedPast[:])
		fmt.Fprintf(f, "     %d\n", v.tameQ14)
		fmt.Fprintln(f, "    },")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func emitSearch(f *os.File, s searchOut) {
	fmt.Fprintf(f, "     {%d, %d, %d, %d, %d, %d},\n", s.ga, s.gb, s.gaBits, s.gbBits, s.gpQ14, s.gammaCQ13)
}

func emitRecon(f *os.File, r reconOut) {
	fmt.Fprintf(f, "     {%d, %d, %d},\n", r.gpQ14, r.gcMantQ14, r.gcExp)
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
