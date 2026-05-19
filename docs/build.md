# Build And Install

This project supports two user-facing build paths:

- `./configure && make && make install` for a small POSIX-style build.
- CMake for IDEs, package managers, shared library builds, and installed
  package discovery.

The library is plain C99 and is tested on Linux x86_64, Linux ARM64, Linux
i386, and macOS ARM64 in GitHub Actions.

## Configure And Make

```sh
./configure --prefix=/usr/local
make
make test
make install
```

`./configure` writes `config.mk`, which is intentionally ignored by git.
Without `./configure`, the developer defaults remain active and `make` builds
the library, tools, tests, and examples.

Return to an unconfigured tree:

```sh
make distclean
```

Common options:

```sh
./configure --prefix=/usr
./configure --enable-shared
./configure --disable-tools
./configure --enable-tests --enable-examples
./configure --enable-werror
```

Install into a staging root:

```sh
./configure --prefix=/usr
make
make install DESTDIR="$PWD/pkgroot"
```

Installed files:

- `include/g729.h`
- `lib/libg729.a`
- `lib/libg729.so` or `lib/libg729.dylib` when `--enable-shared` is used
- `lib/pkgconfig/g729.pc`
- `bin/g729enc`, `bin/g729dec`, and `bin/g729bench` unless tools are disabled

## CMake

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
cmake --install build-cmake --prefix /usr/local
```

Build a shared library:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build-cmake
```

Useful options:

```sh
-DG729_BUILD_TOOLS=ON
-DG729_BUILD_TESTS=ON
-DG729_BUILD_EXAMPLES=ON
-DG729_ENABLE_WARNINGS=ON
-DG729_ENABLE_WERROR=OFF
```

Installed CMake package usage:

```cmake
find_package(g729 CONFIG REQUIRED)
add_executable(app app.c)
target_link_libraries(app PRIVATE g729::g729)
```

Installed pkg-config usage:

```sh
cc app.c $(pkg-config --cflags --libs g729) -o app
```

## i386 / 32-bit x86

The CI matrix verifies i386 through Docker with `linux/386`. For local native
32-bit builds on Linux, install the distro's multilib packages and use:

```sh
./configure --arch=i386
make
make test
```

The `--arch=i386` option adds `-m32` to C, C++, and linker flags on non-Darwin
systems. On macOS it uses `-arch i386`, although modern macOS toolchains no
longer ship 32-bit runtime support.

Docker i386 smoke test:

```sh
docker run --rm --platform linux/386 \
  -v "$PWD:/workspace" -w /workspace \
  i386/ubuntu:24.04 \
  bash -lc 'apt-get update && apt-get install -y build-essential cmake && ./configure && make test'
```

## Cross Toolchains

For Make builds, pass a host triple when the cross tools are in `PATH`:

```sh
./configure --host=aarch64-linux-gnu --disable-tools
make
```

For CMake, prefer a normal CMake toolchain file:

```sh
cmake -S . -B build-aarch64 -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain.cmake
cmake --build build-aarch64
```

When cross-compiling, tests usually require an emulator or target device, so
build-only validation may be the correct local check.
