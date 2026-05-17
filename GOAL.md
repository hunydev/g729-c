# g729-c Long-Running Goal

This document is the long-form goal for building `github.com/hunydev/g729-c`
from the clean-room Go implementation in `/home/exedev/g729`.

Use this file as the detailed instruction source for a long-running Codex
`/goal` session. The short `/goal` prompt should refer back to this file.

## Final Objective

Implement a clean-room, MIT-licensed C version of the G.729A-compatible codec
using `/home/exedev/g729` as the source-of-truth implementation.

The final `g729-c` repository should contain:

- A C codec library.
- A public C API.
- Encoder and decoder state management.
- Frame encode/decode functions.
- CLI tools for raw file conversion.
- Tests and generated fixtures.
- Build documentation.
- Clean-room/IP provenance documentation.
- Release-readiness documentation.

Correctness is the first priority. Performance optimization comes only after
the C implementation matches the Go implementation on the relevant fixtures.

## Source-of-Truth

Allowed implementation source:

- `/home/exedev/g729` Go source.
- `/home/exedev/g729` public README/docs/tests.
- Numeric or byte fixtures generated directly from the Go implementation.
- C code written in `/home/exedev/g729-c`.

Forbidden implementation sources:

- ITU reference C.
- `bcg729`.
- FFmpeg G.729 encoder/decoder source.
- Sipro implementations.
- Asterisk/FreeSWITCH G.729 codec modules.
- Sangoma or other commercial G.729 codec modules.
- Any other existing G.729 implementation source.

Do not read, copy, translate, summarize, or derive implementation logic from
forbidden sources. Do not commit third-party G.729 source or binaries.

`/home/exedev/g729` is read-only for this effort. Modify only
`/home/exedev/g729-c`.

## Supported Scope

Implement:

- G.729 Annex A compatible codec behavior.
- RTP `G729/8000 annexb=no` frame shape.
- 10 ms frames.
- 80 signed 16-bit PCM samples per frame.
- 10 packed G.729 bytes per frame.
- Encoder and decoder.
- Resettable per-stream state.
- Heap-free steady-state encode/decode if practical.

Do not implement or claim support for:

- Annex B SID/CNG/DTX.
- `annexb=yes`.
- G.729.1.
- G.729D.
- G.729E.
- ITU certification.
- ITU endorsement.
- Byte-exact conformance to ITU reference encoder, `bcg729`, FFmpeg, or any
  third-party implementation.

The C implementation may claim Go-port equivalence only when tests prove it.

## Public API Target

Design a small C API around one encoder or decoder instance per stream:

- `g729_encoder` state type.
- `g729_decoder` state type.
- `g729_encoder_init`.
- `g729_encoder_reset`.
- `g729_decoder_init`.
- `g729_decoder_reset`.
- `g729_encode_frame`.
- `g729_decode_frame`.
- Frame constants:
  - `G729_SAMPLE_RATE = 8000`
  - `G729_FRAME_SAMPLES = 80`
  - `G729_FRAME_BYTES = 10`

Preferred frame function shape:

```c
int g729_encode_frame(g729_encoder *enc,
                      const int16_t pcm[G729_FRAME_SAMPLES],
                      uint8_t bits[G729_FRAME_BYTES]);

int g729_decode_frame(g729_decoder *dec,
                      const uint8_t bits[G729_FRAME_BYTES],
                      int16_t pcm[G729_FRAME_SAMPLES]);
```

Document error codes and thread-safety:

- One encoder/decoder instance per stream.
- Concurrent calls on the same instance are not safe unless explicitly guarded
  by the caller.

## CLI Target

Add simple raw file tools:

- `g729enc`: raw signed 16-bit little-endian 8 kHz mono PCM to raw G.729 frames.
- `g729dec`: raw G.729 frames to raw signed 16-bit little-endian 8 kHz mono PCM.

The tools should support file paths and stdin/stdout where practical.

## Long-Running Plan

### Phase 0: Repository Bootstrap

1. Inspect the current `/home/exedev/g729-c` repository.
2. Choose a build system appropriate for the repository, such as CMake or a
   simple Makefile.
3. Establish a basic layout:
   - `include/`
   - `src/`
   - `tests/`
   - `tools/`
   - `docs/`
   - `testdata/`
4. Add a minimal README if needed.
5. Add initial build and smoke test commands.

### Phase 1: Go Oracle Bridge

Create a fixture generation path that uses the Go implementation as the oracle.
Do not modify `/home/exedev/g729` unless explicitly requested.

Generate small committed fixtures for:

- Silence.
- Impulse.
- Low-amplitude deterministic signal.
- Speech-like deterministic synthetic signal.
- Near-clipping deterministic signal.
- Pseudo-random deterministic signal.

Useful fixture categories:

- Decoder input G.729 bytes and expected PCM.
- Encoder input PCM and expected 10-byte output.
- Loopback expected PCM from Go encode followed by Go decode.
- Boundary inputs for short frames and 2-byte Annex B SID-like payloads.

Large generated fixtures may remain local/private if they are too large for
normal source distribution.

### Phase 2: Fixed-Point Foundation

Port the Go fixed-point behavior first.

Implement and test:

- Saturating add/subtract.
- Fixed-point multiply.
- Multiply-accumulate.
- Rounding.
- Q-format shifts.
- Explicit overflow handling.
- Safe signed integer behavior in C.

Avoid C undefined behavior. Signed overflow and implementation-dependent shift
behavior must be handled explicitly.

### Phase 3: Bitstream Pack/Unpack

Implement:

- 10-byte frame unpacking.
- 10-byte frame packing.
- Field round-trip tests.
- Short input handling.
- Short output handling if applicable.
- 2-byte Annex B SID-like rejection.

### Phase 4: Decoder Port

Port the decoder first because fixed input bitstreams make validation direct.

Recommended order:

1. LSP/LSF decode.
2. LSP to LP coefficient pipeline.
3. Pitch/adaptive codebook.
4. Fixed codebook.
5. Gain decode.
6. Excitation update.
7. Synthesis.
8. Postfilter.
9. Output high-pass filter.

For every mismatch, compare against Go-generated numeric fixtures. Do not look
at external G.729 implementations.

Decoder acceptance:

- C decoder output matches Go decoder output sample-for-sample on committed
  fixtures.
- Decoder reset is deterministic.
- Multiple decoder instances maintain independent state.

### Phase 5: Encoder Port

Port the Go product-default encoder path. The target is Go `EncoderProfileCore`.

Recommended order:

1. Preprocessing.
2. LPC analysis.
3. LSP quantization.
4. Open-loop pitch.
5. Closed-loop pitch.
6. Adaptive contribution.
7. Fixed codebook search.
8. Gain quantization.
9. Frame packing.
10. Encoder state update.

Encoder acceptance:

- C encoder output matches Go `EncoderProfileCore` output byte-for-byte on
  committed fixtures.
- C encode followed by C decode matches Go encode followed by Go decode on
  committed fixtures.
- Encoder reset is deterministic.
- Multiple encoder instances maintain independent state.

### Phase 6: API Hardening

Review and harden:

- Public header.
- State struct visibility policy.
- Error codes.
- Buffer preconditions.
- Thread-safety documentation.
- No global mutable codec state unless unavoidable and documented.
- C and C++ consumer compatibility if practical.

### Phase 7: CLI Tools

Implement and test:

- `g729enc`.
- `g729dec`.
- stdin/stdout mode if practical.
- raw PCM frame alignment checks.
- raw G.729 frame length checks.
- CLI roundtrip smoke tests.

### Phase 8: Test Matrix

Add tests for:

- Fixed-point primitives.
- Bitstream pack/unpack.
- LSP/LP.
- Pitch.
- Fixed codebook.
- Gain.
- Decoder pipeline.
- Encoder pipeline.
- Public API.
- CLI tools.
- Boundary behavior.
- Reset determinism.
- Multiple independent streams.

If available, run sanitizers:

- AddressSanitizer.
- UndefinedBehaviorSanitizer.

Sanitizers must not be used as a substitute for oracle equality tests.

### Phase 9: Performance Baseline

After correctness gates pass, add a simple benchmark:

- encode frame time;
- decode frame time;
- real-time factor;
- streams/core estimate;
- heap allocation check if applicable.

Do not optimize in ways that change Go-equivalent output.

### Phase 10: Documentation

Add or update:

- README.
- Build instructions.
- API usage.
- CLI usage.
- Supported scope.
- Unsupported scope.
- Validation summary.
- Clean-room provenance.
- Third-party notices if any non-runtime tools are mentioned.
- Release checklist.

Keep wording conservative:

- This is a C port of the clean-room Go implementation.
- It is not ITU certified.
- It is not ITU endorsed.
- It does not support Annex B, G.729.1, G.729D, or G.729E.

### Phase 11: Release Candidate

Before a release candidate:

1. Fresh clone build passes.
2. Full test suite passes.
3. CLI smoke tests pass.
4. Sanitizers pass if available.
5. Decoder C vs Go fixtures are sample-exact.
6. Encoder C vs Go fixtures are byte-exact.
7. No forbidden source or binary is tracked.
8. Documentation claims are reviewed.
9. Release note is prepared.

## Acceptance Criteria

The goal is complete when:

- `g729-c` builds with documented commands on Linux.
- Public C API is usable by a small example program.
- Decoder matches Go decoder output sample-for-sample on committed fixtures.
- Encoder matches Go `EncoderProfileCore` output byte-for-byte on committed
  fixtures.
- CLI encode/decode tools work on raw files.
- Tests pass without external G.729 implementations.
- No forbidden G.729 implementation source or binary is committed.
- Documentation clearly describes the clean-room Go-to-C port boundary.

## Mismatch Debugging Rule

If C output differs from Go output:

1. Do not consult external G.729 implementations.
2. Generate a smaller Go oracle fixture.
3. Compare stage outputs.
4. Inspect only Go source, generated numeric fixtures, and the C port.
5. Fix C arithmetic, rounding, state update, packing, or memory behavior.
6. Add a regression test before moving on.

## Working Style

- Prefer small, reviewable milestones.
- Run tests after every meaningful step.
- Keep `/home/exedev/g729` read-only.
- Modify only `/home/exedev/g729-c`.
- Commit logical milestones when the worktree is ready.
- Favor correctness and auditability over speed.
