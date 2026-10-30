// libFuzzer harness for the vendored libzip (3rdparty/libzip): open an archive from memory and extract entries.
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <zip.h>
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 22 || size > (1u << 20))
		return -1;

	zip_error_t error;
	zip_error_init(&error);
	zip_source_t* source = zip_source_buffer_create(data, size, 0, &error);
	if (!source)
	{
		zip_error_fini(&error);
		return 0;
	}

	zip_t* za = zip_open_from_source(source, ZIP_RDONLY, &error);
	if (!za)
	{
		zip_source_free(source);
		zip_error_fini(&error);
		return 0;
	}

	const zip_int64_t count = zip_get_num_entries(za, 0);
	for (zip_int64_t i = 0; i < count && i < 32; i++)
	{
		zip_stat_t st;
		if (zip_stat_index(za, static_cast<zip_uint64_t>(i), 0, &st) != 0)
			continue;
		zip_get_name(za, static_cast<zip_uint64_t>(i), 0);

		zip_file_t* zf = zip_fopen_index(za, static_cast<zip_uint64_t>(i), 0);
		if (!zf)
			continue;
		char buf[4096];
		size_t total = 0;
		zip_int64_t n;
		while ((n = zip_fread(zf, buf, sizeof(buf))) > 0 && total < (4u << 20))
			total += static_cast<size_t>(n);
		zip_fclose(zf);
	}

	zip_discard(za);
	zip_error_fini(&error);
	return 0;
}
