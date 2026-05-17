package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/tables"
)

func main() {
	outPath := flag.String("out", "src/g729_lsp_tables.c", "output C source path")
	flag.Parse()

	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "port-lsp-tables: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "port-lsp-tables: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/tables LSP tables. */")
	fmt.Fprintln(f, "#include \"g729_lsp_tables.h\"")
	fmt.Fprintln(f)
	emit2D(f, "g729_lsp_codebook_l1", tables.LSPCodebookL1[:])
	fmt.Fprintln(f)
	emit2D(f, "g729_lsp_codebook_l2", tables.LSPCodebookL2[:])
	fmt.Fprintln(f)
	emit2D(f, "g729_lsp_codebook_l3", tables.LSPCodebookL3[:])
	fmt.Fprintln(f)
	emit3D(f, "g729_lsp_ma_predictors", tables.MAPredictorsLSP[:])
	fmt.Fprintln(f)
	emit2D(f, "g729_lsp_ma_predictor_inv_sum", tables.MAPredictorInvSumLSP[:])
	fmt.Fprintln(f)
	emit1D(f, "g729_lsp_cos", tables.CosLSP[:])
	fmt.Fprintln(f)
	emit1D(f, "g729_lsp_cos_slope", tables.CosLSPSlope[:])
}

func emit1D(f *os.File, name string, rows []int16) {
	fmt.Fprintf(f, "const int16_t %s[%d] = {\n", name, len(rows))
	for i, v := range rows {
		if i%8 == 0 {
			fmt.Fprint(f, "    ")
		}
		fmt.Fprintf(f, "%d", v)
		if i != len(rows)-1 {
			if i%8 == 7 {
				fmt.Fprint(f, ",")
			} else {
				fmt.Fprint(f, ", ")
			}
		}
		if i%8 == 7 || i == len(rows)-1 {
			fmt.Fprintln(f)
		}
	}
	fmt.Fprintln(f, "};")
}

type row2 interface {
	~[10]int16 | ~[5]int16
}

func emit2D[T row2](f *os.File, name string, rows []T) {
	width := len(any(rows[0]).(T))
	fmt.Fprintf(f, "const int16_t %s[%d][%d] = {\n", name, len(rows), width)
	for _, row := range rows {
		fmt.Fprint(f, "    {")
		for i := 0; i < width; i++ {
			fmt.Fprintf(f, "%d", any(row).(T)[i])
			if i != width-1 {
				fmt.Fprint(f, ", ")
			}
		}
		fmt.Fprintln(f, "},")
	}
	fmt.Fprintln(f, "};")
}

func emit3D(f *os.File, name string, rows [][4][10]int16) {
	fmt.Fprintf(f, "const int16_t %s[%d][4][10] = {\n", name, len(rows))
	for _, group := range rows {
		fmt.Fprintln(f, "    {")
		for _, row := range group {
			fmt.Fprint(f, "        {")
			for i, v := range row {
				fmt.Fprintf(f, "%d", v)
				if i != len(row)-1 {
					fmt.Fprint(f, ", ")
				}
			}
			fmt.Fprintln(f, "},")
		}
		fmt.Fprintln(f, "    },")
	}
	fmt.Fprintln(f, "};")
}
