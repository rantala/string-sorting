#include "routine.h"
#include "parallel_string_radix_sort.h"

namespace parallel_string_radix_sort {
namespace internal {
template<> class Compare<const unsigned char*> {
public:
	explicit Compare(int depth) : depth_(depth) {}
	inline bool operator()(const unsigned char* const a,
			const unsigned char* const b) {
		return strcmp((char*)a + depth_, (char*)b + depth_) < 0;
	}
private:
	int depth_;
};
}
}

void parallel_msd_radix_sort(unsigned char **strings, size_t count)
{
	parallel_string_radix_sort::Sort<const unsigned char *>(
			(const unsigned char **)strings, count);
}

ROUTINE_REGISTER_MULTICORE(parallel_msd_radix_sort,
		"Parallel MSD radix sort by Takuya Akiba")
