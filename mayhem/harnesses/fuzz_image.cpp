// libFuzzer harness for PCSX2's image loaders (common/Image.cpp): PNG, JPEG, WebP and BMP from memory.
#include <cstdint>
#include <cstring>

#include "common/Image.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 8 || size > (1u << 20))
		return -1;

	const char* name;
	if (!memcmp(data, "\x89PNG", 4))
		name = "fuzz.png";
	else if (data[0] == 0xFF && data[1] == 0xD8)
		name = "fuzz.jpg";
	else if (!memcmp(data, "RIFF", 4))
		name = "fuzz.webp";
	else if (!memcmp(data, "BM", 2))
		name = "fuzz.bmp";
	else
		return -1;

	RGBA8Image image;
	if (image.LoadFromBuffer(name, data, size) && image.GetWidth() > 0 && image.GetHeight() > 0)
		image.SaveToBuffer("out.png", 50);
	return 0;
}
