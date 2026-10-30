// libFuzzer harness for PCSX2's VU0/VU1 microprogram interpreter (pcsx2/VUops.cpp, VUflags.cpp,
// VU0microInterp.cpp, VU1microInterp.cpp).
//
// The input is a small machine state followed by a raw VU microprogram, which is run from address 0 for a
// bounded number of cycles through the same InterpVU0/InterpVU1::Execute the emulator uses:
//
//   u8 unit (bit 0: 0 = VU0, 1 = VU1), u8 reserved, u16 FBRST, u16 vi1..vi15 (15 little-endian words), u16 reserved
//   microprogram (64-bit upper/lower instruction pairs; truncated to the unit's program memory)
//
// Data memory and VF1..VF31 are filled from the same bytes, so the program controls what it loads.
#include <cstdint>
#include <cstring>

#include "Common.h"
#include "Config.h"
#include "Memory.h"
#include "Gif_Unit.h"
#include "VU.h"
#include "VUmicro.h"

namespace
{
	constexpr size_t kHeaderBytes = 36;
	constexpr u32 kCycles = 4000;

	bool s_init;
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < kHeaderBytes + 8 || size > kHeaderBytes + VU1_PROGSIZE)
		return -1;

	if (!s_init)
	{
		s_init = true;
		vuMemAllocate();
		CpuIntVU0.Reset();
		CpuIntVU1.Reset();
	}

	for (auto& path : gifUnit.gifPath)
		path.Reset();

	const bool vu1 = data[0] & 1;
	VURegs& vu = vu1 ? VU1 : VU0;
	const u32 progSize = vu1 ? VU1_PROGSIZE : VU0_PROGSIZE;
	const u32 memSize = vu1 ? VU1_MEMSIZE : VU0_MEMSIZE;

	u8* const micro = vu.Micro;
	u8* const mem = vu.Mem;
	std::memset(&vuRegs[vu1], 0, sizeof(VURegs));
	vu.Micro = micro;
	vu.Mem = mem;
	std::memset(&_cpuRegistersPack, 0, sizeof(_cpuRegistersPack));

	const size_t codeSize = std::min<size_t>(size - kHeaderBytes, progSize);
	std::memset(micro, 0, progSize);
	std::memcpy(micro, data + kHeaderBytes, codeSize);
	for (u32 i = 0; i < memSize; i += static_cast<u32>(codeSize))
		std::memcpy(mem + i, micro, std::min<size_t>(codeSize, memSize - i));
	for (int i = 1; i < 32; i++)
		std::memcpy(&vu.VF[i], mem + (i - 1) * 16, 16);
	vu.VF[0].f.w = 1.0f;

	for (int i = 1; i < 16; i++)
	{
		uint16_t v;
		std::memcpy(&v, data + 4 + (i - 1) * 2, 2);
		vu.VI[i].UL = v;
	}
	uint16_t fbrst;
	std::memcpy(&fbrst, data + 2, 2);
	VU0.VI[REG_FBRST].UL = fbrst;
	VU0.VI[REG_VPU_STAT].UL = vu1 ? 0x100 : 0x1;
	vu.VI[REG_TPC].UL = 0;

	(vu1 ? static_cast<BaseVUmicroCPU&>(CpuIntVU1) : static_cast<BaseVUmicroCPU&>(CpuIntVU0)).SetStartPC(0);
	(vu1 ? static_cast<BaseVUmicroCPU&>(CpuIntVU1) : static_cast<BaseVUmicroCPU&>(CpuIntVU0)).Execute(kCycles);
	return 0;
}
