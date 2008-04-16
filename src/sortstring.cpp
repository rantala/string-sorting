/*
 * Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

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

void system(const std::string& cmd)
{
	(void) system(cmd.c_str());
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

static void
readbytes(const std::string& fname,
          std::vector<unsigned char>& text)
{
	// Use mmap and memcpy, because it's much much faster under callgrind
	// compared to manual byte-by-byte copy.
	off_t filesize = (off_t)0;
	int   fd   = -1;
	void* vp   = NULL;
	if ((fd = open(fname.c_str(), O_RDONLY)) == -1) {
		std::cerr << "Could not open file!" << std::endl;
		exit(1);
	}
	if ((filesize = lseek(fd, 0, SEEK_END)) == -1) {
		std::cerr << "Could not seek file!" << std::endl;
		exit(1);
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		std::cerr << "Could not seek file!" << std::endl;
		exit(1);
	}
	if ((vp = mmap(0, filesize, PROT_READ, MAP_SHARED, fd, 0))
			== MAP_FAILED) {
		std::cerr << "Could not mmap file!" << std::endl;
		exit(1);
	}
	unsigned char* data = (unsigned char*) vp;
	text.reserve(filesize);
	text.assign(filesize, 0);
	memcpy(&text[0], data, filesize);
	if (munmap(vp, filesize) == -1) {
		std::cerr << "Could not munmap file!" << std::endl;
		exit(1);
	}
	if (close(fd) == -1) {
		std::cerr << "Could not close file!" << std::endl;
		exit(1);
	}
}

static void
create_strings(std::vector<unsigned char>& text,
               std::vector<unsigned char*>& strings)
{
	// Calculate how many strings we'll have to reserve correct amount of
	// bytes for the strings vector. Makes it easier to calculate the heap
	// peak memory taken by algorithms.
	size_t strs = 0;
	for (size_t i=0;i<text.size();++i) { if (text[i] == '\n') ++strs; }
	strings.reserve(strs);
	size_t pos = 0;
	unsigned char* line_start = &text[0];
	for (size_t pos=0; pos<text.size(); ++pos) {
		if (text[pos] == '\n') {
			strings.push_back(line_start);
			line_start = &text[0] + pos + 1;
			text[pos] = 0;
		}
	}
}

static void
create_suffixes(std::vector<unsigned char>& text,
                std::vector<unsigned char*>& strings)
{
	for (size_t i=0; i < text.size(); ++i) {
		strings.push_back(&text[i]);
	}
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
	for (size_t i=0; i < n-1; ++i) {
		if (strings[i] == strings[i+1]) {
			++identical;
		}
		if (strcmp(strings[i], strings[i+1]) > 0) {
			++wrong;
		}
	}
	if (identical) {
		std::cerr << "WARNING: found " << identical << " identical pointers!\n";
	}
	if (wrong) {
		std::cerr << "WARNING: found " << wrong << " incorrect orderings!\n";
	}
	if (not identical and not wrong) {
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

	algs[30] = make_pair(msd_DV,          "msd_DV (std::vector)");
	algs[31] = make_pair(msd_DV_adaptive, "msd_DV (std::vector, Adaptive)");
	algs[32] = make_pair(msd_DL,          "msd_DL (std::list)");
	algs[33] = make_pair(msd_DL_adaptive, "msd_DL (std::list, Adaptive)");
	algs[34] = make_pair(msd_DD,          "msd_DD (std::deque)");
	algs[35] = make_pair(msd_DD_adaptive, "msd_DD (std::deque, Adaptive)");

	algs[40] = make_pair(msd_DV_REALLOC,          "msd_DV (realloc)");
	algs[41] = make_pair(msd_DV_REALLOC_adaptive, "msd_DV (realloc, Adaptive)");
	algs[42] = make_pair(msd_DV_MALLOC,                 "msd_DV (malloc, full clear)");
	algs[43] = make_pair(msd_DV_MALLOC_adaptive,        "msd_DV (malloc, Adaptive, full clear)");
	algs[44] = make_pair(msd_DV_CHEAT_REALLOC,          "msd_DV (realloc, counter clear)");
	algs[45] = make_pair(msd_DV_CHEAT_REALLOC_adaptive, "msd_DV (realloc, Adaptive, counter clear)");
	algs[46] = make_pair(msd_DV_CHEAT_MALLOC,           "msd_DV (malloc, counter clear)");
	algs[47] = make_pair(msd_DV_CHEAT_MALLOC_adaptive,  "msd_DV (malloc, Adaptive, counter clear)");
	algs[48] = make_pair(msd_D_vector_block,             "msd_D_vector_block");
	algs[49] = make_pair(msd_D_vector_block_adaptive,    "msd_D_vector_block_adaptive");
	algs[50] = make_pair(msd_D_vector_brodnik,           "msd_D_vector_brodnik");
	algs[51] = make_pair(msd_D_vector_brodnik_adaptive,  "msd_D_vector_brodnik_adaptive");
	algs[52] = make_pair(msd_D_vector_bagwell,           "msd_D_vector_bagwell");
	algs[53] = make_pair(msd_D_vector_bagwell_adaptive,  "msd_D_vector_bagwell_adaptive");

	algs[54] = make_pair(msd_DB,          "msd_DB");
	algs[55] = make_pair(msd_A,           "msd_A");
	algs[56] = make_pair(msd_A_adaptive,  "msd_A_adaptive");
	algs[57] = make_pair(msd_A2,          "msd_A2");
	algs[58] = make_pair(msd_A2_adaptive, "msd_A2_adaptive");

	algs[60] = make_pair(multikey_simd1, "multikey_simd1");
	algs[61] = make_pair(multikey_simd2, "multikey_simd2");
	algs[62] = make_pair(multikey_simd4, "multikey_simd4");
	algs[63] = make_pair(multikey_dynamic_vector1, "multikey_dynamic_vector1");
	algs[64] = make_pair(multikey_dynamic_vector2, "multikey_dynamic_vector2");
	algs[65] = make_pair(multikey_dynamic_vector4, "multikey_dynamic_vector4");
	algs[66] = make_pair(multikey_dynamic_brodnik1, "multikey_dynamic_brodnik1");
	algs[67] = make_pair(multikey_dynamic_brodnik2, "multikey_dynamic_brodnik2");
	algs[68] = make_pair(multikey_dynamic_brodnik4, "multikey_dynamic_brodnik4");
	algs[69] = make_pair(multikey_dynamic_bagwell1, "multikey_dynamic_bagwell1");
	algs[70] = make_pair(multikey_dynamic_bagwell2, "multikey_dynamic_bagwell2");
	algs[71] = make_pair(multikey_dynamic_bagwell4, "multikey_dynamic_bagwell4");
	algs[72] = make_pair(multikey_dynamic_vector_block1, "multikey_dynamic_vector_block1");
	algs[73] = make_pair(multikey_dynamic_vector_block2, "multikey_dynamic_vector_block2");
	algs[74] = make_pair(multikey_dynamic_vector_block4, "multikey_dynamic_vector_block4");
	algs[75] = make_pair(multikey_block1, "multikey_block1");
	algs[76] = make_pair(multikey_block2, "multikey_block2");
	algs[77] = make_pair(multikey_block4, "multikey_block4");
	algs[78] = make_pair(multikey_multipivot_brute_simd1, "multikey_multipivot_brute_simd1");
	algs[79] = make_pair(multikey_multipivot_brute_simd2, "multikey_multipivot_brute_simd2");
	algs[80] = make_pair(multikey_multipivot_brute_simd4, "multikey_multipivot_brute_simd4");
	algs[81] = make_pair(multikey_cache4, "multikey_cache4");
	algs[82] = make_pair(multikey_cache8, "multikey_cache8");

	algs[90] = make_pair(burstsort_mkq_simpleburst_1,    "burstsort_mkq_simpleburst_1");
	algs[91] = make_pair(burstsort_mkq_simpleburst_2,    "burstsort_mkq_simpleburst_2");
	algs[92] = make_pair(burstsort_mkq_simpleburst_4,    "burstsort_mkq_simpleburst_4");
	algs[95] = make_pair(burstsort_mkq_recursiveburst_1, "burstsort_mkq_recursiveburst_1");
	algs[96] = make_pair(burstsort_mkq_recursiveburst_2, "burstsort_mkq_recursiveburst_2");
	algs[97] = make_pair(burstsort_mkq_recursiveburst_4, "burstsort_mkq_recursiveburst_4");

	algs[100] = make_pair(burstsort_vector,  "burstsort_vector");
	algs[101] = make_pair(burstsort_brodnik, "burstsort_brodnik");
	algs[102] = make_pair(burstsort_bagwell, "burstsort_bagwell");
	algs[103] = make_pair(burstsort_vector_block, "burstsort_vector_block");
	algs[104] = make_pair(burstsort_superalphabet_vector,  "burstsort_superalphabet_vector");
	algs[105] = make_pair(burstsort_superalphabet_brodnik, "burstsort_superalphabet_brodnik");
	algs[106] = make_pair(burstsort_superalphabet_bagwell, "burstsort_superalphabet_bagwell");
	algs[107] = make_pair(burstsort_superalphabet_vector_block, "burstsort_superalphabet_vector_block");
	algs[110] = make_pair(burstsort_sampling_vector,  "burstsort_sampling_vector");
	algs[111] = make_pair(burstsort_sampling_brodnik, "burstsort_sampling_brodnik");
	algs[112] = make_pair(burstsort_sampling_bagwell, "burstsort_sampling_bagwell");
	algs[113] = make_pair(burstsort_sampling_vector_block, "burstsort_sampling_vector_block");
	algs[114] = make_pair(burstsort_sampling_superalphabet_vector,  "burstsort_sampling_superalphabet_vector");
	algs[115] = make_pair(burstsort_sampling_superalphabet_brodnik, "burstsort_sampling_superalphabet_brodnik");
	algs[116] = make_pair(burstsort_sampling_superalphabet_bagwell, "burstsort_sampling_superalphabet_bagwell");
	algs[117] = make_pair(burstsort_sampling_superalphabet_vector_block, "burstsort_sampling_superalphabet_vector_block");

	algs[120] = make_pair(burstsort2_vector,  "burstsort2_vector");
	algs[121] = make_pair(burstsort2_brodnik, "burstsort2_brodnik");
	algs[122] = make_pair(burstsort2_bagwell, "burstsort2_bagwell");
	algs[123] = make_pair(burstsort2_vector_block, "burstsort2_vector_block");
	algs[124] = make_pair(burstsort2_superalphabet_vector,  "burstsort2_superalphabet_vector");
	algs[125] = make_pair(burstsort2_superalphabet_brodnik, "burstsort2_superalphabet_brodnik");
	algs[126] = make_pair(burstsort2_superalphabet_bagwell, "burstsort2_superalphabet_bagwell");
	algs[127] = make_pair(burstsort2_superalphabet_vector_block, "burstsort2_superalphabet_vector_block");
	algs[130] = make_pair(burstsort2_sampling_vector,  "burstsort2_sampling_vector");
	algs[131] = make_pair(burstsort2_sampling_brodnik, "burstsort2_sampling_brodnik");
	algs[132] = make_pair(burstsort2_sampling_bagwell, "burstsort2_sampling_bagwell");
	algs[133] = make_pair(burstsort2_sampling_vector_block, "burstsort2_sampling_vector_block");
	algs[134] = make_pair(burstsort2_sampling_superalphabet_vector,  "burstsort2_sampling_superalphabet_vector");
	algs[135] = make_pair(burstsort2_sampling_superalphabet_brodnik, "burstsort2_sampling_superalphabet_brodnik");
	algs[136] = make_pair(burstsort2_sampling_superalphabet_bagwell, "burstsort2_sampling_superalphabet_bagwell");
	algs[137] = make_pair(burstsort2_sampling_superalphabet_vector_block, "burstsort2_sampling_superalphabet_vector_block");

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
usage(const Algorithms& algs)
{
	std::cout <<
	     "String sorting\n"
	     "--------------\n"
	     "\n"
	     "Usage: ./sortstring [options] <algorithm> <filename>\n"
	     "\n"
	     "Examples:\n"
	     "   ./sortstring 1 ~/testdata/testfile1\n"
	     "   ./sortstring --check --suffix-sorting 1 ~/testdata/text\n"
	     "\n"
	     "Options:\n"
	     "   --check          : Tries to check output for validity. Might not catch\n"
	     "                      all errors. Prints a warning when errors found.\n"
	     "   --oprofile       : Executes `oprofile --start' just before calling the\n"
	     "                      actual sorting algorithm, and `oprofile --stop' after\n"
	     "                      returning from the call. Can be used to obtain more\n"
	     "                      accurate statistics with OProfile.\n"
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
	     "\n"
	     "Available algorithms:\n";

	print_alg_names(algs);
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
		usage(algs);
		return 1;
	}
	static struct option long_options[] = {
		{"help",           0, 0, 1000},
		{"alg-names",      0, 0, 1001},
		{"alg-nums",       0, 0, 1002},
		{"check",          0, 0, 1003},
		{"suffix-sorting", 0, 0, 1004},
		{"write",          2, 0, 1005},
		{"alg-name",       1, 0, 1006},
		{"oprofile",       0, 0, 1007},
		{"xml-stats",      0, 0, 1008},
		{0,                0, 0, 0}
	};
	while (true) {
		int c = getopt_long(argc, argv, "h", long_options, 0);
		if (c == -1) break;
		switch (c) {
		case 'h':
		case 1000:
			usage(algs);
			return 0;
		case 1001: 
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
	std::vector<unsigned char> text;
	std::vector<unsigned char*> strings;
	readbytes(filename, text);
	if (opts.suffixsorting) {
		log() << "suffix sorting" << std::endl;
		strings.reserve(text.size());
		create_suffixes(text, strings);
	} else {
		create_strings(text, strings);
	}
	run(opts.algorithm, algs, &strings[0], strings.size(), filename);
}
