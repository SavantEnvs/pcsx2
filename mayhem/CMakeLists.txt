# Build of the vendored libraries and the PCSX2 sources the Mayhem fuzz harnesses exercise.
# This is a separate top-level project (the upstream CMake needs Qt, SDL, FFmpeg, ... and
# refuses in-source builds); it only reuses upstream's own sources.  Driven by mayhem/build.sh.
cmake_minimum_required(VERSION 3.16)
project(pcsx2_mayhem C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

set(P2 ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(TP ${P2}/3rdparty)

find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
add_library(Zstd::Zstd ALIAS PkgConfig::ZSTD)

# ---- vendored C/C++ libraries (their own CMake projects) ----
add_subdirectory(${TP}/demangler ${CMAKE_BINARY_DIR}/demangler EXCLUDE_FROM_ALL)
add_subdirectory(${TP}/ccc ${CMAKE_BINARY_DIR}/ccc EXCLUDE_FROM_ALL)
add_subdirectory(${TP}/libzip ${CMAKE_BINARY_DIR}/libzip EXCLUDE_FROM_ALL)
add_subdirectory(${TP}/lzma ${CMAKE_BINARY_DIR}/lzma EXCLUDE_FROM_ALL)
add_subdirectory(${TP}/libchdr ${CMAKE_BINARY_DIR}/libchdr EXCLUDE_FROM_ALL)
add_subdirectory(${TP}/rcheevos ${CMAKE_BINARY_DIR}/rcheevos EXCLUDE_FROM_ALL)
# rcheevos sizes a parse by running it against a NULL buffer (null + 0 pointer arithmetic, member access
# through the NULL scratch pointer, which clang also reports via the object-size check); these
# UBSan checks fire on every well-formed input.  ASan and the other UBSan checks stay on.
target_compile_options(rcheevos PRIVATE -fno-sanitize=null,object-size,pointer-overflow)

# ---- fmt + simpleini + the slice of common/ and pcsx2/ the harnesses need ----
add_library(fmt STATIC ${TP}/fmt/src/format.cc ${TP}/fmt/src/os.cc)
target_include_directories(fmt PUBLIC ${TP}/fmt/include)

add_library(simpleini STATIC ${TP}/simpleini/src/ConvertUTF.c)
target_include_directories(simpleini PUBLIC ${TP}/simpleini/include)

add_library(pcsx2_min STATIC
	${P2}/common/Assertions.cpp
	${P2}/common/Console.cpp
	${P2}/common/Error.cpp
	${P2}/common/FastJmp.cpp
	${P2}/common/FileSystem.cpp
	${P2}/common/Image.cpp
	${P2}/common/ProgressCallback.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/stubs.cpp
	${P2}/common/SmallString.cpp
	${P2}/common/StringUtil.cpp
	${P2}/common/Timer.cpp
	${P2}/common/AlignedMalloc.cpp
	${P2}/pcsx2/INISettingsInterface.cpp
)
target_include_directories(pcsx2_min PUBLIC ${P2} ${P2}/common ${TP}/fast_float/include ${TP}/rapidjson/include)
target_link_libraries(pcsx2_min PUBLIC fmt simpleini PkgConfig::PNG PkgConfig::JPEG PkgConfig::WEBP)

# ---- the CPU cores, linked unmodified (R3000A interpreter + GTE; mayhem/cpu_env.cpp is the host environment) ----
add_library(pcsx2_cpu STATIC
	${P2}/pcsx2/R3000A.cpp
	${P2}/pcsx2/R3000AInterpreter.cpp
	${P2}/pcsx2/R3000AOpcodeTables.cpp
	${P2}/pcsx2/IopGte.cpp
	${P2}/pcsx2/IopMem.cpp
	${P2}/pcsx2/VUops.cpp
	${P2}/pcsx2/VUflags.cpp
	${P2}/pcsx2/VU0microInterp.cpp
	${P2}/pcsx2/VU1microInterp.cpp
	${P2}/pcsx2/Pcsx2Config.cpp
	${P2}/pcsx2/SourceLog.cpp
	${P2}/common/SettingsWrapper.cpp
	${P2}/common/Linux/LnxThreads.cpp
	${P2}/common/Semaphore.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/cpu_env.cpp
)
target_compile_definitions(pcsx2_cpu PUBLIC _M_X86=1 __POSIX__)
target_compile_options(pcsx2_cpu PUBLIC -msse4.1 -mfxsr -fno-exceptions)
# Two UBSan checks fire on ordinary guest instructions and would halt every run within seconds, hiding
# everything behind them; both are benign on x86, where the hardware gives the MIPS-defined result:
#  - alignment: guest loads/stores are `*(u16*)(p + addr)` with guest-controlled, often unaligned, addresses;
#  - shift-exponent: sllv/srlv/srav shift by the whole rs register (R3000AOpcodeTables.cpp:120-122, no `& 31`).
# ASan and every other UBSan check stay on and halting.
target_compile_options(pcsx2_cpu PRIVATE -fno-sanitize=alignment,shift-exponent)
target_include_directories(pcsx2_cpu PUBLIC ${P2}/pcsx2 ${TP}/xbyak ${TP}/ccc/src ${TP}/ccc/include)
target_link_libraries(pcsx2_cpu PUBLIC pcsx2_min ccc)

# ---- fuzz harnesses ----
add_library(lsan_off OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/lsan_off.cc)
add_library(standalone_main OBJECT $ENV{STANDALONE_FUZZ_MAIN})

set(HARNESS ${CMAKE_CURRENT_SOURCE_DIR}/harnesses)
function(add_harness name)
	add_executable(${name} ${HARNESS}/${name}.cpp $<TARGET_OBJECTS:lsan_off>)
	target_link_libraries(${name} PRIVATE ${ARGN})
	target_link_options(${name} PRIVATE -fsanitize=fuzzer)
	add_executable(${name}-standalone ${HARNESS}/${name}.cpp $<TARGET_OBJECTS:lsan_off> $<TARGET_OBJECTS:standalone_main>)
	target_link_libraries(${name}-standalone PRIVATE ${ARGN})
endfunction()

add_harness(fuzz_demangle demanglegnu)
target_include_directories(fuzz_demangle PRIVATE ${TP}/demangler/include)
target_include_directories(fuzz_demangle-standalone PRIVATE ${TP}/demangler/include)

add_harness(fuzz_ccc ccc demanglegnu)
add_harness(fuzz_chd libchdr ZLIB::ZLIB LZMA::LZMA Zstd::Zstd)
add_harness(fuzz_zip zip ZLIB::ZLIB Zstd::Zstd)
add_harness(fuzz_rcheevos rcheevos)
target_include_directories(fuzz_rcheevos PRIVATE ${TP}/rcheevos/src/rcheevos)
target_include_directories(fuzz_rcheevos-standalone PRIVATE ${TP}/rcheevos/src/rcheevos)
add_harness(fuzz_image pcsx2_min)
add_harness(fuzz_ini pcsx2_min)
add_harness(fuzz_strings pcsx2_min)
add_harness(fuzz_iop pcsx2_cpu)
add_harness(fuzz_vu pcsx2_cpu)
