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
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

/* Read /proc/self/status, return the (whitespace-trimmed) value of the
 * requested key, or an empty string if not present. */
std::string
status_entry(std::string_view key)
{
	std::ifstream f("/proc/self/status");
	if (!f) return std::string();
	for (std::string line; std::getline(f, line); ) {
		auto colon = line.find(':');
		if (colon == std::string::npos) continue;
		if (std::string_view(line).substr(0, colon) != key) continue;
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
high_bit_order(std::string_view allowed)
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
set_cpu_bits(std::string_view allowed, cpu_set_t *c, size_t setsize)
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

std::optional<int>
read_int_file(const std::string &path)
{
	std::ifstream f(path);
	if (!f) return std::nullopt;
	int v;
	if (!(f >> v)) return std::nullopt;
	return v;
}

} // namespace

std::string
cpus_allowed_list()
{
	return status_entry("Cpus_allowed_list");
}

cpus_info
cpus_allowed()
{
	cpus_info info;
	std::string allowed = status_entry("Cpus_allowed");
	if (allowed.empty()) return info;
	int top = high_bit_order(allowed);
	if (top == -1) return info;
	cpu_set_t *c = CPU_ALLOC(top + 1);
	if (!c) return info;
	info.set     = c;
	info.setsize = CPU_ALLOC_SIZE(top + 1);
	info.maxcpu  = top;
	CPU_ZERO_S(info.setsize, info.set);
	set_cpu_bits(allowed, info.set, info.setsize);
	return info;
}

std::optional<int>
cpu_scaling_min_freq(int cpu)
{
	std::ostringstream path;
	path << "/sys/devices/system/cpu/cpu" << cpu << "/cpufreq/scaling_min_freq";
	return read_int_file(path.str());
}

std::optional<int>
cpu_scaling_max_freq(int cpu)
{
	std::ostringstream path;
	path << "/sys/devices/system/cpu/cpu" << cpu << "/cpufreq/scaling_max_freq";
	return read_int_file(path.str());
}
