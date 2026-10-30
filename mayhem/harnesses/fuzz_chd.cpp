// libFuzzer harness for libchdr (3rdparty/libchdr): open a CHD image from memory, read hunks and metadata.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <libchdr/chd.h>
}

namespace {
struct MemFile
{
	const uint8_t* data;
	uint64_t size;
	uint64_t pos;
};

uint64_t MemSize(core_file* f) { return static_cast<MemFile*>(f->argp)->size; }

size_t MemRead(void* ptr, size_t size, size_t nmemb, core_file* f)
{
	MemFile* m = static_cast<MemFile*>(f->argp);
	const uint64_t want = static_cast<uint64_t>(size) * nmemb;
	if (m->pos >= m->size || want == 0)
		return 0;
	const uint64_t n = want < m->size - m->pos ? want : m->size - m->pos;
	memcpy(ptr, m->data + m->pos, n);
	m->pos += n;
	return size ? n / size : 0;
}

int MemClose(core_file* f)
{
	free(f->argp);
	free(f);
	return 0;
}

int MemSeek(core_file* f, int64_t offset, int whence)
{
	MemFile* m = static_cast<MemFile*>(f->argp);
	int64_t base = 0;
	if (whence == SEEK_CUR)
		base = static_cast<int64_t>(m->pos);
	else if (whence == SEEK_END)
		base = static_cast<int64_t>(m->size);
	const int64_t np = base + offset;
	if (np < 0)
		return -1;
	m->pos = static_cast<uint64_t>(np);
	return 0;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 16)
		return -1;

	core_file* cf = static_cast<core_file*>(malloc(sizeof(core_file)));
	MemFile* mf = static_cast<MemFile*>(malloc(sizeof(MemFile)));
	mf->data = data;
	mf->size = size;
	mf->pos = 0;
	cf->argp = mf;
	cf->fsize = MemSize;
	cf->fread = MemRead;
	cf->fclose = MemClose;
	cf->fseek = MemSeek;

	chd_file* chd = nullptr;
	if (chd_open_core_file(cf, CHD_OPEN_READ, nullptr, &chd) != CHDERR_NONE)
		return 0;

	const chd_header* header = chd_get_header(chd);
	if (header->hunkbytes != 0 && header->hunkbytes <= (1u << 20))
	{
		std::vector<uint8_t> hunk(header->hunkbytes);
		const uint32_t count = header->totalhunks < 16 ? header->totalhunks : 16;
		for (uint32_t i = 0; i < count; i++)
			chd_read(chd, i, hunk.data());
	}

	uint8_t meta[512];
	uint32_t len;
	for (uint32_t idx = 0; idx < 4; idx++)
		chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, idx, meta, sizeof(meta), &len, nullptr, nullptr);
	chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, 0, meta, sizeof(meta), &len, nullptr, nullptr);
	chd_get_metadata(chd, HARD_DISK_METADATA_TAG, 0, meta, sizeof(meta), &len, nullptr, nullptr);

	chd_close(chd);
	return 0;
}
