# Release Checklist

- Fresh clone build passes on Linux.
- Full C and C++ smoke test suite passes.
- CLI smoke tests pass for `g729enc` and `g729dec`, including stdin/stdout and
  trailing partial-frame rejection.
- AddressSanitizer and UndefinedBehaviorSanitizer pass when available.
- Static analysis passes with clang, clang static analyzer, and cppcheck.
- Platform tests pass on Ubuntu x64, macOS ARM64, Linux ARM64 Docker/QEMU, and
  Linux i386 Docker.
- `./configure`, `make`, `make install`, CMake, and CTest pass in the platform
  test matrix.
- Deterministic stress/load checks pass with `make loadtest`.
- Decoder output matches Go decoder output sample-for-sample on committed
  fixtures.
- Encoder output matches Go `EncoderProfileCore` output byte-for-byte on
  committed fixtures.
- C encode followed by C decode matches Go encode followed by Go decode on
  committed fixtures.
- No forbidden G.729 implementation source or binary is tracked.
- `LICENSE` is present and matches the intended MIT release terms.
- GitHub Actions CI runs the release gate, static-analysis gate, and platform
  test gate on pushes and pull requests.
- GitHub Actions platform runs publish Markdown job summaries and the combined
  `platform-test-report` artifact.
- Compatibility notes list the compiler, libc, platform, API, ABI, and binary
  release baseline for the release.
- Linux shared objects have their libc symbol versions checked with
  `readelf --version-info`; macOS dylibs have their dependencies checked with
  `otool -L`.
- Public header changes are reviewed against the API/ABI policy before release.
- README and clean-room documentation make only conservative claims.
- Release notes state unsupported Annex B, G.729.1, G.729D, G.729E, ITU
  certification, and ITU endorsement.
- `make release-check` passes before tagging or publishing a release.
- `make platform-check` passes before tagging or publishing a release.
- `make static-analysis` passes before tagging or publishing a release.
