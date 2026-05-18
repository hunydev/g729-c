# Release Notes Draft

## Scope

- Clean-room C port of the MIT-licensed Go implementation in
  `/home/exedev/g729`.
- G.729 Annex A-compatible `G729/8000 annexb=no` speech frames.
- 10 ms frames, 80 signed 16-bit PCM samples, and 10 packed G.729 bytes.
- Public C encoder/decoder API and raw-file `g729enc` / `g729dec` tools.

## Validation

- C decoder output is sample-exact against committed Go decoder fixtures.
- C encoder output is byte-exact against committed Go `EncoderProfileCore`
  fixtures.
- C encode followed by C decode matches Go encode followed by Go decode on
  committed loopback fixtures.
- CLI encode/decode smoke tests cover file paths, stdin/stdout, and trailing
  partial-frame rejection.
- Sanitizer coverage uses AddressSanitizer and UndefinedBehaviorSanitizer when
  available.
- Load testing uses `make loadtest` / `tools/g729bench`.

## Unsupported

- Annex B SID/CNG/DTX and `annexb=yes`.
- G.729.1, G.729D, and G.729E.
- ITU certification or ITU endorsement.
- Byte-exact conformance claims against ITU reference C, bcg729, FFmpeg, or
  any other third-party G.729 implementation.

## Clean-Room Boundary

Implementation inputs are limited to the sibling clean-room Go repository,
Go-generated numeric/byte fixtures, public docs/tests from that repository,
and original C code in this repository.
