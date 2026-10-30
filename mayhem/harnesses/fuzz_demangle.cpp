// libFuzzer harness for the vendored GNU libiberty demangler (3rdparty/demangler).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "demangle.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2 || size > 4096)
		return -1;

	static const int styles[] = {DMGL_AUTO, DMGL_GNU_V3, DMGL_JAVA, DMGL_GNAT, DMGL_DLANG, DMGL_RUST, DMGL_GNU, DMGL_ARM};
	const int style = styles[data[0] % (sizeof(styles) / sizeof(styles[0]))];
	const int flags = (data[1] & (DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_TYPES | DMGL_RET_POSTFIX | DMGL_RET_DROP));

	const std::string mangled(reinterpret_cast<const char*>(data) + 2, size - 2);

	char* out = cplus_demangle(mangled.c_str(), style | flags);
	free(out);
	out = cplus_demangle_opname(mangled.c_str(), flags);
	free(out);
	out = dlang_demangle(mangled.c_str(), flags);
	free(out);
	out = rust_demangle(mangled.c_str(), flags);
	free(out);
	return 0;
}
