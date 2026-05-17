package main

import (
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"

	"github.com/hunydev/g729"
)

type fixtureFile struct {
	Source       string          `json:"source"`
	SampleRate   int             `json:"sample_rate"`
	FrameSamples int             `json:"frame_samples"`
	FrameBytes   int             `json:"frame_bytes"`
	Vectors      []fixtureVector `json:"vectors"`
}

type fixtureVector struct {
	Name   string         `json:"name"`
	Frames []fixtureFrame `json:"frames"`
}

type fixtureFrame struct {
	PCMIn       []int16 `json:"pcm_in"`
	BitsHex     string  `json:"bits_hex"`
	DecodedPCM  []int16 `json:"decoded_pcm"`
	LoopbackPCM []int16 `json:"loopback_pcm"`
}

func main() {
	outPath := flag.String("out", "testdata/oracle/basic_vectors.json", "output JSON fixture path")
	flag.Parse()

	fixture := fixtureFile{
		Source:       "/home/exedev/g729",
		SampleRate:   g729.SampleRate,
		FrameSamples: g729.FrameSamples,
		FrameBytes:   g729.FrameBytes,
	}

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
		vec, err := generateVector(spec.name, spec.fn, 3)
		if err != nil {
			fmt.Fprintf(os.Stderr, "oracle-gen: %v\n", err)
			os.Exit(1)
		}
		fixture.Vectors = append(fixture.Vectors, vec)
	}

	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "oracle-gen: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "oracle-gen: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	enc := json.NewEncoder(f)
	enc.SetIndent("", "  ")
	if err := enc.Encode(fixture); err != nil {
		fmt.Fprintf(os.Stderr, "oracle-gen: encode: %v\n", err)
		os.Exit(1)
	}
}

func generateVector(name string, signal func(frame, sample int) int16, frames int) (fixtureVector, error) {
	enc := g729.NewEncoder()
	decForDecoderExpected := g729.NewDecoder()
	decForLoopback := g729.NewDecoder()
	vec := fixtureVector{Name: name}

	for frame := 0; frame < frames; frame++ {
		pcm := make([]int16, g729.FrameSamples)
		for i := range pcm {
			pcm[i] = signal(frame, i)
		}

		bits := make([]byte, g729.FrameBytes)
		if err := enc.EncodeFrame(pcm, bits); err != nil {
			return fixtureVector{}, fmt.Errorf("%s frame %d encode: %w", name, frame, err)
		}

		decoded := make([]int16, g729.FrameSamples)
		if err := decForDecoderExpected.DecodeFrame(bits, decoded); err != nil {
			return fixtureVector{}, fmt.Errorf("%s frame %d decode expected: %w", name, frame, err)
		}

		loopback := make([]int16, g729.FrameSamples)
		if err := decForLoopback.DecodeFrame(bits, loopback); err != nil {
			return fixtureVector{}, fmt.Errorf("%s frame %d loopback: %w", name, frame, err)
		}

		vec.Frames = append(vec.Frames, fixtureFrame{
			PCMIn:       pcm,
			BitsHex:     hex.EncodeToString(bits),
			DecodedPCM:  decoded,
			LoopbackPCM: loopback,
		})
	}

	return vec, nil
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
