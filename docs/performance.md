# Performance Baseline

This page records release-candidate load and performance checks for the
clean-room C codec. Results are environment-specific and are intended as a
regression baseline, not a product guarantee.

Run:

```sh
make loadtest
```

The default load test encodes and decodes 20,000 deterministic 10 ms frames,
covering 200 seconds of synthetic 8 kHz mono audio. The benchmark prints frame
counts, wall-clock timings, realtime multiples, and checksums to guard against
dead-code elimination or accidental no-op paths.

Use a larger run for release candidate spot checks:

```sh
make loadtest BENCH_FRAMES=100000
```

## Latest Local Baseline

Captured 2026-05-18 in the exe.dev VM:

- OS: Linux 6.12.67 x86_64.
- CPU visible to VM: 2 vCPU, AMD EPYC 9554P.
- Compiler: `cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`.
- Build flags: default Makefile `-std=c99 -Wall -Wextra -Wpedantic -Werror
  -O2 -g`.

Default load test:

```text
build/tools/g729bench 20000
frames: 20000
audio_seconds: 200.00
encode_seconds: 2.437333
decode_seconds: 0.331663
encode_fps: 8205.69
decode_fps: 60302.17
encode_realtime: 82.06x
decode_realtime: 603.02x
encode_checksum: 2519a302d4c02f3e
decode_checksum: c49297ae6d0b4eec
```

Longer spot check:

```text
build/tools/g729bench 100000
frames: 100000
audio_seconds: 1000.00
encode_seconds: 12.049318
decode_seconds: 1.669550
encode_fps: 8299.22
decode_fps: 59896.38
encode_realtime: 82.99x
decode_realtime: 598.96x
encode_checksum: 3c010f791c023697
decode_checksum: c5bb456054c7e991
```
