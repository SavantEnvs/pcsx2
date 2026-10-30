// libFuzzer harness for PCSX2's INI settings parser (pcsx2/INISettingsInterface.cpp over SimpleIni).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "pcsx2/INISettingsInterface.h"
#include "common/Error.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 20))
		return -1;

	const char* tmpdir = getenv("TMPDIR");
	std::string path = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/fuzz_ini.XXXXXX";
	const int fd = mkstemp(path.data());
	if (fd < 0)
		return 0;
	const bool wrote = write(fd, data, size) == static_cast<ssize_t>(size);
	close(fd);

	if (wrote)
	{
		INISettingsInterface si(path);
		if (si.Load())
		{
			std::string value;
			s32 ivalue;
			float fvalue;
			bool bvalue;
			si.GetStringValue("Folders", "Cache", &value);
			si.GetIntValue("EmuCore", "CdvdVerboseReads", &ivalue);
			si.GetFloatValue("EmuCore/GS", "upscale_multiplier", &fvalue);
			si.GetBoolValue("EmuCore", "EnableCheats", &bvalue);
			si.GetStringList("EmuCore", "Items");
			si.GetKeyValueList("Pad1");
			si.GetKeyValueList("");
			si.SetStringValue("Fuzz", "Key", "Value");
			Error err;
			si.Save(&err);
		}
	}
	unlink(path.c_str());
	return 0;
}
