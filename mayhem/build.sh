#!/usr/bin/env bash
#
# mayhem/build.sh — build the PCSX2 fuzz harnesses (and the test oracle) inside the commit image.
#
# The upstream CMake build needs Qt, SDL, FFmpeg, ... so this builds only what the harnesses exercise
# (vendored libraries + a slice of common/ and pcsx2/) through mayhem/CMakeLists.txt, which reuses
# upstream's sources unmodified.  Every dependency (zlib, zstd, lzma, png, jpeg, webp) is an apt -dev
# package installed by mayhem/Dockerfile, so the build needs no network and is re-runnable.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
BUILD="$SRC/build-mayhem"   # covered by upstream's /build* .gitignore entry

TARGETS="fuzz_demangle fuzz_ccc fuzz_chd fuzz_zip fuzz_rcheevos fuzz_image fuzz_ini fuzz_strings fuzz_iop fuzz_vu"

# 1) Sanitized + DWARF-3 fuzz harnesses and their -standalone reproducers (mayhem/CMakeLists.txt compiles
#    mayhem/lsan_off.cc with $SANITIZER_FLAGS and links it into every one, turning LeakSanitizer off).
cmake -S mayhem -B "$BUILD/fuzz" -G Ninja \
	-DCMAKE_BUILD_TYPE=None \
	-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
	-DCMAKE_C_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS" \
	-DCMAKE_CXX_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS" \
	-DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS"
targets=()
for t in $TARGETS; do targets+=("$t" "$t-standalone"); done
cmake --build "$BUILD/fuzz" -j"$MAYHEM_JOBS" --target "${targets[@]}"
for t in "${targets[@]}"; do cp -f "$BUILD/fuzz/$t" "$SRC/$t"; done

# 2) The test oracle, with the project's NORMAL flags (no sanitizers): upstream's demangler
#    known-answer suite (3rdparty/demangler/testsuite/demangle-expected).
cmake -S mayhem -B "$BUILD/test" -G Ninja \
	-DCMAKE_BUILD_TYPE=None \
	-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
	-DCMAKE_C_FLAGS="-O1 $COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="-O1 $COVERAGE_FLAGS" \
	-DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build "$BUILD/test" -j"$MAYHEM_JOBS" --target demangler_test
cp -f "$BUILD/test/demangler/demangler_test" "$SRC/demangler_test"
