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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char *
status_entry(const char *key)
{
	char *result = NULL;
	char *line = NULL;
	size_t line_n = 0;
	FILE *fp;
	fp = fopen("/proc/self/status", "r");
	if (!fp)
		goto done;
	while (getline(&line, &line_n, fp) != -1) {
		char *v;
		v = strchr(line, ':');
		if (!v || *v == '\0')
			continue;
		*v = '\0';
		if (strcmp(line, key) != 0)
			continue;
		++v;
		while (*v == ' ' || *v == '\t')
			++v;
		if (strlen(v) > 1)
			v[strlen(v)-1] = '\0';
		if (*v == '\0')
			goto done;
		result = line;
		while ((*line++ = *v++))
			;
		goto done;
	}
done:
	if (!result)
		free(line);
	if (fp)
		fclose(fp);
	return result;
}

char *
cpus_allowed_list(void)
{
	return status_entry("Cpus_allowed_list");
}

static int
ishexdigit(char ch)
{
	return (ch >= '0' && ch <= '9')
	    || (ch >= 'a' && ch <= 'f');
}

static int
hex2int(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	abort();
	return 0;
}

static int
high_bit_order(char *allowed)
{
	int order = -1;
	int i = 0;
	int k = strlen(allowed)-1;
	for (; k >= 0; --k) {
		char ch = allowed[k];
		if (!ishexdigit(ch))
			continue;
		int mask = hex2int(ch);
		if (mask) {
			int neworder;
			if (mask & 8)
				neworder = 4 + i;
			else if (mask & 4)
				neworder = 3 + i;
			else if (mask & 2)
				neworder = 2 + i;
			else
				neworder = 1 + i;
			if (neworder > order)
				order = neworder;
		}
		i += 4;
	}
	return order;
}

static void
set_cpu_bits(char *allowed, cpu_set_t *c, size_t setsize)
{
	int i = 0;
	int k = strlen(allowed)-1;
	for (; k >= 0; --k) {
		char ch = allowed[k];
		if (!ishexdigit(ch))
			continue;
		int mask = hex2int(ch);
		if (mask & 1) CPU_SET_S(i+0, setsize, c);
		if (mask & 2) CPU_SET_S(i+1, setsize, c);
		if (mask & 4) CPU_SET_S(i+2, setsize, c);
		if (mask & 8) CPU_SET_S(i+3, setsize, c);
		i += 4;
	}
}

cpu_set_t *
cpus_allowed(size_t *setsize, int *maxcpu)
{
	cpu_set_t *c = NULL;
	char *allowed = status_entry("Cpus_allowed");
	if (!allowed || strlen(allowed) == 0)
		goto done;
	*maxcpu = high_bit_order(allowed);
	if (*maxcpu == -1)
		goto done;
	c = CPU_ALLOC(*maxcpu+1);
	if (!c)
		goto done;
	*setsize = CPU_ALLOC_SIZE(*maxcpu+1);
	set_cpu_bits(allowed, c, *setsize);
done:
	free(allowed);
	return c;
}

int
cpu_scaling_min_freq(int cpu)
{
	int min_freq;
	FILE *fp;
	char *filename = NULL;
	if (asprintf(&filename,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
			cpu) == -1) {
		return -1;
	}
	fp = fopen(filename, "r");
	free(filename);
	if (!fp)
		return -1;
	if (fscanf(fp, "%d", &min_freq) != 1)
		min_freq = -1;
	fclose(fp);
	return min_freq;
}

int
cpu_scaling_max_freq(int cpu)
{
	int max_freq;
	FILE *fp;
	char *filename = NULL;
	if (asprintf(&filename,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
			cpu) == -1) {
		return -1;
	}
	fp = fopen(filename, "r");
	free(filename);
	if (!fp)
		return -1;
	if (fscanf(fp, "%d", &max_freq) != 1)
		max_freq = -1;
	fclose(fp);
	return max_freq;
}
