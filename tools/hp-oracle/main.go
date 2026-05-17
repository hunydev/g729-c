package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/fixed"
)

const (
	subframeLen = 40

	hpB0Q13 = 7699
	hpB1Q13 = -15398
	hpB2Q13 = 7699

	hpFeedbackA1Q13 = 15836
	hpFeedbackA2Q13 = -7667
)

type hpState struct {
	x [2]int16
	y [2]int32
}

type vector struct {
	state hpState
	in    [subframeLen]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/hp_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "hp-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "hp-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	cases := []vector{
		{},
		{in: pulse(0, 10000)},
		{in: ramp(-1000, 73)},
		{state: hpState{x: [2]int16{200, -300}, y: [2]int32{1234567, -7654321}}, in: centered(512, -20)},
		{state: hpState{x: [2]int16{32767, -32768}, y: [2]int32{500000000, -400000000}}, in: alternating(32767, -32768)},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/decoder/hpfilter.go. */")
	fmt.Fprintln(f, "#ifndef TEST_HP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_HP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct hp_oracle_vector {")
	fmt.Fprintln(f, "    int16_t x_in[2];")
	fmt.Fprintln(f, "    int32_t y_in[2];")
	fmt.Fprintln(f, "    int16_t in[40];")
	fmt.Fprintln(f, "    int16_t pre[40];")
	fmt.Fprintln(f, "    int16_t final[40];")
	fmt.Fprintln(f, "    int16_t x_out[2];")
	fmt.Fprintln(f, "    int32_t y_out[2];")
	fmt.Fprintln(f, "} hp_oracle_vector;")
	fmt.Fprintf(f, "#define HP_ORACLE_VECTOR_COUNT %d\n", len(cases))
	fmt.Fprintln(f, "static const hp_oracle_vector HP_ORACLE_VECTORS[HP_ORACLE_VECTOR_COUNT] = {")
	for _, tc := range cases {
		state := tc.state
		pre, final := state.filter(tc.in)
		fmt.Fprintln(f, "    {")
		fmt.Fprint(f, "     ")
		emitArray16(f, tc.state.x[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray32(f, tc.state.y[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, tc.in[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, pre[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, final[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray16(f, state.x[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray32(f, state.y[:])
		fmt.Fprintln(f, "\n    },")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func (s *hpState) filter(in [subframeLen]int16) (pre, final [subframeLen]int16) {
	x1 := s.x[0]
	x2 := s.x[1]
	y1 := s.y[0]
	y2 := s.y[1]
	for n := 0; n < subframeLen; n++ {
		xn := in[n]
		ff := fixed.LMult(fixed.Word16(xn), hpB0Q13)
		ff = fixed.LMac(ff, fixed.Word16(x1), hpB1Q13)
		ff = fixed.LMac(ff, fixed.Word16(x2), hpB2Q13)

		fb := fixed.LAdd(
			hpMpy32_16(y1, hpFeedbackA1Q13),
			hpMpy32_16(y2, hpFeedbackA2Q13),
		)
		acc := fixed.LShl(fixed.LAdd(ff, fb), 2)
		pre[n] = int16(fixed.Round(acc))
		final[n] = hpFinalFromAccNative(acc)

		x2 = x1
		x1 = xn
		y2 = y1
		y1 = int32(acc)
	}
	s.x[0] = x1
	s.x[1] = x2
	s.y[0] = y1
	s.y[1] = y2
	return pre, final
}

func hpFinalFromAccNative(acc fixed.Word32) int16 {
	return int16(fixed.Round(fixed.LShl(acc, 1)))
}

func hpMpy32_16(x int32, n fixed.Word16) fixed.Word32 {
	hi, lo := hpLExtract(x)
	return fixed.LMac(fixed.LMult(hi, n), fixed.Mult(lo, n), 1)
}

func hpLExtract(x int32) (hi, lo fixed.Word16) {
	hi = fixed.ExtractH(fixed.Word32(x))
	lo = fixed.ExtractL(fixed.LMsu(fixed.LShr(fixed.Word32(x), 1), hi, 16384))
	return hi, lo
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

func emitArray32(f *os.File, vals []int32) {
	fmt.Fprint(f, "{")
	for i, v := range vals {
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
}

func pulse(pos int, amp int16) [subframeLen]int16 {
	var out [subframeLen]int16
	out[pos] = amp
	return out
}

func ramp(start, step int) [subframeLen]int16 {
	var out [subframeLen]int16
	for i := range out {
		out[i] = int16(start + i*step)
	}
	return out
}

func centered(scale, offset int) [subframeLen]int16 {
	var out [subframeLen]int16
	for i := range out {
		out[i] = int16((i + offset) * scale)
	}
	return out
}

func alternating(a, b int16) [subframeLen]int16 {
	var out [subframeLen]int16
	for i := range out {
		if i%2 == 0 {
			out[i] = a
		} else {
			out[i] = b
		}
	}
	return out
}
