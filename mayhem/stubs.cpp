#include <cstdlib>

// HostSys.cpp drags in cpuinfo and platform code; Assertions.cpp only needs this one symbol.
[[noreturn]] void AbortWithMessage(const char*)
{
	abort();
}
