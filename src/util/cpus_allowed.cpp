/*
 * Copyright 2012 by Tommi Rantala <tt.rantala@gmail.com>
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

#include "cpus_allowed.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

char *
strdup_malloc(const std::string &s)
{
	char *p = static_cast<char *>(std::malloc(s.size() + 1));
	if (!p) std::abort();
	std::memcpy(p, s.c_str(), s.size() + 1);
	return p;
}

/* Read /proc/self/status, return the (whitespace-trimmed) value of the
 * requested key, or an empty string if not present. */
std::string
status_entry(const std::string &key)
{
	std::ifstream f("/proc/self/status");
	if (!f) return std::string();
	for (std::string line; std::getline(f, line); ) {
		auto colon = line.find(':');
		if (colon == std::string::npos) continue;
		if (line.compare(0, colon, key) != 0) continue;
		auto v = colon + 1;
		while (v < line.size() && (line[v] == ' ' || line[v] == '\t'))
			++v;
		auto end = line.size();
		while (end > v && std::isspace(static_cast<unsigned char>(line[end-1])))
			--end;
		return line.substr(v, end - v);
	}
	return std::string();
}

bool
ishex(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

int
hex_val(char ch)
{
	return (ch >= '0' && ch <= '9') ? ch - '0' : ch - 'a' + 10;
}

/* Highest bit index set in the hex bitmask string `allowed` (low-order
 * digits on the right). Non-hex characters (e.g. comma separators) are
 * skipped. Returns -1 if no bits are set. */
int
high_bit_order(const std::string &allowed)
{
	int order = -1;
	int i = 0;
	for (auto it = allowed.rbegin(); it != allowed.rend(); ++it) {
		char ch = *it;
		if (!ishex(ch)) continue;
		int mask = hex_val(ch);
		if (mask) {
			int neworder;
			if      (mask & 8) neworder = 4 + i;
			else if (mask & 4) neworder = 3 + i;
			else if (mask & 2) neworder = 2 + i;
			else               neworder = 1 + i;
			if (neworder > order) order = neworder;
		}
		i += 4;
	}
	return order;
}

void
set_cpu_bits(const std::string &allowed, cpu_set_t *c, size_t setsize)
{
	int i = 0;
	for (auto it = allowed.rbegin(); it != allowed.rend(); ++it) {
		char ch = *it;
		if (!ishex(ch)) continue;
		int mask = hex_val(ch);
		if (mask & 1) CPU_SET_S(i+0, setsize, c);
		if (mask & 2) CPU_SET_S(i+1, setsize, c);
		if (mask & 4) CPU_SET_S(i+2, setsize, c);
		if (mask & 8) CPU_SET_S(i+3, setsize, c);
		i += 4;
	}
}

int
read_int_file(const std::string &path)
{
	std::ifstream f(path);
	if (!f) return -1;
	int v;
	if (!(f >> v)) return -1;
	return v;
}

} // namespace

char *
cpus_allowed_list(void)
{
	std::string s = status_entry("Cpus_allowed_list");
	if (s.empty()) return nullptr;
	return strdup_malloc(s);
}

cpu_set_t *
cpus_allowed(size_t *setsize, int *maxcpu)
{
	std::string allowed = status_entry("Cpus_allowed");
	if (allowed.empty()) return nullptr;
	int top = high_bit_order(allowed);
	if (top == -1) return nullptr;
	*maxcpu = top;
	cpu_set_t *c = CPU_ALLOC(top + 1);
	if (!c) return nullptr;
	*setsize = CPU_ALLOC_SIZE(top + 1);
	CPU_ZERO_S(*setsize, c);
	set_cpu_bits(allowed, c, *setsize);
	return c;
}

int
cpu_scaling_min_freq(int cpu)
{
	std::ostringstream path;
	path << "/sys/devices/system/cpu/cpu" << cpu << "/cpufreq/scaling_min_freq";
	return read_int_file(path.str());
}

int
cpu_scaling_max_freq(int cpu)
{
	std::ostringstream path;
	path << "/sys/devices/system/cpu/cpu" << cpu << "/cpufreq/scaling_max_freq";
	return read_int_file(path.str());
}
