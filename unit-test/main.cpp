#include "../src/vector_brodnik.h"
#include "../src/vector_bagwell.h"
#include "../src/vector_block.h"
#include <cassert>
#include <iostream>

template <typename Container>
void test_basics()
{
	std::cerr<<__PRETTY_FUNCTION__<<std::endl;
	{
		Container v;
		assert(v.size()==0);
	}
	{
		Container v;
		v.push_back(1);
		assert(v.size()==1);
		assert(v[0]==1);
	}
	{
		Container v;
		const unsigned N=1000000;
		for (unsigned i=0; i < N; ++i) v.push_back(i);
		assert(v.size()==N);
		for (unsigned i=0; i < N; ++i) assert(v[i]==i);
	}
	{
		Container v;
		v.push_back(1);
		v.clear();
		assert(v.size()==0);
		v.push_back(2);
		assert(v.size()==1);
		assert(v[0]==2);
	}
}

int main()
{
	test_basics<vector_brodnik<int> >();
	test_basics<vector_bagwell<int> >();
	test_basics<vector_block<int> >();
	std::cerr<<"All OK\n";
}
