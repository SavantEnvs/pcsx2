// libFuzzer harness for PCSX2's string and path helpers (common/StringUtil.cpp, common/Path.h).
#include <cstdint>
#include <string>
#include <string_view>

#include "common/Path.h"
#include "common/StringUtil.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 2 || size > 4096)
		return -1;

	const size_t split = data[0] % size;
	const std::string_view a(reinterpret_cast<const char*>(data) + 1, split > 0 ? split - 1 : 0);
	const std::string_view b(reinterpret_cast<const char*>(data) + split + (split == 0 ? 1 : 0), size - split - (split == 0 ? 1 : 0));

	Path::Canonicalize(a);
	Path::Combine(a, b);
	Path::SanitizeFileName(a);
	Path::IsValidFileName(a, true);
	Path::MakeRelative(a, b);
	Path::GetExtension(a);
	Path::StripExtension(a);
	Path::ReplaceExtension(a, b);
	Path::GetDirectory(a);
	Path::GetFileName(a);
	Path::GetFileTitle(a);
	Path::ChangeFileName(a, b);
	Path::AppendDirectory(a, b);
	Path::JoinWindowsPath(Path::SplitWindowsPath(a));
	Path::JoinNativePath(Path::SplitNativePath(a));
	Path::URLDecode(a);
	Path::URLEncode(a);
	if (Path::IsAbsolute(a) && !Path::SplitNativePath(a).empty())
		Path::CreateFileURL(a);
	Path::BuildRelativePath(a, b);

	std::string_view key, value;
	StringUtil::ParseAssignmentString(a, &key, &value);
	StringUtil::DecodeHex(a);
	StringUtil::SplitString(a, ',');
	StringUtil::StripWhitespace(a);
	StringUtil::ReplaceAll(a, b, a);
	StringUtil::Ellipsise(a, 12);
	StringUtil::toLower(a);
	StringUtil::compareNoCase(a, b);
	StringUtil::FromChars<int>(a);
	StringUtil::FromChars<uint64_t>(a, 16);
	StringUtil::FromChars<double>(a);
	StringUtil::FromChars<float>(a);
	const std::string sa(a), sb(b);
	StringUtil::WildcardMatch(sa.c_str(), sb.c_str(), false);
	StringUtil::splitOnNewLine(sa);
	char32_t ch;
	for (size_t off = 0; off < a.size();)
	{
		const size_t n = StringUtil::DecodeUTF8(a, off, &ch);
		off += n ? n : 1;
	}
	return 0;
}
