/*
 * Copyright 2008 by Tommi Rantala <tt.rantala@gmail.com>
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

/* Give out memory from huge pages using a malloc() style API. There are two
 * calls:
 *
 *   void* hugetlb_alloc(size_t bytes)
 *   void hugetlb_dealloc(void* ptr, size_t bytes)
 *
 * Memory is allocated in multiples of huge page size, meaning that some memory
 * might be wasted due to alignment mismatch. The ''libhugetlbfs'' library can
 * be used to fix this problem, and it can also replace all calls to malloc() to
 * use such huge pages.
 *
 * Requires a recent kernel version[1], properly adjusted settings[2], and the
 * ''hugetlbfs'' file system must be mounted[3] and writable by the current
 * user.
 *
 * [1] apparently 2.6.16 or later
 * [2] see /proc/sys/vm/
 * [3] mount -t hugetlbfs nodev /somewhere/
 */

#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <fstream>
#include <mntent.h>
#include <cassert>
#include <sys/mman.h>
#include <cstdlib>
#include "debug.h"

#ifndef _PATH_MOUNTED
#define _PATH_MOUNTED "/etc/mtab"
#endif

static std::string
hugetlb_mountpoint()
{
	static std::string mountpoint;
	if (not mountpoint.empty()) { return mountpoint; }
	FILE* fp = setmntent(_PATH_MOUNTED, "r");
	if (fp == 0) {
		std::cerr << "ERROR: Failed to read " _PATH_MOUNTED << std::endl;
		exit(1);
	}
	while (mntent* entry = getmntent(fp)) {
		if (strcmp("hugetlbfs", entry->mnt_type) == 0) {
			mountpoint = entry->mnt_dir;
			debug() << "HugeTLB: found hugetlbfs mounted at "
			        << mountpoint << "\n";
			break;
		}
	}
	endmntent(fp);
	if (mountpoint.empty()) {
		std::cerr << "ERROR: hugetlbfs must be mounted to use "
		             "this feature" << std::endl;
		exit(1);
	}
	return mountpoint;
}

static size_t
hugetlb_pagesize()
{
	static size_t pagesize = 0;
	if (pagesize) { return pagesize; }
	std::ifstream meminfo("/proc/meminfo");
	if (not meminfo) {
		std::cerr << "ERROR: could not read /proc/meminfo" << std::endl;
		exit(1);
	}
	std::string line;
	while (std::getline(meminfo, line)) {
		if (line.find("Hugepagesize:") != std::string::npos) {
			std::string s;
			size_t val = 0;
			std::stringstream strm(line);
			strm >> s >> val >> s;
			if (s != "kB") {
				std::cerr << "ERROR: HugeTLB: failed to parse "
				             "/proc/meminfo" << std::endl;
				exit(1);
			}
			pagesize = val * 1024;
			debug() << "HugeTLB: pagesize is " << val << " kB\n";
			return pagesize;
		}
	}
	std::cerr << "ERROR: could not parse ''Hugepagesize'' from /proc/meminfo"
	          << std::endl;
	exit(1);
	return 0;
}

static size_t
hugetlb_align_to_pagesize(size_t bytes)
{
	const size_t pagesize = hugetlb_pagesize();
	if (bytes % pagesize) { bytes += (pagesize - bytes % pagesize); }
	return bytes;
}

void* hugetlb_alloc(size_t bytes)
{
	debug() << "HugeTLB: about to allocate " << bytes / 1024 << " kB = "
	        << bytes / 1048576 << " MB\n";
	const char t[] = "/sortstring.XXXXXX";
	char* tmpname = strdup((hugetlb_mountpoint()+t).c_str());
	int fd = mkstemp(tmpname);
	if (fd == -1) {
		std::cerr << "ERROR: HugeTLB: failed to create temporary file"
		          << std::endl;
		exit(1);
	}
	size_t bytes_aligned = hugetlb_align_to_pagesize(bytes);
	assert(bytes_aligned >= bytes);
	if (bytes != bytes_aligned) {
		debug() << "HugeTLB: wasting " << (bytes_aligned-bytes) / 1024
		        << " kB due to alignment mismatch." << std::endl;
	}
	void* raw = mmap(0, bytes_aligned, PROT_READ|PROT_WRITE,
	                 MAP_SHARED, fd, 0);
	if (raw == MAP_FAILED) {
		std::cerr << "ERROR: HugeTLB: memory mapping failed" << std::endl;
		exit(1);
	}
	if (unlink(tmpname) != 0) {
		std::cerr << "ERROR: HugeTLB: could not unlink temporary file"
		          << std::endl;
		exit(1);
	}
	free(tmpname);
	return raw;
}

void hugetlb_dealloc(void* ptr, size_t len)
{
	assert(size_t(ptr) % hugetlb_pagesize() == 0);
	if (munmap(ptr, hugetlb_align_to_pagesize(len)) != 0) {
		std::cerr << "ERROR: Failed to unmap HugeTLB'd memory area" << std::endl;
		exit(1);
	}
}
