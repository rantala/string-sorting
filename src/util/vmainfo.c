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

#define _GNU_SOURCE
#include "vmainfo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
static void
add_smaps(char *buf, char **pairs, unsigned pairs_cnt)
{
	unsigned i, j;
	for (i=0, j=pairs_cnt/2; i < pairs_cnt/2; ++i, ++j) {
		pairs[i][strlen(pairs[i])-1] = '\0';
		strcat(buf, "    ");
		strcat(buf, pairs[i]);
		strcat(buf, "  |  ");
		strcat(buf, pairs[j]);
	}
	if (j < pairs_cnt) {
		strcat(buf, "    ");
		strcat(buf, pairs[j]);
	}
}

static void
free_pairs(char **pairs, unsigned pairs_cnt)
{
	unsigned i;
	for (i=0; i < pairs_cnt; ++i)
		free(pairs[i]);
	free(pairs);
}

char *
vma_info(void *ptr)
{
	FILE *fp = NULL;
	char *buf = NULL;
	char *line = NULL;
	char **pairs = NULL, **tmp = NULL;
	unsigned pairs_cnt = 0;
	size_t line_n = 0;
	buf = malloc(2048);
	if (!buf)
		goto done;
	buf[0] = 0;
	fp = fopen("/proc/self/smaps", "r");
	if (!fp)
		goto done;
	while (getline(&line, &line_n, fp) != -1) {
		unsigned long a, b;
		if (sscanf(line, "%lx-%lx", &a, &b) != 2)
			continue;
		if (a <= (unsigned long)ptr && (unsigned long)ptr < b) {
			/* OK, found it! */
			strcat(buf, "    ");
			strcat(buf, line);
			while (getline(&line, &line_n, fp) != -1) {
				if (line[0] >= 'A' && line[0] <= 'Z'
						&& strchr(line, ':') != NULL) {
					tmp = realloc(pairs, (pairs_cnt+1) * sizeof(char *));
					if (!tmp)
						goto done;
					pairs = tmp;
					pairs[pairs_cnt++] = line;
					line = NULL;
					line_n = 0;
				} else {
					free(line);
					goto done;
				}
			}
		}
	}
done:
	add_smaps(buf, pairs, pairs_cnt);
	free_pairs(pairs, pairs_cnt);
	if (fp)
		fclose(fp);
	return buf;
}
