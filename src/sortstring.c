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

#define _GNU_SOURCE

#include "timing.h"
#include "vmainfo.h"
#include "routines.h"
#include "cpus_allowed.h"
#include "util/sdt.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static struct {
	const struct routine *r;
	char *write_filename;
	unsigned suffixsorting    : 1;
	unsigned check_result     : 1;
	unsigned oprofile         : 1;
	unsigned write            : 1;
	unsigned xml_stats        : 1;
	unsigned hugetlb_text     : 1;
	unsigned hugetlb_pointers : 1;
	unsigned text_raw         : 1;
	int perf_control_fd;
} opts;

static FILE *log_file;

static void
open_log_file(void)
{
	char *log_fn = NULL;
	char *host = NULL;
	if (log_file)
		return;
	host = getenv("HOSTNAME");
	if (host && *host)
		if (asprintf(&log_fn, "sortstring_log_%s", host) == -1)
			log_fn = NULL;
	if (log_fn)
		log_file = fopen(log_fn, "a");
	else
		log_file = fopen("sortstring_log", "a");
	free(log_fn);
	setlinebuf(log_file);
}

static void
perf_control_write(int fd, const char *msg)
{
	ssize_t len = (ssize_t)strlen(msg);
	ssize_t ret = write(fd, msg, len);
	if (ret != len) {
		int err = errno;
		fprintf(stderr,
			"ERROR: perf control fd write %s failed (ret=%zd, errno=%d): %s\n",
			msg, ret, err, strerror(err));
		if (log_file)
			fprintf(log_file,
				"FATAL: perf control fd write %s failed (ret=%zd, errno=%d): %s\n",
				msg, ret, err, strerror(err));
		exit(1);
	}
}

static void
perf_control_enable(int fd)
{
	perf_control_write(fd, "enable\n");
}

static void
perf_control_disable(int fd)
{
	perf_control_write(fd, "disable\n");
}

static void
opcontrol_start(void)
{
	int ret = system("opcontrol --start");
	if (ret == -1 || WIFEXITED(ret) == 0 || WEXITSTATUS(ret) != 0) {
		fprintf(stderr, "ERROR: opcontrol --start failed.\n");
		if (log_file)
			fprintf(log_file,
		             "FATAL: opcontrol --start failed. "
			     "ret=%d, WIFEXITED=%d, WEXITSTATUS=%d\n",
			     ret, WIFEXITED(ret), WEXITSTATUS(ret));
		exit(1);
	}
}

static void
opcontrol_stop(void)
{
	int ret = system("opcontrol --stop");
	if (ret == -1 || WIFEXITED(ret) == 0 || WEXITSTATUS(ret) != 0) {
		fprintf(stderr, "ERROR: opcontrol --stop failed.");
		if (log_file)
			fprintf(log_file,
		             "FATAL: opcontrol --stop failed. "
			     "ret=%d, WIFEXITED=%d, WEXITSTATUS=%d\n",
			     ret, WIFEXITED(ret), WEXITSTATUS(ret));
		exit(1);
	}
}

static char *
bazename(const char *fname)
{
	static char buf[300];
	strcpy(buf, fname);
	return basename(buf);
}

static void *
alloc_bytes(size_t bytes, int hugetlb)
{
	int map_flags;
	void *p;
	map_flags = MAP_ANONYMOUS | MAP_PRIVATE;
	if (hugetlb)
		map_flags |= MAP_HUGETLB;
	p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, map_flags, -1, 0);
	if (p == MAP_FAILED) {
		fprintf(stderr,
			"ERROR: unable to mmap memory for input: %s.\n",
			strerror(errno));
		exit(1);
	}
	return p;
}

static unsigned char *
alloc_text(size_t bytes)
{
	void *p = alloc_bytes(bytes, opts.hugetlb_text);
	return (unsigned char *)p;
}

static unsigned char **
alloc_pointers(size_t num)
{
	void *p = alloc_bytes(num*sizeof(unsigned char *),
			opts.hugetlb_pointers);
	return (unsigned char **)p;
}

static void
free_text(unsigned char *text, size_t text_len)
{
	munmap((void *)text, text_len);
}

static void
free_pointers(unsigned char **strings, size_t strings_len)
{
	munmap((void *)strings, strings_len);
}

static off_t
file_size(int fd)
{
	off_t size;
	size = lseek(fd, 0, SEEK_END);
	if (size == -1) {
		fprintf(stderr,
			"ERROR: unable to lseek() input file: %s.\n",
			strerror(errno));
		exit(1);
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		fprintf(stderr,
			"ERROR: unable to lseek() input file: %s.\n",
			strerror(errno));
		exit(1);
	}
	return size;
}

static void
input_copy(const char *fname, unsigned char **text_, size_t *text_len_)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	off_t filesize = file_size(fd);
	if (filesize <= 0) {
		fprintf(stderr,
			"ERROR: input file '%s' empty.\n",
			fname);
		exit(1);
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
			fprintf(stderr, "ERROR: failed read() "
				"from input file '%s': %s.\n",
				fname, strerror(errno));
			exit(1);
		} else if (ret == 0) {
			fprintf(stderr, "ERROR: EOF read() before reading "
				"whole input file '%s': %s.\n",
				fname, strerror(errno));
			exit(1);
		}
		i += ret;
	}
	if (close(fd) == -1) {
		fprintf(stderr,
			"ERROR: unable to close() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	*text_ = text;
	*text_len_ = filesize;
}

/* mmap() input data that is in raw format (uses NULL bytes for delimiting
 * strings). */
static void
input_mmap(const char *fname, unsigned char **text, size_t *text_len)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	off_t filesize = file_size(fd);
	if (filesize <= 0) {
		fprintf(stderr,
			"ERROR: input file '%s' empty.\n",
			fname);
		exit(1);
	}
	void *raw = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (raw == MAP_FAILED) {
		fprintf(stderr,
			"ERROR: unable to mmap input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	if (close(fd) == -1) {
		fprintf(stderr,
			"ERROR: unable to close() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	*text = (unsigned char *)raw;
	*text_len = filesize;
}

static void
readbytes(const char *fname, unsigned char **text, size_t *text_len)
{
	/* mapping file with MAP_HUGETLB does not work. */
	if (opts.text_raw && !opts.hugetlb_text)
		return input_mmap(fname, text, text_len);
	else
		return input_copy(fname, text, text_len);
}

static void
create_strings_delim(unsigned char *text, size_t text_len, int delim,
		unsigned char ***strings, size_t *strings_cnt)
{
	size_t strs_cnt = 0;
	for (size_t i=0; i < text_len; ++i)
		if (text[i] == delim)
			++strs_cnt;
	if (strs_cnt == 0) {
		fprintf(stderr,
			"ERROR: unable to read any lines from the input "
			"file.\n");
		exit(1);
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

static void
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

static void
create_suffixes(unsigned char *text, size_t text_len,
		unsigned char ***strings, size_t *strings_cnt)
{
	unsigned char **strs = alloc_pointers(text_len);
	for (size_t i=0; i < text_len; ++i)
		strs[i] = text+i;
	*strings = strs;
	*strings_cnt = text_len;
}

static int
strcmp_u(unsigned char *a, unsigned char *b)
{
	return strcmp((char *)a, (char *)b);
}

static void
write_result(unsigned char **strings, size_t n)
{
	FILE *fp;
	if (!opts.write_filename) {
		const char *username = getenv("USERNAME");
		if (!username)
			username = "";
		if (asprintf(&opts.write_filename, "/tmp/%s/alg.out", username) == -1)
			opts.write_filename = NULL;
	}
	fp = fopen(opts.write_filename, "w");
	if (!fp) {
		fprintf(stderr,
			"WARNING: --write failed: "
			"unable to open file for writing!\n");
		return;
	}
	for (size_t i=0; i < n; ++i) {
		fputs((const char *)strings[i], fp);
		fputc('\n', fp);
	}
	fclose(fp);
	fprintf(stderr, "Wrote sorted output to '%s'.\n",
			opts.write_filename);
	/*
	std::cout << "\n";
	system("md5sum " + opts.write_filename);
	system("cat    " + opts.input_filename + ".md5sum");
	std::cout << "\n";
	*/
}

static void
check_result(unsigned char **strings, size_t n)
{
	size_t wrong = 0;
	size_t identical = 0;
	size_t invalid = 0;
	for (size_t i=0; i < n-1; ++i) {
		if (strings[i] == strings[i+1])
			++identical;
		if (strings[i]==NULL || strings[i+1]==NULL)
			++invalid;
		else if (strcmp_u(strings[i], strings[i+1]) > 0)
			++wrong;
	}
	if (identical)
		fprintf(stderr,
			"WARNING: found %zu identical pointers!\n",
			identical);
	if (wrong)
		fprintf(stderr,
			"WARNING: found %zu incorrect orderings!\n",
			wrong);
	if (invalid)
		fprintf(stderr,
			"WARNING: found %zu invalid pointers!\n",
			invalid);
	if (!identical && !wrong && !invalid)
		fprintf(stderr, "Check: GOOD\n");
}

static void
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

static void
print_timing_results_human(void)
{
	printf("%10.2f ms : wall-clock\n", gettime_wall_clock());
	printf("%10.2f ms : user\n", gettime_user());
	printf("%10.2f ms : sys\n", gettime_sys());
	printf("%10.2f ms : user+sys\n", gettime_user_sys());
	printf("%10.2f ms : PROCESS_CPUTIME\n", gettime_process_cputime());
}

static void
print_timing_results(void)
{
	if (opts.xml_stats)
		print_timing_results_xml();
	else
		print_timing_results_human();
}

void
run(const struct routine *r, unsigned char **strings, size_t n)
{
	puts("Timing ...");
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
	if (opts.check_result)
		check_result(strings, n);
	if (opts.write)
		write_result(strings, n);
}

static int
routine_cmp(const void *a, const void *b)
{
	const struct routine *aa = *(const struct routine **)a;
	const struct routine *bb = *(const struct routine **)b;
	if (aa->f == bb->f)
		return 0;
	if (aa->multicore < bb->multicore)
		return -1;
	if (aa->multicore > bb->multicore)
		return 1;
	return strcmp(aa->name, bb->name);
}

static void
print_alg_names_and_descs(void)
{
	const struct routine **routines;
	unsigned routines_cnt;
	unsigned i = 0;
	routine_get_all(&routines, &routines_cnt);
	qsort(routines, routines_cnt, sizeof(struct routine *), routine_cmp);
	if (routines[0]->multicore == 0) {
		puts(":: SINGLE CORE ROUTINES ::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
		puts(":: NAME :::::::::::::::::::::: DESCRIPTION :::::::::::::::::::::::::::::::::::::");
		for (i=0; i < routines_cnt && routines[i]->multicore == 0; ++i)
			if (strlen(routines[i]->name) > 30) {
				printf("%s\n", routines[i]->name);
				printf("%30s %s\n", "", routines[i]->desc);
			} else
				printf("%-30s %s\n", routines[i]->name, routines[i]->desc);
	}
	if (i < routines_cnt && routines[i]->multicore) {
		if (i)
			puts("");
		puts(":: MULTI CORE ROUTINES :::::::::::::::::::::::::::::::::::::::::::::::::::::::::");
		puts(":: NAME :::::::::::::::::::::: DESCRIPTION :::::::::::::::::::::::::::::::::::::");
		for (; i < routines_cnt; ++i)
			if (strlen(routines[i]->name) > 30) {
				printf("%s\n", routines[i]->name);
				printf("%30s %s\n", "", routines[i]->desc);
			} else
				printf("%-30s %s\n", routines[i]->name, routines[i]->desc);
	}
}

static void
print_alg_names(void)
{
	const struct routine **routines;
	unsigned routines_cnt;
	unsigned i;
	routine_get_all(&routines, &routines_cnt);
	qsort(routines, routines_cnt, sizeof(struct routine *), routine_cmp);
	for (i=0; i < routines_cnt; ++i)
		puts(routines[i]->name);
}

static void
routine_information(const struct routine *r)
{
	printf("Routine (%s): %s\n",
			r->multicore ? "multi core" : "single core",
			r->name);
	printf("    \"%s\"\n", r->desc);
	printf("\n");
}

static void
input_information(unsigned char *text, size_t text_len,
		unsigned char **strings, size_t strings_len)
{
	size_t input_mb = text_len / (1024*1024);
	size_t input_kb = text_len / 1024;
	if (input_mb)
		printf("    size: %zu MB (%zu kB, %zu bytes)\n",
				input_mb, input_kb, text_len);
	else if (input_kb)
		printf("    size: %zu kB (%zu bytes)\n",
				input_kb, text_len);
	else
		printf("    size: %zu bytes\n", text_len);
	printf("    strings: %zu\n", strings_len);
	puts("");
	char *vma_info_text = vma_info(text);
	char *vma_info_strings = vma_info(strings);
	if (strcmp(vma_info_text, vma_info_strings) == 0) {
		puts("VMA information for text and string pointer arrays:");
		puts(vma_info_text);
	} else {
		puts("VMA information for text array:");
		puts(vma_info_text);
		puts("VMA information for string pointer array:");
		puts(vma_info_strings);
	}
	free(vma_info_text);
	free(vma_info_strings);
}

static void
cpu_information(void)
{
	int i;
	int maxcpu = -1;
	size_t cpus_setsize = 0;
	char *cpus_al = cpus_allowed_list();
	cpu_set_t *cpus = cpus_allowed(&cpus_setsize, &maxcpu);
	if (!cpus_al && !cpus)
		return;
	printf("CPU information:\n");
	if (cpus_al)
		printf("    CPUs allowed: %s\n", cpus_al);
	for (i=0; i < maxcpu; ++i) {
		if (CPU_ISSET_S(i, cpus_setsize, cpus)) {
			int min_freq, max_freq;
			printf("    CPU%d", i);
			min_freq = cpu_scaling_min_freq(i);
			max_freq = cpu_scaling_max_freq(i);
			if (min_freq != -1 && max_freq != -1)
				printf(", scaling frequencies: [%dMHz .. %dMHz]",
						max_freq/1000, min_freq/1000);
			puts("");
		}
	}
	putchar('\n');
	free(cpus_al);
	free(cpus);
}

static void
usage(void)
{
	puts(
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

static void
print_cmdline(int argc, char **argv, FILE *fp)
{
	int i;
	fprintf(fp, "Command line:");
	for (i=0; i < argc; ++i)
		fprintf(fp, " %s", argv[i]);
	fprintf(fp, "\n");
}

int main(int argc, char **argv)
{
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
			opts.check_result = 1;
			break;
		case 1004:
			opts.suffixsorting = 1;
			break;
		case 1005:
			opts.write = 1;
			if (optarg)
				opts.write_filename = optarg;
			break;
		case 1007:
			opts.oprofile = 1;
			break;
		case 1008:
			opts.xml_stats = 1;
			break;
		case 1009:
			opts.hugetlb_text = 1;
			break;
		case 1010:
			opts.hugetlb_pointers = 1;
			break;
		case 1011:
			opts.text_raw = 1;
			break;
		case 1012:
			opts.perf_control_fd = atoi(optarg);
			break;
		case '?':
		default:
			break;
		}
	}
	if (argc - 2 != optind) {
		fprintf(stderr,
			"ERROR: wrong number of arguments.\n");
		return 1;
	}
	const char *algorithm = argv[optind];
	if (!algorithm || strlen(algorithm) == 0) {
		fprintf(stderr,
			"ERROR: please specify algorithm name.\n");
		return 1;
	}
	opts.r = routine_from_name(algorithm);
	if (!opts.r) {
		fprintf(stderr,
			"ERROR: no match found for algorithm '%s'!\n",
			algorithm);
		return 1;
	}
	const char *filename = argv[optind+1];
	if (!filename || strlen(filename) == 0) {
		fprintf(stderr,
			"ERROR: please specify input filename.\n");
		return 1;
	}
	open_log_file();
	if (log_file)
		fprintf(log_file, "===START===\n");
	print_cmdline(argc, argv, log_file);
	routine_information(opts.r);
	cpu_information();
	unsigned long seed = getpid()*time(0);
	//seed = 0xdeadbeef;
	srand48(seed);
	if (log_file)
		fprintf(log_file, "Random seed: %lu.\n", seed);
	printf("Input (%s): %s ...\n",
			opts.text_raw ? "RAW" : "plain",
			bazename(filename));
	unsigned char *text;
	unsigned char **strings;
	size_t text_len, strings_len;
	readbytes(filename, &text, &text_len);
	if (opts.suffixsorting) {
		if (log_file)
			fprintf(log_file, "Suffix sorting mode!\n");
		create_suffixes(text, text_len, &strings, &strings_len);
	} else {
		create_strings(text, text_len, &strings, &strings_len);
	}
	input_information(text, text_len, strings, strings_len);
	run(opts.r, strings, strings_len);
	free_text(text, text_len);
	free_pointers(strings, strings_len);
	if (log_file) {
		fprintf(log_file, "===DONE===\n");
		fclose(log_file);
	}
}
