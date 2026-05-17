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
)

const frameCount = 6

type frameVector struct {
	pcm       [pcm.FrameLength]int16
	a         [lpc.LPCOrder + 1]int16
	lspQ15    [10]int16
	lsfQ13    [10]int16
	indices   lsp.Indices
	freqAfter [4][10]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/lsp_encode_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "lsp-enc-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "lsp-enc-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	frames := generate()

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729 encoder LSP front-end. */")
	fmt.Fprintln(f, "#ifndef TEST_LSP_ENCODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_LSP_ENCODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct lsp_encode_oracle_frame {")
	fmt.Fprintln(f, "    int16_t pcm[80];")
	fmt.Fprintln(f, "    int16_t a[11];")
	fmt.Fprintln(f, "    int16_t lsp_q15[10];")
	fmt.Fprintln(f, "    int16_t lsf_q13[10];")
	fmt.Fprintln(f, "    uint8_t l0, l1, l2, l3;")
	fmt.Fprintln(f, "    int16_t freq_prev_after[4][10];")
	fmt.Fprintln(f, "} lsp_encode_oracle_frame;")
	fmt.Fprintf(f, "#define LSP_ENCODE_ORACLE_FRAME_COUNT %d\n", len(frames))
	fmt.Fprintln(f, "static const lsp_encode_oracle_frame LSP_ENCODE_ORACLE_FRAMES[LSP_ENCODE_ORACLE_FRAME_COUNT] = {")
	for _, fr := range frames {
		fmt.Fprintln(f, "    {")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.pcm[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.a[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.lspQ15[:])
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitArray(f, fr.lsfQ13[:])
		fmt.Fprintf(f, ",\n     %d, %d, %d, %d,\n     ",
			fr.indices.L0, fr.indices.L1, fr.indices.L2, fr.indices.L3)
		emitMatrix(f, fr.freqAfter)
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

		if err := analyzer.Analyze(&oldSpeech, &fr.a); err != nil {
			panic(err)
		}
		if err := lsp.LPToLSP(&fr.a, &fr.lspQ15); err != nil {
			if !errors.Is(err, lsp.ErrLPCNonStable) {
				panic(err)
			}
			fr.lspQ15 = lspOld
		} else {
			lspOld = fr.lspQ15
		}
		lsp.LSPToLSF(&fr.lspQ15, &fr.lsfQ13)
		fr.indices = lsp.Quantize(&fr.lsfQ13, &freqPrev)
		fr.freqAfter = freqPrev
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

func emitMatrix(f *os.File, vals [4][10]int16) {
	fmt.Fprint(f, "{")
	for i := range vals {
		emitArray(f, vals[i][:])
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
}

func speechLike(frame, sample int) int16 {
	x := float64(frame*pcm.FrameLength + sample)
	env := 0.55 + 0.45*math.Sin(2*math.Pi*x/160)
	v := env * (2100*math.Sin(2*math.Pi*x/53) + 900*math.Sin(2*math.Pi*x/17))
	return int16(math.Round(v))
}
