/*
 * Copyright 2007-2008,2011,2020 by Tommi Rantala <tt.rantala@gmail.com>
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

#include "timing.h"
#include "vmainfo.h"
#include "routines.h"
#include "cpus_allowed.h"
#include "util/debug.h"
#include "util/sdt.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct {
	const routine *r          = nullptr;
	std::string write_filename;
	bool suffixsorting    = false;
	bool check_result     = false;
	bool oprofile         = false;
	bool write            = false;
	bool xml_stats        = false;
	bool hugetlb_text     = false;
	bool hugetlb_pointers = false;
	bool text_raw         = false;
	int perf_control_fd   = 0;
} opts;

FILE *log_file;

void
open_log_file(void)
{
	if (log_file)
		return;
	std::string log_fn = "sortstring_log";
	if (const char *host = std::getenv("HOSTNAME"); host && *host) {
		log_fn += '_';
		log_fn += host;
	}
	log_file = std::fopen(log_fn.c_str(), "a");
	if (log_file)
		setlinebuf(log_file);
}

void
perf_control_write(int fd, const char *msg)
{
	ssize_t len = (ssize_t)std::strlen(msg);
	ssize_t ret = write(fd, msg, len);
	if (ret != len) {
		int err = errno;
		std::fprintf(stderr,
			"ERROR: perf control fd write %s failed (ret=%zd, errno=%d): %s\n",
			msg, ret, err, std::strerror(err));
		if (log_file)
			std::fprintf(log_file,
				"FATAL: perf control fd write %s failed (ret=%zd, errno=%d): %s\n",
				msg, ret, err, std::strerror(err));
		std::exit(1);
	}
}

void
perf_control_enable(int fd)
{
	perf_control_write(fd, "enable\n");
}

void
perf_control_disable(int fd)
{
	perf_control_write(fd, "disable\n");
}

void
opcontrol_start(void)
{
	int ret = std::system("opcontrol --start");
	if (ret == -1 || WIFEXITED(ret) == 0 || WEXITSTATUS(ret) != 0) {
		std::fprintf(stderr, "ERROR: opcontrol --start failed.\n");
		if (log_file)
			std::fprintf(log_file,
		             "FATAL: opcontrol --start failed. "
			     "ret=%d, WIFEXITED=%d, WEXITSTATUS=%d\n",
			     ret, WIFEXITED(ret), WEXITSTATUS(ret));
		std::exit(1);
	}
}

void
opcontrol_stop(void)
{
	int ret = std::system("opcontrol --stop");
	if (ret == -1 || WIFEXITED(ret) == 0 || WEXITSTATUS(ret) != 0) {
		std::fprintf(stderr, "ERROR: opcontrol --stop failed.");
		if (log_file)
			std::fprintf(log_file,
		             "FATAL: opcontrol --stop failed. "
			     "ret=%d, WIFEXITED=%d, WEXITSTATUS=%d\n",
			     ret, WIFEXITED(ret), WEXITSTATUS(ret));
		std::exit(1);
	}
}

std::string
bazename(std::string_view fname)
{
	std::string copy(fname);
	return basename(copy.data());
}

[[nodiscard]] void *
alloc_bytes(size_t bytes, int hugetlb)
{
	int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;
	if (hugetlb)
		map_flags |= MAP_HUGETLB;
	void *p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, map_flags, -1, 0);
	if (p == MAP_FAILED) {
		std::fprintf(stderr,
			"ERROR: unable to mmap memory for input: %s.\n",
			std::strerror(errno));
		std::exit(1);
	}
	return p;
}

[[nodiscard]] unsigned char *
alloc_text(size_t bytes)
{
	return static_cast<unsigned char *>(alloc_bytes(bytes, opts.hugetlb_text));
}

[[nodiscard]] unsigned char **
alloc_pointers(size_t num)
{
	return static_cast<unsigned char **>(
		alloc_bytes(num * sizeof(unsigned char *), opts.hugetlb_pointers));
}

void
free_text(unsigned char *text, size_t text_len)
{
	munmap(text, text_len);
}

void
free_pointers(unsigned char **strings, size_t strings_len)
{
	munmap(strings, strings_len);
}

[[nodiscard]] off_t
file_size(int fd)
{
	off_t size = lseek(fd, 0, SEEK_END);
	if (size == -1) {
		std::fprintf(stderr,
			"ERROR: unable to lseek() input file: %s.\n",
			std::strerror(errno));
		std::exit(1);
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		std::fprintf(stderr,
			"ERROR: unable to lseek() input file: %s.\n",
			std::strerror(errno));
		std::exit(1);
	}
	return size;
}

void
input_copy(const char *fname, unsigned char **text_, size_t *text_len_)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		std::fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, std::strerror(errno));
		std::exit(1);
	}
	off_t filesize = file_size(fd);
	if (filesize <= 0) {
		std::fprintf(stderr,
			"ERROR: input file '%s' empty.\n",
			fname);
		std::exit(1);
	}
	unsigned char *text = alloc_text(filesize);
	const size_t block_size = 128*1024;
	for (size_t i=0; i < (size_t)filesize; ) {
		size_t r = (size_t)filesize - i;
		if (r > block_size)
			r = block_size;
		ssize_t ret = read(fd, &text[i], r);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			std::fprintf(stderr, "ERROR: failed read() "
				"from input file '%s': %s.\n",
				fname, std::strerror(errno));
			std::exit(1);
		} else if (ret == 0) {
			std::fprintf(stderr, "ERROR: EOF read() before reading "
				"whole input file '%s': %s.\n",
				fname, std::strerror(errno));
			std::exit(1);
		}
		i += ret;
	}
	if (close(fd) == -1) {
		std::fprintf(stderr,
			"ERROR: unable to close() input file '%s': %s.\n",
			fname, std::strerror(errno));
		std::exit(1);
	}
	*text_ = text;
	*text_len_ = filesize;
}

/* mmap() input data that is in raw format (uses NULL bytes for delimiting
 * strings). */
void
input_mmap(const char *fname, unsigned char **text, size_t *text_len)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		std::fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, std::strerror(errno));
		std::exit(1);
	}
	off_t filesize = file_size(fd);
	if (filesize <= 0) {
		std::fprintf(stderr,
			"ERROR: input file '%s' empty.\n",
			fname);
		std::exit(1);
	}
	void *raw = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (raw == MAP_FAILED) {
		std::fprintf(stderr,
			"ERROR: unable to mmap input file '%s': %s.\n",
			fname, std::strerror(errno));
		std::exit(1);
	}
	if (close(fd) == -1) {
		std::fprintf(stderr,
			"ERROR: unable to close() input file '%s': %s.\n",
			fname, std::strerror(errno));
		std::exit(1);
	}
	*text = static_cast<unsigned char *>(raw);
	*text_len = filesize;
}

void
readbytes(const char *fname, unsigned char **text, size_t *text_len)
{
	/* mapping file with MAP_HUGETLB does not work. */
	if (opts.text_raw && !opts.hugetlb_text)
		return input_mmap(fname, text, text_len);
	else
		return input_copy(fname, text, text_len);
}

void
create_strings_delim(unsigned char *text, size_t text_len, int delim,
		unsigned char ***strings, size_t *strings_cnt)
{
	size_t strs_cnt = 0;
	for (size_t i=0; i < text_len; ++i)
		if (text[i] == delim)
			++strs_cnt;
	if (strs_cnt == 0) {
		std::fprintf(stderr,
			"ERROR: unable to read any lines from the input "
			"file.\n");
		std::exit(1);
	}
	unsigned char **strs = alloc_pointers(strs_cnt);
	unsigned char *line_start = text;
	for (size_t j=0, i=0; i < text_len; ++i)
		if (text[i] == delim) {
			strs[j++] = line_start;
			line_start = text + i + 1;
			if (delim != '\0')
				text[i] = '\0';
		}
	*strings = strs;
	*strings_cnt = strs_cnt;
}

void
create_strings(unsigned char *text, size_t text_len,
		unsigned char ***strings, size_t *strings_cnt)
{
	if (opts.text_raw)
		return create_strings_delim(text, text_len, '\0',
				strings, strings_cnt);
	else
		return create_strings_delim(text, text_len, '\n',
				strings, strings_cnt);
}

void
create_suffixes(unsigned char *text, size_t text_len,
		unsigned char ***strings, size_t *strings_cnt)
{
	unsigned char **strs = alloc_pointers(text_len);
	for (size_t i=0; i < text_len; ++i)
		strs[i] = text+i;
	*strings = strs;
	*strings_cnt = text_len;
}

void
write_result(unsigned char **strings, size_t n)
{
	if (opts.write_filename.empty()) {
		const char *username = std::getenv("USERNAME");
		if (!username)
			username = "";
		opts.write_filename = "/tmp/";
		opts.write_filename += username;
		opts.write_filename += "/alg.out";
	}
	FILE *fp = std::fopen(opts.write_filename.c_str(), "w");
	if (!fp) {
		std::fprintf(stderr,
			"WARNING: --write failed: "
			"unable to open file for writing!\n");
		return;
	}
	for (size_t i=0; i < n; ++i) {
		std::fputs(reinterpret_cast<const char *>(strings[i]), fp);
		std::fputc('\n', fp);
	}
	std::fclose(fp);
	std::fprintf(stderr, "Wrote sorted output to '%s'.\n",
			opts.write_filename.c_str());
}

void
print_timing_results_xml(void)
{
	/*
	"<event>\n"
	"  <algorithm num=\""<<algnum<<"\" name=\""<<algname<<"\"/>\n"
	"  <input fullpath=\""<<filename<<"\" n=\""<<n<<"\"/>\n"
	"  <time seconds=\""<<time<<"\"/>\n"
	"</event>";
	*/
}

void
print_timing_results_human(void)
{
	std::printf("%10.2f ms : wall-clock\n", gettime_wall_clock());
	std::printf("%10.2f ms : user\n", gettime_user());
	std::printf("%10.2f ms : sys\n", gettime_sys());
	std::printf("%10.2f ms : user+sys\n", gettime_user_sys());
	std::printf("%10.2f ms : PROCESS_CPUTIME\n", gettime_process_cputime());
}

void
print_timing_results(void)
{
	if (opts.xml_stats)
		print_timing_results_xml();
	else
		print_timing_results_human();
}

int
run(const routine *r, unsigned char **strings, size_t n)
{
	int ret = 0;
	std::puts("Timing ...");
	if (opts.oprofile)
		opcontrol_start();
	if (opts.perf_control_fd > 0)
		perf_control_enable(opts.perf_control_fd);
	STAP_PROBE2(sortstring, routine_start, r->name, n);
	timing_start();
	r->f(strings, n);
	timing_stop();
	STAP_PROBE2(sortstring, routine_done, r->name, n);
	if (opts.oprofile)
		opcontrol_stop();
	if (opts.perf_control_fd > 0)
		perf_control_disable(opts.perf_control_fd);
	print_timing_results();
	if (opts.check_result) {
		ret = check_result(strings, n);
		if (ret == 0)
			std::fprintf(stderr, "Check: GOOD\n");
	}
	if (opts.write)
		write_result(strings, n);
	return ret;
}

void
print_alg_names_and_descs(void)
{
	auto [routines, routines_cnt] = routine_get_all();
	unsigned i = 0;
	if (routines[0]->multicore == 0) {
		std::puts(":: SINGLE CORE ROUTINES ::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
		std::puts(":: NAME :::::::::::::::::::::: DESCRIPTION :::::::::::::::::::::::::::::::::::::");
		for (i=0; i < routines_cnt && routines[i]->multicore == 0; ++i)
			if (std::strlen(routines[i]->name) > 30) {
				std::printf("%s\n", routines[i]->name);
				std::printf("%30s %s\n", "", routines[i]->desc);
			} else
				std::printf("%-30s %s\n", routines[i]->name, routines[i]->desc);
	}
	if (i < routines_cnt && routines[i]->multicore) {
		if (i)
			std::puts("");
		std::puts(":: MULTI CORE ROUTINES :::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
		std::puts(":: NAME :::::::::::::::::::::: DESCRIPTION :::::::::::::::::::::::::::::::::::::");
		for (; i < routines_cnt; ++i)
			if (std::strlen(routines[i]->name) > 30) {
				std::printf("%s\n", routines[i]->name);
				std::printf("%30s %s\n", "", routines[i]->desc);
			} else
				std::printf("%-30s %s\n", routines[i]->name, routines[i]->desc);
	}
}

void
print_alg_names(void)
{
	auto [routines, routines_cnt] = routine_get_all();
	for (unsigned i=0; i < routines_cnt; ++i)
		std::puts(routines[i]->name);
}

void
routine_information(const routine *r)
{
	std::printf("Routine (%s): %s\n",
			r->multicore ? "multi core" : "single core",
			r->name);
	std::printf("    \"%s\"\n", r->desc);
	std::printf("\n");
}

void
input_information(unsigned char *text, size_t text_len,
		unsigned char **strings, size_t strings_len)
{
	size_t input_mb = text_len / (1024*1024);
	size_t input_kb = text_len / 1024;
	if (input_mb)
		std::printf("    size: %zu MB (%zu kB, %zu bytes)\n",
				input_mb, input_kb, text_len);
	else if (input_kb)
		std::printf("    size: %zu kB (%zu bytes)\n",
				input_kb, text_len);
	else
		std::printf("    size: %zu bytes\n", text_len);
	std::printf("    strings: %zu\n", strings_len);
	std::puts("");
	std::string vma_info_text    = vma_info(text);
	std::string vma_info_strings = vma_info(strings);
	if (vma_info_text == vma_info_strings) {
		std::puts("VMA information for text and string pointer arrays:");
		std::puts(vma_info_text.c_str());
	} else {
		std::puts("VMA information for text array:");
		std::puts(vma_info_text.c_str());
		std::puts("VMA information for string pointer array:");
		std::puts(vma_info_strings.c_str());
	}
}

void
cpu_information(void)
{
	std::string cpus_al = cpus_allowed_list();
	auto [cpus, cpus_setsize, maxcpu] = cpus_allowed();
	if (cpus_al.empty() && !cpus)
		return;
	std::printf("CPU information:\n");
	if (!cpus_al.empty())
		std::printf("    CPUs allowed: %s\n", cpus_al.c_str());
	if (auto boost = cpu_boost_enabled())
		std::printf("    Turbo/boost: %s\n", *boost ? "on" : "off");
	for (int i=0; i < maxcpu; ++i) {
		if (!CPU_ISSET_S(i, cpus_setsize, cpus))
			continue;
		const cpu_info info = cpu_info_for(i);
		std::printf("    CPU%d", i);
		switch (info.klass) {
		case cpu_info::core_class::performance: std::printf(" (P-core)"); break;
		case cpu_info::core_class::efficiency:  std::printf(" (E-core)"); break;
		case cpu_info::core_class::unknown: break;
		}
		if (info.scaling_min_khz && info.scaling_max_khz)
			std::printf(", scaling [%dMHz .. %dMHz]",
					*info.scaling_min_khz/1000,
					*info.scaling_max_khz/1000);
		if (info.hw_max_khz && info.scaling_max_khz &&
		    *info.hw_max_khz > *info.scaling_max_khz)
			std::printf(", hw peak %dMHz", *info.hw_max_khz/1000);
		std::puts("");
	}
	std::putchar('\n');
	if (cpus)
		CPU_FREE(cpus);
}

void
usage(void)
{
	std::puts(
	     "String sorting\n"
	     "--------------\n"
	     "\n"
	     "Usage: ./sortstring [options] <algorithm> <filename>\n"
	     "\n"
	     "Options:\n"
	     "   --check          : Tries to check output for validity. Might not catch\n"
	     "                      all errors. Prints a warning when errors found.\n"
	     "   --perf-ctrl-fd=FD  Use file descriptor to control perf tool.\n"
	     "                      Enable perf just before sorting algorithm is called,\n"
	     "                      and disable after returning from the call.\n"
	     "                      See perf --control option.\n"
	     "   --oprofile       : Executes `oprofile --start' just before calling the\n"
	     "                      actual sorting algorithm, and `oprofile --stop' after\n"
	     "                      returning from the call. Can be used to obtain more\n"
	     "                      accurate statistics with OProfile.\n"
	     "   -A,--algs        : Prints available algorithm names and descriptions.\n"
	     "   -L,--alg-names   : Prints available algorithm names, useful for scripts.\n"
	     "                      Example:\n"
	     "                         for N in `./sortstring -L` ; do\n"
	     "                                   ./sortstring $N input ; done\n"
	     "   --suffix-sorting : Treat input as text, and sort each suffix of the text.\n"
	     "                      Can be _very_ slow.\n"
	     "   --write          : Writes sorted output to `/tmp/$USERNAME/alg.out'\n"
	     "   --write=outfile  : Writes sorted output to `outfile'\n"
	     "   --xml-stats      : Outputs statistics in XML (default: human readable)\n"
	     "   --hugetlb-text   : Place the input text into huge pages.\n"
	     "   --hugetlb-ptrs   : Place the string pointer array into huge pages.\n"
	     "                      HugeTLB requires kernel and hardware support.\n"
	     "   --raw            : The input file is in raw format: strings are delimited\n"
	     "                      with NULL bytes instead of newlines.\n"
	     "\n"
	     "Examples:\n"
	     "   # Get list of what is available:\n"
	     "   ./sortstring -A\n"
	     "\n"
	     "   # Sort input file with quicksort:\n"
	     "   ./sortstring quicksort ~/testdata/testfile1\n"
	     "\n"
	     "   # Sort all suffixes of of the given text file with quicksort:\n"
	     "   ./sortstring --check --suffix-sorting quicksort ~/testdata/text\n"
	     "\n"
	     "   # Perf tool and control file descriptor:\n"
	     "   mkfifo ctrl && exec 9<>ctrl && rm ctrl"
	         " && perf stat --delay=-1 --control=fd:9"
		 " -- taskset -c 0 ./sortstring --perf-ctrl-fd=9 quicksort testfile"
	     "\n");
}

void
print_cmdline(int argc, char **argv, FILE *fp)
{
	if (!fp)
		return;
	std::fprintf(fp, "Command line:");
	for (int i=0; i < argc; ++i)
		std::fprintf(fp, " %s", argv[i]);
	std::fprintf(fp, "\n");
}

} // namespace

int main(int argc, char **argv)
{
	int ret = 0;
	if (argc < 2) {
		usage();
		return 1;
	}
	static const struct option long_options[] = {
		{"help",           0, 0, 1000},
		{"algs",           0, 0, 'A' },
		{"alg-names",      0, 0, 'L' },
		{"check",          0, 0, 1003},
		{"suffix-sorting", 0, 0, 1004},
		{"write",          2, 0, 1005},
		{"oprofile",       0, 0, 1007},
		{"xml-stats",      0, 0, 1008},
		{"hugetlb-text",   0, 0, 1009},
		{"hugetlb-ptrs",   0, 0, 1010},
		{"raw",            0, 0, 1011},
		{"perf-ctrl-fd",   1, 0, 1012},
		{0,                0, 0, 0}
	};
	while (1) {
		int c = getopt_long(argc, argv, "hAL", long_options, 0);
		if (c == -1) break;
		switch (c) {
		case 'h':
		case 1000:
			usage();
			return 0;
		case 'A':
			print_alg_names_and_descs();
			return 0;
		case 'L':
			print_alg_names();
			return 0;
		case 1003:
			opts.check_result = true;
			break;
		case 1004:
			opts.suffixsorting = true;
			break;
		case 1005:
			opts.write = true;
			if (optarg)
				opts.write_filename = optarg;
			break;
		case 1007:
			opts.oprofile = true;
			break;
		case 1008:
			opts.xml_stats = true;
			break;
		case 1009:
			opts.hugetlb_text = true;
			break;
		case 1010:
			opts.hugetlb_pointers = true;
			break;
		case 1011:
			opts.text_raw = true;
			break;
		case 1012:
			opts.perf_control_fd = std::atoi(optarg);
			break;
		case '?':
		default:
			break;
		}
	}
	if (argc - 2 != optind) {
		std::fprintf(stderr,
			"ERROR: wrong number of arguments.\n");
		return 1;
	}
	const char *algorithm = argv[optind];
	if (!algorithm || std::strlen(algorithm) == 0) {
		std::fprintf(stderr,
			"ERROR: please specify algorithm name.\n");
		return 1;
	}
	if (auto r = routine_from_name(algorithm); r) {
		opts.r = *r;
	} else {
		std::fprintf(stderr,
			"ERROR: no match found for algorithm '%s'!\n",
			algorithm);
		return 1;
	}
	const char *filename = argv[optind+1];
	if (!filename || std::strlen(filename) == 0) {
		std::fprintf(stderr,
			"ERROR: please specify input filename.\n");
		return 1;
	}
	open_log_file();
	if (log_file)
		std::fprintf(log_file, "===START===\n");
	print_cmdline(argc, argv, log_file);
	routine_information(opts.r);
	cpu_information();
	unsigned long seed = getpid() * std::time(nullptr);
	//seed = 0xdeadbeef;
	srand48(seed);
	if (log_file)
		std::fprintf(log_file, "Random seed: %lu.\n", seed);
	std::printf("Input (%s): %s ...\n",
			opts.text_raw ? "RAW" : "plain",
			bazename(filename).c_str());
	unsigned char *text;
	unsigned char **strings;
	size_t text_len, strings_len;
	readbytes(filename, &text, &text_len);
	if (opts.suffixsorting) {
		if (log_file)
			std::fprintf(log_file, "Suffix sorting mode!\n");
		create_suffixes(text, text_len, &strings, &strings_len);
	} else {
		create_strings(text, text_len, &strings, &strings_len);
	}
	input_information(text, text_len, strings, strings_len);
	ret = run(opts.r, strings, strings_len);
	free_text(text, text_len);
	free_pointers(strings, strings_len);
	if (log_file) {
		std::fprintf(log_file, "===DONE===\n");
		std::fclose(log_file);
	}
	return ret;
}
