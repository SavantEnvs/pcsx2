// libFuzzer harness for rcheevos (3rdparty/rcheevos): achievement trigger / leaderboard / rich presence parsers.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "rc_runtime_types.h"

#include "rc_validate.h"

static uint32_t Peek(uint32_t address, uint32_t num_bytes, void* ud)
{
	const auto* mem = static_cast<const std::vector<uint8_t>*>(ud);
	uint32_t value = 0;
	for (uint32_t i = 0; i < num_bytes && i < 4; i++)
	{
		const size_t idx = static_cast<size_t>(address) + i;
		if (idx < mem->size())
			value |= static_cast<uint32_t>((*mem)[idx]) << (8 * i);
	}
	return value;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2 || size > 8192)
		return -1;

	const uint8_t mode = data[0] & 3;
	const std::string script(reinterpret_cast<const char*>(data) + 1, size - 1);
	std::vector<uint8_t> memory(256);
	for (size_t i = 0; i < memory.size(); i++)
		memory[i] = static_cast<uint8_t>(data[i % size] + i);

	char buf[256];
	switch (mode)
	{
		case 0:
		{
			const int sz = rc_trigger_size(script.c_str());
			if (sz <= 0)
				return 0;
			void* mem = malloc(sz);
			rc_trigger_t* trigger = rc_parse_trigger(mem, script.c_str(), nullptr, 0);
			if (trigger)
			{
				rc_validate_trigger(trigger, buf, sizeof(buf), 0xFFFFFF);
				for (int i = 0; i < 3; i++)
					rc_test_trigger(trigger, Peek, &memory, nullptr);
			}
			free(mem);
			break;
		}
		case 1:
		{
			const int sz = rc_value_size(script.c_str());
			if (sz <= 0)
				return 0;
			void* mem = malloc(sz);
			rc_value_t* value = rc_parse_value(mem, script.c_str(), nullptr, 0);
			if (value)
			{
				rc_evaluate_value(value, Peek, &memory, nullptr);
				rc_format_value(buf, sizeof(buf), rc_evaluate_value(value, Peek, &memory, nullptr), data[1] % 16);
			}
			free(mem);
			break;
		}
		case 2:
		{
			const int sz = rc_lboard_size(script.c_str());
			if (sz <= 0)
				return 0;
			void* mem = malloc(sz);
			rc_lboard_t* lboard = rc_parse_lboard(mem, script.c_str(), nullptr, 0);
			if (lboard)
				{
				int32_t lb_value = 0;
				rc_evaluate_lboard(lboard, &lb_value, Peek, &memory, nullptr);
			}
			free(mem);
			break;
		}
		default:
		{
			const int sz = rc_richpresence_size(script.c_str());
			if (sz <= 0)
				return 0;
			void* mem = malloc(sz);
			rc_richpresence_t* rp = rc_parse_richpresence(mem, script.c_str(), nullptr, 0);
			if (rp)
			{
				char out[512];
				rc_evaluate_richpresence(rp, out, sizeof(out), Peek, &memory, nullptr);
			}
			free(mem);
			break;
		}
	}
	return 0;
}
