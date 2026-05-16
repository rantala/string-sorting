/*
 * Copyright 2011 by Tommi Rantala <tt.rantala@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "vmainfo.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/* Format the /proc/pid/smaps key-value pairs into two columns:
 *
 * Size:                136 kB
 * Rss:                   8 kB
 * Pss:                   8 kB
 * Shared_Clean:          0 kB
 * Shared_Dirty:          0 kB
 * Private_Clean:         0 kB
 * Private_Dirty:         8 kB
 * Referenced:            8 kB
 * Anonymous:             8 kB
 * AnonHugePages:         0 kB
 * Swap:                  0 kB
 * KernelPageSize:        4 kB
 * MMUPageSize:           4 kB
 * Locked:                0 kB
 *
 * =>
 *
 * Size:             390636 kB  |  Referenced:       390636 kB
 * Rss:              390636 kB  |  Anonymous:        390636 kB
 * Pss:              390636 kB  |  AnonHugePages:         0 kB
 * Shared_Clean:          0 kB  |  Swap:                  0 kB
 * Shared_Dirty:          0 kB  |  KernelPageSize:        4 kB
 * Private_Clean:         0 kB  |  MMUPageSize:           4 kB
 * Private_Dirty:    390636 kB  |  Locked:                0 kB
 */

namespace {

struct Entry {
	std::string header;
	std::vector<std::string> pairs;
};

Entry
find_entry(unsigned long target)
{
	Entry e;
	std::ifstream f("/proc/self/smaps");
	if (!f) return e;
	for (std::string line; std::getline(f, line); ) {
		unsigned long a, b;
		if (std::sscanf(line.c_str(), "%lx-%lx", &a, &b) != 2)
			continue;
		if (!(a <= target && target < b))
			continue;
		e.header = std::move(line);
		while (std::getline(f, line)) {
			if (line.empty()) break;
			if (line[0] < 'A' || line[0] > 'Z') break;
			if (line.find(':') == std::string::npos) break;
			e.pairs.push_back(std::move(line));
		}
		break;
	}
	return e;
}

std::string
format(const Entry &e)
{
	if (e.header.empty()) return std::string();
	std::ostringstream out;
	out << "    " << e.header << '\n';
	const size_t n = e.pairs.size();
	const size_t half = n / 2;
	for (size_t i = 0, j = half; i < half; ++i, ++j)
		out << "    " << e.pairs[i] << "  |  " << e.pairs[j] << '\n';
	if (half * 2 < n)
		out << "    " << e.pairs.back() << '\n';
	return out.str();
}

} // namespace

std::string
vma_info(const void *ptr)
{
	auto target = reinterpret_cast<unsigned long>(ptr);
	return format(find_entry(target));
}
