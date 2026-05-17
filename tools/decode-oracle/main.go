package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729"
)

const frameCount = 3

type vector struct {
	name   string
	frames []frame
}

type frame struct {
	bits [g729.FrameBytes]byte
	pcm  [g729.FrameSamples]int16
}

func main() {
	outPath := flag.String("out", "tests/fixtures/decode_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "decode-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "decode-oracle: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	var vectors []vector
	for _, spec := range []struct {
		name string
		fn   func(frame, sample int) int16
	}{
		{"silence", silence},
		{"impulse", impulse},
		{"low_amplitude", lowAmplitude},
		{"speech_like", speechLike},
		{"near_clipping", nearClipping},
		{"pseudo_random", pseudoRandom},
	} {
		vec, err := generateVector(spec.name, spec.fn)
		if err != nil {
			fmt.Fprintf(os.Stderr, "decode-oracle: %v\n", err)
			os.Exit(1)
		}
		vectors = append(vectors, vec)
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729 Decoder.DecodeFrame. */")
	fmt.Fprintln(f, "#ifndef TEST_DECODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_DECODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct decode_oracle_frame {")
	fmt.Fprintln(f, "    uint8_t bits[10];")
	fmt.Fprintln(f, "    int16_t pcm[80];")
	fmt.Fprintln(f, "} decode_oracle_frame;")
	fmt.Fprintln(f, "typedef struct decode_oracle_vector {")
	fmt.Fprintln(f, "    const char *name;")
	fmt.Fprintln(f, "    decode_oracle_frame frames[3];")
	fmt.Fprintln(f, "} decode_oracle_vector;")
	fmt.Fprintf(f, "#define DECODE_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "#define DECODE_ORACLE_FRAMES_PER_VECTOR 3")
	fmt.Fprintln(f, "static const decode_oracle_vector DECODE_ORACLE_VECTORS[DECODE_ORACLE_VECTOR_COUNT] = {")
	for _, vec := range vectors {
		fmt.Fprintf(f, "    {\"%s\", {\n", vec.name)
		for _, fr := range vec.frames {
			fmt.Fprint(f, "        {")
			emitArray8(f, fr.bits[:])
			fmt.Fprint(f, ", ")
			emitArray16(f, fr.pcm[:])
			fmt.Fprintln(f, "},")
		}
		fmt.Fprintln(f, "    }},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func generateVector(name string, signal func(frame, sample int) int16) (vector, error) {
	enc := g729.NewEncoder()
	dec := g729.NewDecoder()
	vec := vector{name: name}
	for frameNo := 0; frameNo < frameCount; frameNo++ {
		pcmIn := make([]int16, g729.FrameSamples)
		for i := range pcmIn {
			pcmIn[i] = signal(frameNo, i)
		}
		bits := make([]byte, g729.FrameBytes)
		if err := enc.EncodeFrame(pcmIn, bits); err != nil {
			return vector{}, fmt.Errorf("%s frame %d encode: %w", name, frameNo, err)
		}
		pcmOut := make([]int16, g729.FrameSamples)
		if err := dec.DecodeFrame(bits, pcmOut); err != nil {
			return vector{}, fmt.Errorf("%s frame %d decode: %w", name, frameNo, err)
		}
		var fr frame
		copy(fr.bits[:], bits)
		copy(fr.pcm[:], pcmOut)
		vec.frames = append(vec.frames, fr)
	}
	return vec, nil
}

func emitArray8(f *os.File, vals []byte) {
	fmt.Fprint(f, "{")
	for i, v := range vals {
		fmt.Fprintf(f, "0x%02x", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ", ")
		}
	}
	fmt.Fprint(f, "}")
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

func silence(frame, sample int) int16 {
	_, _ = frame, sample
	return 0
}

func impulse(frame, sample int) int16 {
	if frame == 0 && sample == 0 {
		return 12000
	}
	return 0
}

func lowAmplitude(frame, sample int) int16 {
	x := float64(frame*g729.FrameSamples + sample)
	return int16(math.Round(700*math.Sin(2*math.Pi*x/37) + 230*math.Sin(2*math.Pi*x/19)))
}

func speechLike(frame, sample int) int16 {
	x := float64(frame*g729.FrameSamples + sample)
	env := 0.55 + 0.45*math.Sin(2*math.Pi*x/160)
	v := env * (2100*math.Sin(2*math.Pi*x/53) + 900*math.Sin(2*math.Pi*x/17))
	return int16(math.Round(v))
}

func nearClipping(frame, sample int) int16 {
	x := float64(frame*g729.FrameSamples + sample)
	v := 29200*math.Sin(2*math.Pi*x/29) + 2300*math.Sin(2*math.Pi*x/7)
	if v > 32000 {
		v = 32000
	}
	if v < -32000 {
		v = -32000
	}
	return int16(math.Round(v))
}

func pseudoRandom(frame, sample int) int16 {
	x := uint32(frame*g729.FrameSamples + sample + 1)
	x ^= x << 13
	x ^= x >> 17
	x ^= x << 5
	return int16(int32(x%20001) - 10000)
}
