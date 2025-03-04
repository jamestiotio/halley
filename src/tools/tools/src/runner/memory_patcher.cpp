#include "halley/tools/runner/memory_patcher.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <functional>
#include <gsl/gsl_assert>

#include "halley/data_structures/hash_map.h"
#include "halley/support/console.h"

using namespace Halley;

constexpr static bool verboseLogging = false;
constexpr static bool semiVerboseLogging = true;

void MemoryPatchingMappings::generate(const Vector<DebugSymbol>& prev, const Vector<DebugSymbol>& next)
{
	struct Mapping
	{
		void* from = nullptr;
		void* to = nullptr;
		std::string name;
	};

	// Construct mapping
	HashMap<std::string, Mapping> mapping;
	for (const auto& p: prev) {
		mapping[p.getName()].from = p.getAddress();
	}
	for (const auto& n: next) {
		mapping[n.getName()].to = n.getAddress();
	}

	// Flatten
	Vector<Mapping> flatMap;
	for (auto& kv: mapping) {
		const auto& m = kv.second;
		if (m.from != nullptr && m.from != m.to) {
			flatMap.push_back(m);
			flatMap.back().name = kv.first;
		}
	}
	mapping.clear();

	// Sort by from address
	std::sort(flatMap.begin(), flatMap.end(), [](const Mapping& a, const Mapping& b) -> bool { return a.from < b.from; });

	// Copy to src and dst arrays
	minSrc = reinterpret_cast<void*>(-1);
	maxSrc = nullptr;
	for (const auto& m: flatMap) {
		minSrc = std::min(minSrc, m.from);
		maxSrc = std::max(maxSrc, m.from);
		src.push_back(m.from);
		dst.push_back(m.to);
		name.push_back(m.name);
	}

	std::cout << "Generated " << src.size() << " memory re-mappings. From " << prev.size() << " to " << next.size() << " symbols, on " << minSrc << " to " << maxSrc << " range." << std::endl;
	
	if constexpr (verboseLogging) {
		for (size_t i = 0; i < src.size(); i++) {
			std::cout << "[" << src[i] << " -> " << dst[i] << "] " << name[i] << "\n";
		}
	}
}

void MemoryPatchingMappings::generate(const Vector<DebugSymbol>& symbols)
{
	struct Mapping
	{
		void* from = nullptr;
		void* to = nullptr;
		std::string name;
	};

	// Construct mapping
	Vector<Mapping> flatMap;
	for (const auto& p: symbols) {
		flatMap.push_back(Mapping{ p.getAddress(), nullptr, p.getName() });
	}

	// Sort by from address
	std::sort(flatMap.begin(), flatMap.end(), [](const Mapping& a, const Mapping& b) -> bool { return a.from < b.from; });

	// Copy to src and dst arrays
	minSrc = reinterpret_cast<void*>(-1);
	maxSrc = nullptr;
	for (const auto& m: flatMap) {
		minSrc = std::min(minSrc, m.from);
		maxSrc = std::max(maxSrc, m.from);
		src.push_back(m.from);
		dst.push_back(m.to);
		name.push_back(m.name);
	}
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

namespace {
	size_t scanProcessMemory(std::function<size_t(void*, size_t)> f)
	{
		size_t patchings = 0;
		size_t totalMemory = 0;

		MEMORY_BASIC_INFORMATION membasic;
		for (char* address = 0; VirtualQuery(address, &membasic, sizeof(membasic)); address += membasic.RegionSize) {
			if (membasic.State == MEM_COMMIT) {
				constexpr unsigned int acceptMask = 0x04 | 0x08 | 0x40 | 0x80;
				if ((membasic.Protect & acceptMask) == membasic.Protect) {
					patchings += f(address, membasic.RegionSize);
					totalMemory += membasic.RegionSize;
				}
			}
		}

		std::cout << "Total memory scanned: " << totalMemory << std::endl;
		return patchings;
	}
}

#else

static size_t scanProcessMemory(std::function<size_t(void*, size_t)>)
{
	// TODO
	return 0;
}

#endif

void MemoryPatcher::patch(const MemoryPatchingMappings& mappings)
{
	if (mappings.src.empty()) {
		std::cout << "Nothing to patch." << std::endl;
	} else {
		size_t n = scanProcessMemory([&](void* ptr, size_t size) {
			return patchMemory(ptr, size, mappings);
		});
		std::cout << "Patched " << n << " pointers." << std::endl;
	}
}

size_t MemoryPatcher::patchMemory(void* address, size_t len, const MemoryPatchingMappings& mappings)
{
	size_t count = 0;

	Expects(len % sizeof(size_t) == 0);
	using ptr = void*;
	ptr* start = reinterpret_cast<ptr*>(address);
	ptr* end = start + (len / sizeof(ptr));

	const void* avoid0start = mappings.src.data();
	const void* avoid0end = mappings.src.data() + mappings.src.size();
	const void* avoid1start = mappings.dst.data();
	const void* avoid1end = mappings.dst.data() + mappings.src.size();
	const void* avoid2start = reinterpret_cast<const void*>(&mappings);
	const void* avoid2end = reinterpret_cast<const char*>(avoid2start) + sizeof(MemoryPatchingMappings);

	if constexpr (verboseLogging) {
		bool isEvilRange = (avoid0start >= start && avoid0start <= end) || (avoid1start >= start && avoid1start <= end) || (avoid2start >= start && avoid2start <= end);
		if (isEvilRange) std::cout << ConsoleColour(Console::RED);
		std::cout << "[" << start << " -> " << (end - 1) << "] (" << len << " bytes)\n";
		if (isEvilRange) std::cout << ConsoleColour(Console::BLUE);
	}

	for (ptr* p = start; p < end; ++p) {
		ptr val = *p;
		if (val >= mappings.minSrc && val <= mappings.maxSrc) {
			auto iter = std::lower_bound(mappings.src.begin(), mappings.src.end(), val);
			if (iter != mappings.src.end()) {
				size_t n = iter - mappings.src.begin();
				if (mappings.src[n] == val) {
					if (p >= avoid0start && p < avoid0end) {
						if constexpr (verboseLogging) {
							std::cout << "Prevent patching at " << p << ", as it's inside src mapping vector.\n";
						}
					} else if (p >= avoid1start && p < avoid1end) {
						if constexpr (verboseLogging) {
							std::cout << "Prevent patching at " << p << ", as it's inside dst mapping vector.\n";
						}
					} else if (p >= avoid2start && p < avoid2end) {
						if constexpr (verboseLogging) {
							std::cout << "Prevent patching at " << p << ", as it's inside MemoryPatchingMappings.\n";
						}
					} else {
						if constexpr (verboseLogging || semiVerboseLogging) {
							std::cout << "! " << p << ": " << val << " -> " << mappings.dst[n] << ": " << mappings.name[n] << "\n";
						}
						*p = mappings.dst[n];
						count++;
					}
				}
			}
		}
	}

	return count;
}
