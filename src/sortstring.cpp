/*
 * Copyright 2007-2008 by Tommi Rantala <tt.rantala@gmail.com>
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

#include "sortstring.h"
#include <libgen.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <cstdlib>

#include <boost/tuple/tuple.hpp>

static int
system(const std::string& cmd)
{
	return system(cmd.c_str());
}

// int -> (function pointer, algorithm description)
typedef std::map<int, std::pair<void (*)(unsigned char **, size_t), std::string> > Algorithms;

struct Options {
	int algorithm;
	bool suffixsorting;
	bool check_result;
	bool oprofile;
	bool write;
	bool xml_stats;
	bool hugetlb_text;
	bool hugetlb_pointers;
	bool text_raw;
	std::string write_filename;
};

static Options opts;

static std::ofstream* log_file = 0;
static std::ostream&
log()
{
	assert(log_file);
	time_t t = time(0);
	std::string tmp = ctime(&t);
	tmp = tmp.substr(0, tmp.size()-1);
	return *log_file << "[" << tmp << "] ";
}

static void
opcontrol_start()
{
	if (opts.oprofile) {
		int ret = system("opcontrol --start");
		if (ret == -1 or WIFEXITED(ret) == false or WEXITSTATUS(ret) != 0) {
			std::cerr << "FATAL: opcontrol --start failed." << std::endl;
			log()     << "FATAL: opcontrol --start failed. ret=" << ret
			          << ", WIFEXITED=" << WIFEXITED(ret)
			          << ", WEXITSTATUS=" << WEXITSTATUS(ret)
			          << std::endl;
			exit(1);
		}
	}
}

static void
opcontrol_stop()
{
	if (opts.oprofile) {
		int ret = system("opcontrol --stop");
		if (ret == -1 or WIFEXITED(ret) == false or WEXITSTATUS(ret) != 0) {
			std::cerr << "FATAL: opcontrol --stop failed." << std::endl;
			log()     << "FATAL: opcontrol --stop failed. ret=" << ret
			          << ", WIFEXITED=" << WIFEXITED(ret)
			          << ", WEXITSTATUS=" << WEXITSTATUS(ret)
			          << std::endl;
			exit(1);
		}
	}
}

static char*
bazename(const char* fname)
{
	static char buf[300];
	strcpy(buf, fname);
	return basename(buf);
}

static char*
bazename(const std::string& fname)
{
	return bazename(fname.c_str());
}

static void
log_perf(const std::string& msg)
{
	std::string fname = std::string(".log_");
	if (getenv("HOSTNAME")) {
		fname += getenv("HOSTNAME");
	}
	std::ofstream file(fname.c_str(), std::ios::out | std::ios::app);
	if (!file) {
		std::cerr << "Warning: could not open file for logging!\n";
		return;
	}
	file << msg << std::endl;
}

static void*
alloc_bytes(size_t bytes)
{
	int map_flags;
	void* p;
	map_flags = MAP_ANONYMOUS | MAP_PRIVATE;
	if (opts.hugetlb_text)
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

static unsigned char*
alloc_text(size_t bytes)
{
	void* p = alloc_bytes(bytes);
	return (unsigned char*)p;
}

static unsigned char**
alloc_pointers(size_t num)
{
	void* p = alloc_bytes(num*sizeof(unsigned char*));
	return (unsigned char**)p;
}

static void
free_text(unsigned char* text, size_t text_len)
{
	munmap((void*)text, text_len);
}

static void
free_pointers(unsigned char** strings, size_t strings_len)
{
	munmap((void*)strings, strings_len);
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

static boost::tuple<unsigned char*, size_t>
input_copy(const char* fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	off_t filesize = file_size(fd);
	unsigned char* text = alloc_text(filesize);
	ssize_t ret = read(fd, text, filesize);
	if (ret < filesize) {
		/* Not strictly a failure... */
		fprintf(stderr,
			"ERROR: read() only %lld bytes out of %lld "
			"from input file '%s': %s.\n",
			(long long)ret, (long long)filesize,
			fname, strerror(errno));
		exit(1);
	}
	if (close(fd) == -1) {
		fprintf(stderr,
			"ERROR: unable to close() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	return boost::make_tuple(text, filesize);
}

/* mmap() input data that is in raw format (uses NULL bytes for delimiting
 * strings). */
static boost::tuple<unsigned char*, size_t>
input_mmap(const char* fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr,
			"ERROR: unable to open() input file '%s': %s.\n",
			fname, strerror(errno));
		exit(1);
	}
	off_t filesize = file_size(fd);
	void* raw = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
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
	return boost::make_tuple((unsigned char*)raw, filesize);
}

static boost::tuple<unsigned char*, size_t>
readbytes(const std::string& fname)
{
	/* mapping file with MAP_HUGETLB does not work. */
	if (opts.text_raw && !opts.hugetlb_text)
		return input_mmap(fname.c_str());
	else
		return input_copy(fname.c_str());
}

static boost::tuple<unsigned char**, size_t>
create_strings(unsigned char* text, size_t text_len, int delim)
{
	size_t strs = 0;
	for (size_t i=0; i < text_len; ++i)
		if (text[i] == delim)
			++strs;
	unsigned char** strings = alloc_pointers(strs);
	unsigned char* line_start = text;
	for (size_t j=0, i=0; i < text_len; ++i)
		if (text[i] == delim) {
			strings[j++] = line_start;
			line_start = text + i + 1;
			if (delim != '\0')
				text[i] = '\0';
		}
	return boost::make_tuple(strings, strs);
}

static boost::tuple<unsigned char**, size_t>
create_strings(unsigned char* text, size_t text_len)
{
	if (opts.text_raw)
		return create_strings(text, text_len, '\0');
	else
		return create_strings(text, text_len, '\n');
}

static boost::tuple<unsigned char**, size_t>
create_suffixes(unsigned char* text, size_t text_len)
{
	unsigned char** strings = alloc_pointers(text_len);
	for (size_t i=0; i < text_len; ++i) { strings[i] = text+i; }
	return boost::make_tuple(strings, text_len);
}

static int
strcmp(unsigned char* a, unsigned char* b)
{
	return strcmp((char*)a, (char*)b);
}

static void
write_result(unsigned char** strings, size_t n)
{
	if (not opts.write) {
		return;
	}
	if (opts.write_filename.empty()) {
		const char* username = getenv("USERNAME");
		if (not username) username = "";
		opts.write_filename = std::string("/tmp/") + username + "/alg.out";
	}
	std::ofstream file(opts.write_filename.c_str());
	if (not file) {
		std::cerr << "WARNING: --write failed: could not open file!\n";
		return;
	}
	for (size_t i=0; i < n; ++i) {
		file << strings[i] << "\n";
	}
	file.close();
	std::cerr << "Wrote sorted output to " << opts.write_filename << "\n";
	/*
	std::cout << "\n";
	system("md5sum " + opts.write_filename);
	system("cat    " + opts.input_filename + ".md5sum");
	std::cout << "\n";
	*/
}

static void
check_result(unsigned char** strings, size_t n)
{
	if (not opts.check_result) {
		return;
	}
	size_t wrong = 0;
	size_t identical = 0;
	size_t invalid = 0;
	for (size_t i=0; i < n-1; ++i) {
		if (strings[i] == strings[i+1]) {
			++identical;
		}
		if (strings[i]==0 or strings[i+1]==0) {
			++invalid;
		} else if (strcmp(strings[i], strings[i+1]) > 0) {
			++wrong;
		}
	}
	if (identical) {
		std::cerr << "WARNING: found " << identical << " identical pointers!\n";
	}
	if (wrong) {
		std::cerr << "WARNING: found " << wrong << " incorrect orderings!\n";
	}
	if (invalid) {
		std::cerr << "WARNING: found " << invalid << " invalid pointers!\n";
	}
	if (not identical and not wrong and not invalid) {
		std::cerr << "Check: GOOD\n";
	}
}

static std::string human_readable_results(int algnum, const std::string& algname,
		size_t n, const std::string& filename, const std::string& time)
{
	using std::setw;
	using std::setfill;
	using std::left;
	std::stringstream strm;
	strm << setw(3)  << setfill('0') << algnum << setfill(' ') << " * "
	     << setw(50) << left << algname << " * "
	     << setw(15) << left << bazename(filename) << "*"
	     << " time: " << time;
	return strm.str();
}

static std::string xml_results(int algnum, const std::string& algname,
		size_t n, const std::string& filename, const std::string& time)
{
	std::stringstream strm;
	strm << "<event>\n"
	        "  <algorithm num=\""<<algnum<<"\" name=\""<<algname<<"\"/>\n"
	        "  <input fullpath=\""<<filename<<"\" n=\""<<n<<"\"/>\n"
	        "  <time seconds=\""<<time<<"\"/>\n"
	        "</event>";
	return strm.str();
}

static std::string time_str()
{
	std::stringstream strm;
	strm << std::fixed << std::setprecision(2) << 1000*gettime();
	return strm.str();
}

// extern "C" so that run() will be demangled in the binary -> we can use the
// function name with callgrind for example.
extern "C"
void
run(int algnum,
    const Algorithms& algs,
    unsigned char** strings,
    size_t n,
    const std::string& filename)
{
	Algorithms::mapped_type alg;
	std::string output_human, output_xml;
	if (algs.find(algnum) != algs.end())
		alg = algs.find(algnum)->second;
	if (alg.first) {
		opcontrol_start();
		clockon();
		alg.first(strings, n);
		clockoff();
		opcontrol_stop();
		check_result(strings, n);
		write_result(strings, n);
		output_human = human_readable_results(algnum, alg.second, n,
				filename, time_str());
		output_xml = xml_results(algnum, alg.second, n, filename,
				time_str());
	} else {
		output_human = human_readable_results(algnum, "NA", n,
				filename, "NA");
		output_xml = xml_results(algnum, "NA", n, filename, "NA");
	}
	log_perf(output_human);
	if (opts.xml_stats) {
		std::cout << output_xml << std::endl;
	} else {
		std::cout << output_human << std::endl;
	}
}

static Algorithms
get_algorithms()
{
	using std::make_pair;
	Algorithms algs;

#define ALG(index, function) algs[index] = make_pair(function, #function);

	algs[1]  = make_pair(quicksort,  "Quicksort (Bentley, McIlroy)");
	algs[2]  = make_pair(multikey2,  "Multi-Key-Quicksort (Bentley, Sedgewick)");
	algs[3]  = make_pair(mbmradix,   "MSD Radix Sort (McIlroy, Bostic, McIlroy)");
	algs[4]  = make_pair(MSDsort,    "MSD Radix Sort (Andersson, Nilsson)");
	algs[5]  = make_pair(arssort,    "Adaptive MSD Radix Sort (Andersson, Nilsson)");
	algs[6]  = make_pair(frssort1,   "Forward Radix Sort 8-bit (Andersson, Nilsson)");
	algs[7]  = make_pair(frssort,    "Forward Radix Sort 16-bit (Andersson, Nilsson)");
	algs[8]  = make_pair(burstsortL, "Burstsort List Based (Sinha, Zobel)");
	algs[9]  = make_pair(burstsortA, "Burstsort Array Based (Sinha, Zobel)");
	algs[10] = make_pair(CRadix,     "CRadix (Ng, Kakehi)");
	algs[11] = make_pair(cradix_improved, "CRadix (Ng, Kakehi) (Modified by T. Rantala)");

	algs[20] = make_pair(msd_CE0, "msd_CE0 (Baseline)");
	algs[21] = make_pair(msd_CE1, "msd_CE1 (Oracle)");
	algs[22] = make_pair(msd_CE2, "msd_CE2 (Oracle+Loop Fission)");
	algs[23] = make_pair(msd_CE3, "msd_CE3 (Oracle+Loop Fission+Adaptive)");

	algs[25] = make_pair(msd_ci,          "msd_CI");
	algs[26] = make_pair(msd_ci_adaptive, "msd_CI (Adaptive)");

	ALG(30, msd_D_std_vector)
	ALG(31, msd_D_std_vector_adaptive)
	ALG(32, msd_D_std_list)
	ALG(33, msd_D_std_list_adaptive)
	ALG(34, msd_D_std_deque)
	ALG(35, msd_D_std_deque_adaptive)

	ALG(38, msd_D_vector_realloc_shrink_clear)
	ALG(39, msd_D_vector_realloc_shrink_clear_adaptive)
	ALG(40, msd_D_vector_realloc)
	ALG(41, msd_D_vector_realloc_adaptive)
	ALG(42, msd_D_vector_malloc)
	ALG(43, msd_D_vector_malloc_adaptive)
	ALG(44, msd_D_vector_realloc_counter_clear)
	ALG(45, msd_D_vector_realloc_counter_clear_adaptive)
	ALG(46, msd_D_vector_malloc_counter_clear)
	ALG(47, msd_D_vector_malloc_counter_clear_adaptive)
	ALG(48, msd_D_vector_block)
	ALG(49, msd_D_vector_block_adaptive)
	ALG(50, msd_D_vector_brodnik)
	ALG(51, msd_D_vector_brodnik_adaptive)
	ALG(52, msd_D_vector_bagwell)
	ALG(53, msd_D_vector_bagwell_adaptive)

	ALG(54, msd_DB)
	ALG(55, msd_A)
	ALG(56, msd_A_adaptive)
	ALG(57, msd_A2)
	ALG(58, msd_A2_adaptive)

	ALG(60, multikey_simd1)
	ALG(61, multikey_simd2)
	ALG(62, multikey_simd4)
	ALG(63, multikey_dynamic_vector1)
	ALG(64, multikey_dynamic_vector2)
	ALG(65, multikey_dynamic_vector4)
	ALG(66, multikey_dynamic_brodnik1)
	ALG(67, multikey_dynamic_brodnik2)
	ALG(68, multikey_dynamic_brodnik4)
	ALG(69, multikey_dynamic_bagwell1)
	ALG(70, multikey_dynamic_bagwell2)
	ALG(71, multikey_dynamic_bagwell4)
	ALG(72, multikey_dynamic_vector_block1)
	ALG(73, multikey_dynamic_vector_block2)
	ALG(74, multikey_dynamic_vector_block4)
	ALG(75, multikey_block1)
	ALG(76, multikey_block2)
	ALG(77, multikey_block4)
	ALG(78, multikey_multipivot_brute_simd1)
	ALG(79, multikey_multipivot_brute_simd2)
	ALG(80, multikey_multipivot_brute_simd4)
	ALG(81, multikey_cache4)
	ALG(82, multikey_cache8)

	ALG(90, burstsort_mkq_simpleburst_1)
	ALG(91, burstsort_mkq_simpleburst_2)
	ALG(92, burstsort_mkq_simpleburst_4)
	ALG(95, burstsort_mkq_recursiveburst_1)
	ALG(96, burstsort_mkq_recursiveburst_2)
	ALG(97, burstsort_mkq_recursiveburst_4)

	ALG(100, burstsort_vector)
	ALG(101, burstsort_brodnik)
	ALG(102, burstsort_bagwell)
	ALG(103, burstsort_vector_block)
	ALG(104, burstsort_superalphabet_vector)
	ALG(105, burstsort_superalphabet_brodnik)
	ALG(106, burstsort_superalphabet_bagwell)
	ALG(107, burstsort_superalphabet_vector_block)
	ALG(110, burstsort_sampling_vector)
	ALG(111, burstsort_sampling_brodnik)
	ALG(112, burstsort_sampling_bagwell)
	ALG(113, burstsort_sampling_vector_block)
	ALG(114, burstsort_sampling_superalphabet_vector)
	ALG(115, burstsort_sampling_superalphabet_brodnik)
	ALG(116, burstsort_sampling_superalphabet_bagwell)
	ALG(117, burstsort_sampling_superalphabet_vector_block)

	ALG(120, burstsort2_vector)
	ALG(121, burstsort2_brodnik)
	ALG(122, burstsort2_bagwell)
	ALG(123, burstsort2_vector_block)
	ALG(124, burstsort2_superalphabet_vector)
	ALG(125, burstsort2_superalphabet_brodnik)
	ALG(126, burstsort2_superalphabet_bagwell)
	ALG(127, burstsort2_superalphabet_vector_block)
	ALG(130, burstsort2_sampling_vector)
	ALG(131, burstsort2_sampling_brodnik)
	ALG(132, burstsort2_sampling_bagwell)
	ALG(133, burstsort2_sampling_vector_block)
	ALG(134, burstsort2_sampling_superalphabet_vector)
	ALG(135, burstsort2_sampling_superalphabet_brodnik)
	ALG(136, burstsort2_sampling_superalphabet_bagwell)
	ALG(137, burstsort2_sampling_superalphabet_vector_block)

	ALG(140, msd_A_lsd4)
	ALG(141, msd_A_lsd6)
	ALG(142, msd_A_lsd8)
	ALG(143, msd_A_lsd10)
	ALG(144, msd_A_lsd12)
	ALG(145, msd_A_lsd_adaptive4)
	ALG(146, msd_A_lsd_adaptive6)
	ALG(147, msd_A_lsd_adaptive8)
	ALG(148, msd_A_lsd_adaptive10)
	ALG(149, msd_A_lsd_adaptive12)

	ALG(150, funnelsort_8way_bfs)
	ALG(151, funnelsort_16way_bfs)
	ALG(152, funnelsort_32way_bfs)
	ALG(153, funnelsort_64way_bfs)
	ALG(154, funnelsort_128way_bfs)
	ALG(155, funnelsort_8way_dfs)
	ALG(156, funnelsort_16way_dfs)
	ALG(157, funnelsort_32way_dfs)
	ALG(158, funnelsort_64way_dfs)
	ALG(159, funnelsort_128way_dfs)

	ALG(160, mergesort_2way)
	ALG(161, mergesort_3way)
	ALG(162, mergesort_4way)
	ALG(163, mergesort_2way_unstable)
	ALG(164, mergesort_3way_unstable)
	ALG(165, mergesort_4way_unstable)

	ALG(170, mergesort_losertree_64way)
	ALG(171, mergesort_losertree_128way)
	ALG(172, mergesort_losertree_256way)
	ALG(173, mergesort_losertree_512way)
	ALG(174, mergesort_losertree_1024way)

	ALG(180, mergesort_lcp_2way)
	ALG(181, mergesort_lcp_3way)
	ALG(182, mergesort_lcp_2way_unstable)
	ALG(183, mergesort_cache1_lcp_2way)
	ALG(184, mergesort_cache2_lcp_2way)
	ALG(185, mergesort_cache4_lcp_2way)
#undef ALG

	return algs;
}

static void
print_alg_names(const Algorithms& algs)
{
	Algorithms::const_iterator it = algs.begin();
	for (; it != algs.end(); ++it) {
		std::cout << "\t" << std::setw(2) << it->first << ":"
		          << it->second.second << "\n";
	}
}

static void
print_alg_nums(const Algorithms& algs)
{
	Algorithms::const_iterator it = algs.begin();
	for (; it != algs.end(); ++it) {
		std::cout << it->first << " ";
	}
	std::cout << std::endl;
}

static void
print_alg_name(const Algorithms& algs, int num)
{
	if (algs.find(num) != algs.end())
		std::cout << (algs.find(num))->second.second << std::endl;
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
	     "   --oprofile       : Executes `oprofile --start' just before calling the\n"
	     "                      actual sorting algorithm, and `oprofile --stop' after\n"
	     "                      returning from the call. Can be used to obtain more\n"
	     "                      accurate statistics with OProfile.\n"
	     "   -A,--alg-names   : Prints available algorithms.\n"
	     "   --alg-nums       : Prints available algorithm numbers, useful for scripts.\n"
	     "                      Example:\n"
	     "                         for i in `./sortstring --alg-nums` ; do\n"
	     "                                   ./sortstring $i input ; done\n"
	     "   --alg-name=k     : Print the name of algorithm number `k'.\n"
	     "   --suffix-sorting : Treat input as text, and sort each suffix of the text.\n"
	     "                      Can be _very_ slow.\n"
	     "   --write          : Writes sorted output to `/tmp/$USERNAME/alg.out'\n"
	     "   --write=outfile  : Writes sorted output to `outfile'\n"
	     "   --xml-stats      : Outputs statistics in XML (default: human readable)\n"
	     "   --hugetlb-text   : Place the input text into huge pages.\n"
	     "   --hugetlb-ptrs   : Place the string pointer array into huge pages.\n"
	     "                      HugeTLB requires kernel and hardware support, and\n"
	     "                      the `hugetlbfs' file system must be mounted somewhere.\n"
	     "   --raw            : The input file is in raw format: strings are delimited\n"
	     "                      with NULL bytes instead of newlines.\n"
	     "\n"
	     "Examples:\n"
	     "   # Get list of what is available:\n"
	     "   ./sortstring -A\n"
	     "\n"
	     "   # Sort input file with algorithm 1 (Quicksort):\n"
	     "   ./sortstring 1 ~/testdata/testfile1\n"
	     "\n"
	     "   # Sort all suffixes of of the given text file with Quicksort:\n"
	     "   ./sortstring --check --suffix-sorting 1 ~/testdata/text\n"
	     "\n");
}

static std::string
cmdline(int argc, char** argv)
{
	std::string tmp;
	for (int i=0; i < argc; ++i) {
		tmp += argv[i];
		tmp += " ";
	}
	return tmp;
}

int main(int argc, char** argv)
{
	log_file = new std::ofstream((std::string(".debug_") +
				(getenv("HOSTNAME") ?
				 getenv("HOSTNAME") : "")).c_str(),
			std::ios::out | std::ios::app);
	log() << "Start, argv=" << cmdline(argc, argv) << std::endl;
	Algorithms algs = get_algorithms();
	if (argc < 2) {
		usage();
		return 1;
	}
	static const struct option long_options[] = {
		{"help",           0, 0, 1000},
		{"alg-names",      0, 0, 'A' },
		{"alg-nums",       0, 0, 1002},
		{"check",          0, 0, 1003},
		{"suffix-sorting", 0, 0, 1004},
		{"write",          2, 0, 1005},
		{"alg-name",       1, 0, 1006},
		{"oprofile",       0, 0, 1007},
		{"xml-stats",      0, 0, 1008},
		{"hugetlb-text",   0, 0, 1009},
		{"hugetlb-ptrs",   0, 0, 1010},
		{"raw",            0, 0, 1011},
		{0,                0, 0, 0}
	};
	while (true) {
		int c = getopt_long(argc, argv, "hA", long_options, 0);
		if (c == -1) break;
		switch (c) {
		case 'h':
		case 1000:
			usage();
			return 0;
		case 'A':
			print_alg_names(algs);
			return 0;
		case 1002:
			print_alg_nums(algs);
			return 0;
		case 1003:
			opts.check_result = true;
			break;
		case 1004:
			opts.suffixsorting = true;
			break;
		case 1005:
			opts.write = true;
			if (optarg) opts.write_filename = optarg;
			break;
		case 1006:
			print_alg_name(algs, atoi(optarg));
			return 0;
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
		case '?':
		default:
			break;
		}
	}
	if (argc - 2 != optind) {
		std::cerr << "Sorry, wrong number of arguments.\n";
		return 1;
	}
	opts.algorithm = atoi(argv[optind]);
	std::string filename = argv[optind+1];
	if (filename.empty()) {
		std::cerr << "Sorry, filename not valid.\n";
		return 1;
	}
	unsigned long int seed = getpid()*time(0);
	//seed = 0xdeadbeef;
	srand48(seed);
	log() << "seed: " << seed << std::endl;
	unsigned char* text;
	unsigned char** strings;
	size_t text_len, strings_len;
	boost::tie(text, text_len) = readbytes(filename);
	if (opts.suffixsorting) {
		log() << "suffix sorting" << std::endl;
		boost::tie(strings,strings_len)=create_suffixes(text,text_len);
	} else {
		boost::tie(strings,strings_len)=create_strings(text,text_len);
	}
	run(opts.algorithm, algs, strings, strings_len, filename);
	free_text(text, text_len);
	free_pointers(strings, strings_len);
}
