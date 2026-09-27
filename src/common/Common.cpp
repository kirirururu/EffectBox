#include "Common.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

std::string getSocketPath()
{
	if (const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
	    runtimeDir != nullptr && fs::is_directory(runtimeDir))
		return (fs::path{runtimeDir} / "EffectBox.sock").string();

	return "/tmp/EffectBox.sock";
}
