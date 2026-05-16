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

#ifndef CPUS_ALLOWED_H
#define CPUS_ALLOWED_H

#include <sched.h>

#include <cstddef>
#include <optional>
#include <string>

struct cpus_info {
	cpu_set_t *set     = nullptr;
	size_t     setsize = 0;
	int        maxcpu  = 0;
};

struct cpu_info {
	enum class core_class {
		unknown,
		performance, // Intel P-core (cpu_core PMU)
		efficiency,  // Intel E-core (cpu_atom PMU)
	};
	std::optional<int> scaling_min_khz;
	std::optional<int> scaling_max_khz;
	std::optional<int> hw_max_khz; // amd_pstate_max_freq or cpuinfo_max_freq
	core_class         klass = core_class::unknown;
};

[[nodiscard]] std::string cpus_allowed_list();
[[nodiscard]] cpus_info cpus_allowed();
[[nodiscard]] cpu_info cpu_info_for(int cpu);
[[nodiscard]] std::optional<bool> cpu_boost_enabled();

#endif /* CPUS_ALLOWED_H */
