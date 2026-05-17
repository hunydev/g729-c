package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/hunydev/g729/internal/tables"
)

func main() {
	outPath := flag.String("out", "src/g729_gain_tables.c", "output C source path")
	flag.Parse()
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		fmt.Fprintf(os.Stderr, "port-gain-tables: mkdir: %v\n", err)
		os.Exit(1)
	}
	f, err := os.Create(*outPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "port-gain-tables: create: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()

	fmt.Fprintln(f, "/* Generated from /home/exedev/g729/internal/tables gain tables. */")
	fmt.Fprintln(f, "#include \"g729_gain_tables.h\"")
	fmt.Fprintln(f)
	emit2D(f, "g729_gain_gbk1", tables.GainGBK1[:])
	fmt.Fprintln(f)
	emit2D(f, "g729_gain_gbk2", tables.GainGBK2[:])
	fmt.Fprintln(f)
	emitU8(f, "g729_gain_map1", tables.GainMap1[:])
	fmt.Fprintln(f)
	emitU8(f, "g729_gain_map2", tables.GainMap2[:])
	fmt.Fprintln(f)
	emitU8(f, "g729_gain_imap1", tables.GainImap1[:])
	fmt.Fprintln(f)
	emitU8(f, "g729_gain_imap2", tables.GainImap2[:])
	fmt.Fprintln(f)
	emitI16(f, "g729_gain_ma_predictor", tables.GainMAPredictor[:])
	fmt.Fprintln(f)
	emitI16(f, "g729_gain_pow2_table", tables.Pow2Table[:])
	fmt.Fprintln(f)
	emitI16(f, "g729_gain_log2_table", tables.Log2Table[:])
}

func emitI16(f *os.File, name string, vals []int16) {
	fmt.Fprintf(f, "const int16_t %s[%d] = {", name, len(vals))
	for i, v := range vals {
		if i%8 == 0 {
			fmt.Fprint(f, "\n    ")
		}
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ",")
			if (i+1)%8 != 0 {
				fmt.Fprint(f, " ")
			}
		}
	}
	fmt.Fprintln(f, "\n};")
}

func emitU8(f *os.File, name string, vals []uint8) {
	fmt.Fprintf(f, "const uint8_t %s[%d] = {", name, len(vals))
	for i, v := range vals {
		if i%8 == 0 {
			fmt.Fprint(f, "\n    ")
		}
		fmt.Fprintf(f, "%d", v)
		if i != len(vals)-1 {
			fmt.Fprint(f, ",")
			if (i+1)%8 != 0 {
				fmt.Fprint(f, " ")
			}
		}
	}
	fmt.Fprintln(f, "\n};")
}

type row2 interface {
	~[2]int16
}

func emit2D[T row2](f *os.File, name string, rows []T) {
	fmt.Fprintf(f, "const int16_t %s[%d][2] = {\n", name, len(rows))
	for _, row := range rows {
		fmt.Fprintf(f, "    {%d, %d},\n", row[0], row[1])
	}
	fmt.Fprintln(f, "};")
}
