// libFuzzer harness for the ccc symbol-table importer (3rdparty/ccc) as used by the PCSX2 debugger:
// ELF / SNDLL parsing plus .symtab / .mdebug / STABS import.
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "ccc/ast.h"
#include "ccc/importer_flags.h"
#include "ccc/symbol_database.h"
#include "ccc/symbol_file.h"
#include "ccc/symbol_table.h"

extern "C" {
#include "demangle.h"
}

static void QuietErrors(const ccc::Error&, ccc::ErrorLevel) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 16 || size > (4u << 20))
		return -1;

	static const bool once = (ccc::set_custom_error_callback(QuietErrors), true);
	(void)once;

	std::vector<ccc::u8> image(data, data + size);
	ccc::Result<std::unique_ptr<ccc::SymbolFile>> file = ccc::parse_symbol_file(std::move(image), "fuzz.elf");
	if (!file.success())
		return 0;

	ccc::Result<std::vector<std::unique_ptr<ccc::SymbolTable>>> tables = (*file)->get_all_symbol_tables();
	if (!tables.success())
		return 0;

	ccc::DemanglerFunctions demangler;
	demangler.cplus_demangle = cplus_demangle;
	demangler.cplus_demangle_opname = cplus_demangle_opname;

	static const std::atomic_bool interrupt{false};
	ccc::SymbolDatabase database;
	(void)ccc::import_symbol_tables(database, *tables, (*file)->name(), ccc::Address(),
		ccc::NO_MEMBER_FUNCTIONS | ccc::NO_OPTIMIZED_OUT_FUNCTIONS | ccc::UNIQUE_FUNCTIONS | ccc::DEMANGLE_PARAMETERS | ccc::DEMANGLE_RETURN_TYPE,
		demangler, &interrupt);
	return 0;
}
