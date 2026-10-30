// libFuzzer harness for PCSX2's IOP (PlayStation R3000A + GTE) interpreter.
//
// The input is a small machine state followed by raw MIPS code: it is loaded into IOP RAM and run through
// upstream's interpreter (pcsx2/R3000AInterpreter.cpp, R3000AOpcodeTables.cpp, IopGte.cpp, R3000A.cpp)
// and its memory map (IopMem.cpp), for a bounded number of instructions.
//
//   u32 cp0_status, u32 r1..r31, u32 hi, u32 lo   (34 little-endian words)
//   MIPS I code, loaded at 0x1000 (up to 64 KiB); execution starts there
#include <cstdint>
#include <cstring>

#include "Common.h"
#include "Config.h"
#include "IopHw.h"
#include "IopMem.h"
#include "Memory.h"
#include "R3000A.h"

namespace
{
	constexpr size_t kHeaderWords = 34;
	constexpr size_t kHeaderBytes = kHeaderWords * 4;
	constexpr size_t kMaxCode = 64 * 1024;
	constexpr u32 kLoadAddr = 0x1000;
	constexpr int kMaxSteps = 20000;

	bool s_init;

	// Same fetch/decode/dispatch as the interpreter's execI(); that function runs until the next branch, which
	// never comes in straight-line code, so the instruction budget is applied here.
	void Step()
	{
		psxRegs.code = iopMemRead32(psxRegs.pc);
		psxRegs.pc += 4;
		psxRegs.cycle++;
		psxBSC[psxRegs.code >> 26]();
	}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < kHeaderBytes + 4 || size > kHeaderBytes + kMaxCode)
		return -1;

	if (!s_init)
	{
		s_init = true;
		eeMem = reinterpret_cast<EEVM_MemoryAllocMess*>(SysMemory::GetEEMem());
		iopMemAlloc();
		psxCpu = &psxInt;
	}

	Ps2MemSize::ExposedIopRam = Ps2MemSize::IopRam;
	iopMemReset();
	std::memset(iopHw, 0, sizeof(iopHw));

	u32 hdr[kHeaderWords];
	std::memcpy(hdr, data, sizeof(hdr));

	std::memset(&psxRegs, 0, sizeof(psxRegs));
	psxRegs.CP0.n.Status = hdr[0];
	psxRegs.CP0.n.PRid = 0x1f;
	std::memcpy(&psxRegs.GPR.r[1], &hdr[1], 31 * sizeof(u32));
	psxRegs.GPR.n.hi = hdr[32];
	psxRegs.GPR.n.lo = hdr[33];
	psxRegs.pc = kLoadAddr;
	psxRegs.iopCycleEE = -1;

	std::memcpy(&iopMem->Main[kLoadAddr], data + kHeaderBytes, size - kHeaderBytes);

	for (int i = 0; i < kMaxSteps; i++)
		Step();

	return 0;
}
