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
	pcm  [g729.FrameSamples]int16
	bits [g729.FrameBytes]byte
}

func main() {
	outPath := flag.String("out", "tests/fixtures/encode_oracle_vectors.h", "output C header path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "encode-oracle: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "encode-oracle: create: %v\n", err)
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
			fmt.Fprintf(os.Stderr, "encode-oracle: %v\n", err)
			os.Exit(1)
		}
		vectors = append(vectors, vec)
	}

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729 EncoderProfileCore EncodeFrame. */")
	fmt.Fprintln(f, "#ifndef TEST_ENCODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#define TEST_ENCODE_ORACLE_VECTORS_H")
	fmt.Fprintln(f, "#include <stdint.h>")
	fmt.Fprintln(f, "typedef struct encode_oracle_frame {")
	fmt.Fprintln(f, "    int16_t pcm[80];")
	fmt.Fprintln(f, "    uint8_t bits[10];")
	fmt.Fprintln(f, "} encode_oracle_frame;")
	fmt.Fprintln(f, "typedef struct encode_oracle_vector {")
	fmt.Fprintln(f, "    const char *name;")
	fmt.Fprintln(f, "    encode_oracle_frame frames[3];")
	fmt.Fprintln(f, "} encode_oracle_vector;")
	fmt.Fprintf(f, "#define ENCODE_ORACLE_VECTOR_COUNT %d\n", len(vectors))
	fmt.Fprintln(f, "#define ENCODE_ORACLE_FRAMES_PER_VECTOR 3")
	fmt.Fprintln(f, "static const encode_oracle_vector ENCODE_ORACLE_VECTORS[ENCODE_ORACLE_VECTOR_COUNT] = {")
	for _, vec := range vectors {
		fmt.Fprintf(f, "    {\"%s\", {\n", vec.name)
		for _, fr := range vec.frames {
			fmt.Fprint(f, "        {")
			emitArray16(f, fr.pcm[:])
			fmt.Fprint(f, ", ")
			emitArray8(f, fr.bits[:])
			fmt.Fprintln(f, "},")
		}
		fmt.Fprintln(f, "    }},")
	}
	fmt.Fprintln(f, "};")
	fmt.Fprintln(f, "#endif")
}

func generateVector(name string, signal func(frame, sample int) int16) (vector, error) {
	enc := g729.NewEncoder()
	vec := vector{name: name}
	for frameNo := 0; frameNo < frameCount; frameNo++ {
		pcm := make([]int16, g729.FrameSamples)
		for i := range pcm {
			pcm[i] = signal(frameNo, i)
		}
		bits := make([]byte, g729.FrameBytes)
		if err := enc.EncodeFrame(pcm, bits); err != nil {
			return vector{}, fmt.Errorf("%s frame %d encode: %w", name, frameNo, err)
		}
		var fr frame
		copy(fr.pcm[:], pcm)
		copy(fr.bits[:], bits)
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
