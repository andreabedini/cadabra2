
#include "Compare.hh"
#include "Storage.hh"

#include <iostream>

using namespace cadabra;

int main()
	{
	Ex a1("A"), a2("A");

	a1.begin()->fl.parent_rel=str_node::p_sub;
	a2.begin()->fl.parent_rel=str_node::p_super;

	tree_exact_less_for_indexmap_obj compare;

	// Objects that differ only in index position must be ordered
	// one way or the other, not considered equal.
	if(compare(a1, a2)==compare(a2, a1)) {
		std::cerr << "test_comparison: sub/super index position not distinguished" << std::endl;
		return 1;
		}
	return 0;
	}
