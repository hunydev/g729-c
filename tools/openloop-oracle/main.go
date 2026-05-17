package main

import (
	"errors"
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/lpc"
	"github.com/hunydev/g729/internal/lsp"
	"github.com/hunydev/g729/internal/pcm"
	"github.com/hunydev/g729/internal/pitch/openloop"
)

const frameCount = 8

type frameVector struct {
	pcm              [pcm.FrameLength]int16
	speech           [pcm.FrameLength]int16
	aHatSF1          [lpc.LPCOrder + 1]int16
	aHatSF2          [lpc.LPCOrder + 1]int16
	result           openloop.SearchResult
	residualMemAfter [10]int16
	swMemAfter       [10]int16
	oldWspeechAfter  [143]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/openloop_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "openloop-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "openloop-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	frames := generate()

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729 encoder open-loop pitch path. */")
	fmt.Fprintln(f, "#ifndef TEST_OPENLOOP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_OPENLOOP_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct openloop_oracle_range_score {")
	fmt.Fprintln(f, "    int16_t lag;")
	fmt.Fprintln(f, "    int32_t r;")
	fmt.Fprintln(f, "    int32_t e;")
	fmt.Fprintln(f, "} openloop_oracle_range_score;")
	fmt.Fprintln(f, "typedef struct openloop_oracle_frame {")
	fmt.Fprintln(f, "    int16_t pcm[80];")
	fmt.Fprintln(f, "    int16_t speech[80];")
	fmt.Fprintln(f, "    int16_t a_hat_sf1[11];")
	fmt.Fprintln(f, "    int16_t a_hat_sf2[11];")
	fmt.Fprintln(f, "    openloop_oracle_range_score range1;")
	fmt.Fprintln(f, "    openloop_oracle_range_score range2;")
	fmt.Fprintln(f, "    openloop_oracle_range_score range3;")
	fmt.Fprintln(f, "    int16_t top;")
	fmt.Fprintln(f, "    int16_t residual_mem_after[10];")
	fmt.Fprintln(f, "    int16_t sw_mem_after[10];")
	fmt.Fprintln(f, "    int16_t old_wspeech_after[143];")
	fmt.Fprintln(f, "} openloop_oracle_frame;")
	fmt.Fprintf(f, "#define OPENLOOP_ORACLE_FRAME_COUNT %d\n", len(frames))
	fmt.Fprintln(f, "static const openloop_oracle_frame OPENLOOP_ORACLE_FRAMES[OPENLOOP_ORACLE_FRAME_COUNT] = {")
	for _, fr := range frames {
		fmt.Fprintln(f, "    {")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.pcm[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.speech[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.aHatSF1[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.aHatSF2[:])
		fmt.Fprintln(f, ",")
		emitRange(f, fr.result.Range1)
		fmt.Fprintln(f, ",")
		emitRange(f, fr.result.Range2)
		fmt.Fprintln(f, ",")
		emitRange(f, fr.result.Range3)
		fmt.Fprintf(f, ",\n     %d,\n     ", fr.result.Top)
		emitArray(f, fr.residualMemAfter[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.swMemAfter[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.oldWspeechAfter[:])
		fmt.Fprintln(f, "\n    },")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func generate() []frameVector {
	var pre pcm.PreProcessor
	var analyzer lpc.Analyzer
	var oldSpeech [lpc.LPCWindowSamples]int16
	var freqPrev [4][10]int16
	var lspOld [10]int16
	var lspDec lsp.Decoder
	var residualMem [10]int16
	var swMem [10]int16
	var oldWspeech [143]int16

	lsp.InitFreqPrev(&freqPrev)
	lsp.InitLSPOld(&lspOld)

	out := make([]frameVector, 0, frameCount)
	for frame := 0; frame < frameCount; frame++ {
		var fr frameVector
		for i := range fr.pcm {
			fr.pcm[i] = speechLike(frame, i)
		}

		var processed [pcm.FrameLength]int16
		pre.Process(fr.pcm[:], processed[:])
		copy(oldSpeech[0:160], oldSpeech[80:240])
		copy(oldSpeech[160:240], processed[:])
		copy(fr.speech[:], oldSpeech[120:200])

		var a [lpc.LPCOrder + 1]int16
		if err := analyzer.Analyze(&oldSpeech, &a); err != nil {
			panic(err)
		}
		var lspQ15 [10]int16
		if err := lsp.LPToLSP(&a, &lspQ15); err != nil {
			if !errors.Is(err, lsp.ErrLPCNonStable) {
				panic(err)
			}
			lspQ15 = lspOld
		} else {
			lspOld = lspQ15
		}
		var lsfQ13 [10]int16
		lsp.LSPToLSF(&lspQ15, &lsfQ13)
		indices := lsp.Quantize(&lsfQ13, &freqPrev)
		fr.aHatSF1, fr.aHatSF2 = lspDec.Decode(indices)

		s := (*[pcm.FrameLength]int16)(oldSpeech[120:200])
		fr.result = openloop.StepSplitSearch(&fr.aHatSF1, &fr.aHatSF2, s, &residualMem, &swMem, &oldWspeech)
		fr.residualMemAfter = residualMem
		fr.swMemAfter = swMem
		fr.oldWspeechAfter = oldWspeech
		out = append(out, fr)
	}
	return out
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

func emitRange(f *os.File, r openloop.RangeScore) {
	fmt.Fprintf(f, "     {%d, %d, %d}", r.Lag, r.R, r.E)
}

func speechLike(frame, sample int) int16 {
	x := float64(frame*pcm.FrameLength + sample)
	env := 0.55 + 0.45*math.Sin(2*math.Pi*x/160)
	v := env * (2100*math.Sin(2*math.Pi*x/53) + 900*math.Sin(2*math.Pi*x/17))
	return int16(math.Round(v))
}
