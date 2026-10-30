// Host environment for the CPU-core fuzz targets (fuzz_iop, ...): upstream's R3000A interpreter, GTE and
// IOP memory map are linked unmodified; everything below stands in for the emulator machinery around them
// (VM memory allocation, debugger, peripherals, interrupt consumers).  None of it contains CPU behaviour.
#include "common/Pcsx2Defs.h"
#include "Common.h"
#include "Config.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900OpcodeTables.h"
#include "IopBios.h"
#include "IopHw.h"
#include "IopMem.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/SymbolGuardian.h"
#include "SIO/Sio0.h"
#include "Host/AudioStream.h"
#include "SIO/Pad/Pad.h"
#include "USB/USB.h"
#include "SIO/Memcard/MemoryCardFile.h"
#include "VMManager.h"
#include "VUmicro.h"
#include "Gif_Unit.h"
#include "MTVU.h"
#include "common/HostSys.h"
#include "Gif.h"
#include "Dmac.h"
#include "R5900.h"
#include "Host.h"

#include <sys/mman.h>
#include <cstdlib>

// ---- VM memory: one lazily committed anonymous mapping instead of the shared-memory arena ----
namespace
{
	u8* s_data;
}

u8* SysMemory::GetDataPtr(size_t offset)
{
	if (!s_data)
	{
		void* p = mmap(nullptr, HostMemoryMap::MainSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
		if (p == MAP_FAILED)
			abort();
		s_data = static_cast<u8*>(p);
	}
	return s_data + offset;
}

alignas(__pagealignsize) u8 eeHw[Ps2MemSize::Hardware];
EEVM_MemoryAllocMess* eeMem;
namespace Ps2MemSize
{
	u32 ExposedRam = MainRam;
	u32 ExposedIopRam = IopRam;
} // namespace Ps2MemSize
u32 PSXCLK = 36864000;
bool eeEventTestIsActive = false;

// ---- debugger: no breakpoints, no memchecks ----
std::vector<MemCheck> CBreakPoints::memChecks_;
bool CBreakPoints::breakpointTriggered_;
BreakPointCpu CBreakPoints::breakpointTriggeredCpu_;
bool CBreakPoints::IsAddressBreakPoint(BreakPointCpu, u32) { return false; }
u32 CBreakPoints::CheckSkipFirst(BreakPointCpu, u32) { return 0; }
void CBreakPoints::ClearSkipFirst(BreakPointCpu) {}
BreakPointCond* CBreakPoints::GetBreakPointCondition(BreakPointCpu, u32) { return nullptr; }
bool CBreakPoints::HandleBreakpointHit(BreakPointCpu, u32) { return false; }
const std::vector<MemCheck> CBreakPoints::GetMemChecks(BreakPointCpu) { return {}; }
SymbolGuardian R3000SymbolGuardian;
void SymbolGuardian::ClearIrxModules() {}
bool DebugInterface::parseExpression(const PostfixExpression&, u64&, std::string&) { return false; }
void VMManager::SetPaused(bool) {}
R5900cpu* Cpu;
const R5900::OPCODE& R5900::GetInstruction(u32)
{
	static const OPCODE none{};
	return none;
}

// ---- IOP HLE / peripherals: not part of the CPU core ----
int R3000A::irxImportExec(u32, u16) { return 0; }
u32 R3000A::irxImportTableAddr(u32) { return 0; }
void R3000A::ioman::reset() {}
void psxBiosReset() {}
bool psxBiosCall() { return false; }
void psxHwReset() {}
s32 psxNextDeltaCounter = 0x7fffffff;
u64 psxNextStartCounter;
void psxRcntUpdate() {}
void cpuSetNextEventDelta(s32) {}
void sif0Interrupt() {}
void sif1Interrupt() {}
void sif2Interrupt() {}
void cdvdSectorReady() {}
void cdvdReadInterrupt() {}
void cdvdActionInterrupt() {}
void cdrInterrupt() {}
void cdrReadInterrupt() {}
void psxDMA11Interrupt() {}
void psxDMA12Interrupt() {}
void dev9Interrupt() {}
void usbInterrupt() {}

u8 DEV9read8(u32) { return 0; }
u16 DEV9read16(u32) { return 0; }
u32 DEV9read32(u32) { return 0; }
void DEV9write8(u32, u8) {}
void DEV9write16(u32, u16) {}
void DEV9write32(u32, u32) {}
u16 SPU2read(u32) { return 0; }
void SPU2write(u32, u16) {}
u8 psxHw4Read8(u32) { return 0; }
void psxHw4Write8(u32, u8) {}

#define IOP_HW_PAGE(n) \
	u8 IopMemory::iopHwRead8_Page##n(u32 a) { return psxHu8(a); } \
	u16 IopMemory::iopHwRead16_Page##n(u32 a) { return psxHu16(a); } \
	u32 IopMemory::iopHwRead32_Page##n(u32 a) { return psxHu32(a); } \
	void IopMemory::iopHwWrite8_Page##n(u32 a, u8 v) { psxHu8(a) = v; } \
	void IopMemory::iopHwWrite16_Page##n(u32 a, u16 v) { psxHu16(a) = v; } \
	void IopMemory::iopHwWrite32_Page##n(u32 a, u32 v) { psxHu32(a) = v; }
IOP_HW_PAGE(1)
IOP_HW_PAGE(3)
IOP_HW_PAGE(8)

Sio0 g_Sio0;
Sio0::Sio0() = default;
Sio0::~Sio0() = default;
void Sio0::Interrupt(Sio0Interrupt) {}

// ---- settings: Pcsx2Config.cpp's defaults are real, the front-end pieces it names are not ----
const char* AudioStream::GetBackendName(AudioBackend) { return ""; }
std::optional<AudioBackend> AudioStream::ParseBackendName(const char*) { return std::nullopt; }
bool FileMcd_IsMultitapSlot(uint) { return false; }
std::string FileMcd_GetDefaultName(uint) { return {}; }
uint FileMcd_GetMtapPort(uint) { return 0; }
uint FileMcd_GetMtapSlot(uint) { return 0; }
const char* Host::TranslateToCString(const std::string_view, const std::string_view msg) { return msg.data(); }
void AudioStreamParameters::LoadSave(SettingsWrapper&, const char*) {}
s32 USB::DeviceTypeNameToIndex(const std::string_view) { return -1; }
const char* USB::DeviceTypeIndexToName(s32) { return ""; }
std::string USB::GetConfigSection(int) { return {}; }
Pad::ControllerType Pad::GetDefaultPadType(u32) { return Pad::ControllerType::NotConnected; }
const Pad::ControllerInfo* Pad::GetControllerInfo(Pad::ControllerType) { return nullptr; }
const Pad::ControllerInfo* Pad::GetControllerInfoByName(const std::string_view) { return nullptr; }
std::string Pad::GetConfigSection(u32) { return {}; }

// ---- EE side of the VU interpreters: register file, VU memory, and the GIF/INTC consumers of XGKICK / E-bit ----
alignas(16) cpuRegistersPack _cpuRegistersPack;
alignas(16) VURegs vuRegs[2];
Gif_Unit gifUnit;
void vuMemAllocate()
{
	u8* curpos = SysMemory::GetVUMem();
	VU0.Micro = curpos; curpos += VU0_PROGSIZE;
	VU0.Mem = curpos; curpos += VU0_MEMSIZE;
	VU1.Micro = curpos; curpos += VU1_PROGSIZE;
	VU1.Mem = curpos;
}
void hwIntcIrq(int) {}
void CPU_INT(EE_EventType, s32) {}
void gifCheckPathStatus(bool) {}
void Gif_MTGS_Wait(bool) {}
void Gif_FinishIRQ() {}
bool Gif_HandlerAD(u8*) { return false; }
void Gif_HandlerAD_MTVU(u8*) {}
void Gif_AddBlankGSPacket(u32, GIF_PATH) {}
void Gif_AddGSPacketMTVU(GS_Packet&, GIF_PATH) {}
void Gif_AddCompletedGSPacket(GS_Packet&, GIF_PATH) {}
VU_Thread vu1Thread;
VU_Thread::VU_Thread() = default;
VU_Thread::~VU_Thread() = default;
// Semaphore.cpp's spin-wait calibration; only reached by the (never started) MTVU thread object above.
u32 ShortSpin() { return 0; }
const u32 SPIN_TIME_NS = 0;
