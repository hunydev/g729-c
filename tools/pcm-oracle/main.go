package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/pcm"
)

const frameCount = 4

type vector struct {
	name   string
	frames [frameCount][pcm.FrameLength]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/pcm_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "pcm-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "pcm-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	vectors := []vector{
		{name: "silence", frames: sequence(silence)},
		{name: "impulse_then_zero", frames: sequence(impulseThenZero)},
		{name: "dc_step", frames: sequence(dcStep)},
		{name: "speech_like", frames: sequence(speechLike)},
		{name: "alternating_clip", frames: sequence(alternatingClip)},
		{name: "pseudo_random", frames: sequence(pseudoRandom)},
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/pcm.PreProcessor. */")
	fmt.Fprintln(f, "#ifndef TEST_PCM_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_PCM_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct pcm_oracle_vector {")
	fmt.Fprintln(f, "    const char *name;")
	fmt.Fprintln(f, "    int16_t in[4][80];")
	fmt.Fprintln(f, "    int16_t out[4][80];")
	fmt.Fprintln(f, "} pcm_oracle_vector;")
	fmt.Fprintf(f, "#define PCM_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "#define PCM_ORACLE_FRAME_COUNT 4")
	fmt.Fprintln(f, "static const pcm_oracle_vector PCM_ORACLE_VECTORS[PCM_ORACLE_VECTOR_COUNT] = {")
	for _, vec := range vectors {
		var pre pcm.PreProcessor
		var out [frameCount][pcm.FrameLength]int16
		for frame := 0; frame < frameCount; frame++ {
			pre.Process(vec.frames[frame][:], out[frame][:])
		}
		fmt.Fprintf(f, "    {\"%s\",\n", vec.name)
		fmt.Fprint(f, "     ")
		emitFrames(f, vec.frames)
		fmt.Fprintln(f, ",")
		fmt.Fprint(f, "     ")
		emitFrames(f, out)
		fmt.Fprintln(f, "},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func emitFrames(f *os.File, frames [frameCount][pcm.FrameLength]int16) {
	fmt.Fprint(f, "{")
	for i, frame := range frames {
		emitArray(f, frame[:])
		if i != len(frames)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
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

func sequence(fn func(frame, sample int) int16) [frameCount][pcm.FrameLength]int16 {
	var out [frameCount][pcm.FrameLength]int16
	for frame := range out {
		for sample := range out[frame] {
			out[frame][sample] = fn(frame, sample)
		}
	}
	return out
}

func silence(frame, sample int) int16 {
	_, _ = frame, sample
	return 0
}

func impulseThenZero(frame, sample int) int16 {
	if frame == 0 && sample == 0 {
		return 32000
	}
	return 0
}

func dcStep(frame, sample int) int16 {
	_, _ = frame, sample
	return 2000
}

func speechLike(frame, sample int) int16 {
	x := float64(frame*pcm.FrameLength + sample)
	env := 0.55 + 0.45*math.Sin(2*math.Pi*x/160)
	v := env * (2100*math.Sin(2*math.Pi*x/53) + 900*math.Sin(2*math.Pi*x/17))
	return int16(math.Round(v))
}

func alternatingClip(frame, sample int) int16 {
	if (frame*pcm.FrameLength+sample)%2 == 0 {
		return 32767
	}
	return -32768
}

func pseudoRandom(frame, sample int) int16 {
	x := uint32(frame*pcm.FrameLength + sample + 1)
	x ^= x << 13
	x ^= x >> 17
	x ^= x << 5
	return int16(int32(x%20001) - 10000)
}
