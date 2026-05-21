# Compatibility

This document separates source compatibility, binary runtime compatibility, and
API/ABI policy. For C libraries those are related but not interchangeable.

## Summary

| Area | Policy |
| --- | --- |
| C language | C99 source. The public header requires `<stdint.h>`. |
| Public header | `include/g729.h` is the only installed API header. It is C++ compatible through `extern "C"`. |
| Build systems | POSIX-style `./configure && make` and CMake. |
| CMake minimum | CMake 3.16 or newer. |
| Libraries | Static `libg729.a`; optional shared `libg729.so` or `libg729.dylib`. |
| Runtime dependencies | The codec library depends only on the platform C runtime. It does not require libm, pthread, dlopen, sockets, or dynamic allocation. |
| Tools | `g729enc`, `g729dec`, and `g729bench` are optional user tools. The codec library can be built without them. |
| Data format | API buffers use host-endian `int16_t` PCM arrays. CLI PCM files are signed 16-bit little-endian 8 kHz mono. G.729 frames are packed 10-byte frames. |
| Threading | Use one encoder or decoder instance per stream. The same instance requires caller-side serialization. Separate instances may be used concurrently on supported compilers. |

## Compiler Baseline

The source baseline is a C99 compiler with `<stdint.h>`.

Supported compiler floor for issue triage and source releases:

| Compiler | Minimum |
| --- | --- |
| GCC | 4.8 or newer |
| Clang | 3.5 or newer |
| Apple Clang | Xcode toolchains with C99 support |
| MSVC | Static-library CMake builds are expected, but not yet release-gated |

Current CI validates newer GCC/Clang toolchains through the platform matrix, not
every historical minimum compiler. Compatibility fixes for the supported floor
are accepted when they keep the C99 source contract intact.

## libc And Binary Runtime Baseline

Source builds do not use glibc-specific APIs. Linux glibc compatibility for a
published shared object is determined by the environment used to build that
shared object.

Recommended Linux binary-release policy:

- Build generic glibc Linux binaries in a glibc 2.17 baseline environment, such
  as manylinux2014 or an equivalent CentOS 7-compatible toolchain.
- Advertise the resulting binary as `glibc >= 2.17` only after checking the
  actual symbol versions.
- Do not build generic Linux release binaries on a newer distro and assume they
  will run on older distros.

Useful checks:

```sh
ldd libg729.so
readelf --version-info libg729.so
readelf -Ws libg729.so | grep GLIBC_
```

Additional compatibility checks used for this policy:

- manylinux2014 x86_64: GCC 10.2.1, glibc 2.17, `./configure`, shared-library
  build, and ELF version-info inspection.
- Alpine 3.20 x86_64: GCC 13.2.1, musl libc, `./configure` shared-library build,
  and CMake build.

The project does not currently publish prebuilt generic Linux binaries. If that
changes, each release should state the build image, compiler, libc baseline,
architecture, and `readelf --version-info` result.

## Tested Platforms

GitHub Actions currently validate:

| Target | Coverage |
| --- | --- |
| Linux x86_64 | Native Ubuntu runner, GCC, configure/make/install, CMake, CTest |
| Linux ARM64 | Docker/QEMU Ubuntu ARM64, GCC, configure/make/install, CMake, CTest |
| Linux i386 | Docker `linux/386`, GCC, configure/make/install, CMake, CTest |
| macOS ARM64 | Native macOS runner, Apple Clang, configure/make/install, CMake, CTest |

Additional source builds have been checked locally on glibc 2.17 and musl.

Not currently release-gated:

- Windows DLL exports and import libraries.
- Big-endian targets.
- Non-GCC/Clang/MSVC compilers.
- Hard-float versus soft-float ABI variants on embedded ARM.

## Public API

Public API is limited to installed declarations in `include/g729.h`:

- Frame constants: `G729_SAMPLE_RATE`, `G729_FRAME_SAMPLES`,
  `G729_FRAME_BYTES`.
- Version constants: `G729_VERSION_MAJOR`, `G729_VERSION_MINOR`,
  `G729_VERSION_PATCH`, `G729_VERSION_STRING`.
- State types: `g729_encoder`, `g729_decoder`.
- Lifecycle functions: `g729_encoder_init`, `g729_encoder_reset`,
  `g729_decoder_init`, `g729_decoder_reset`.
- Codec functions: `g729_encode_frame`, `g729_decode_frame`.
- Utility functions: `g729_version_string`, `g729_strerror`.

Headers under `src/`, generated fixture headers, tests, oracle tools, and helper
symbols are internal implementation details. They are not API-compatible or
ABI-compatible release surfaces.

## ABI Policy

The current project version is `0.1.0`, and the shared-library `SOVERSION` is
`0`. ABI is not frozen before a future `1.0` release.

Compatibility rules for releases:

- Patch releases may fix behavior without changing public function signatures,
  public constants, or state object sizes.
- Minor releases may add new public functions or constants while preserving
  existing source compatibility.
- Any incompatible change to public function signatures, enum values, state
  object size/layout, or binary calling convention requires a major version and
  shared-library SOVERSION bump.

`g729_encoder` and `g729_decoder` are intentionally opaque-by-convention state
objects. Callers may allocate them on the stack or heap, but must not inspect or
modify their fields. Current tested ABI size is 2048 bytes. Alignment follows
the platform ABI for `uint64_t`, so consumers should use normal C allocation
rather than hard-coding an alignment. Future incompatible changes require an ABI
break.

## Thread Safety

Each encoder or decoder instance carries stream history. Do not share one
instance across threads without external locking.

Separate instances can be used concurrently on supported compilers. The internal
fixed-point overflow diagnostic is thread-local on GCC, Clang, C11-capable
compilers, and MSVC. If porting to a compiler without thread-local storage,
serialize codec calls or add an equivalent thread-local implementation.

## Release Checklist Additions

Before publishing a C library release, record:

- Version, git commit, and source archive checksum.
- Compiler name/version and C flags.
- Build system path: configure/make or CMake.
- Target OS, architecture, libc, and dynamic loader baseline.
- Static/shared build choice and shared-library SOVERSION.
- `readelf --version-info` for Linux shared objects or `otool -L` for macOS
  dylibs.
- Public header diff against the previous release.
- Whether the release includes tools, pkg-config metadata, and CMake package
  metadata.
