
#include <catch2/catch_test_macros.hpp>

#include "Storage.hh"
#include "Compare.hh"
#include "Grouping.hh"
#include "Kernel.hh"

using namespace cadabra;

TEST_CASE( "C++: Ex equality", "[ex1]" )
	{
	Ex ex1("y");
	Ex ex2("y");
	
	REQUIRE( ex1 == ex2 );
	}

TEST_CASE("C++: Ex comparison", "[ex2]")
	{
	Ex ex1("y");
	Ex ex2("y");

	std::map<Ex, int, tree_exact_less_no_wildcards_obj> mp;
	mp[ex1]=3;
	mp[ex2]=4;

	REQUIRE( mp.size() == 1 );
	}

TEST_CASE("C++: Ex grouping", "[group1]")
	{
	Kernel kernel(true);
	auto ex  = kernel.ex_from_string("{ A_{m n}, B_{m n}, A_{m n}, C_{m n}, B_{m n} }");
	auto grp = group_by_equivalence(*ex, ex->begin());
	
	REQUIRE( grp.size() == 2 );
	REQUIRE( grp[ex->child(ex->begin(), 2)].second == ex->child(ex->begin(), 0) );
	REQUIRE( grp[ex->child(ex->begin(), 4)].second == ex->child(ex->begin(), 1) );
	}

TEST_CASE("C++: advanced Ex grouping", "[group2]")
	{
	Kernel kernel(true);

	equiv_fun_t component_equiv = [](const Ex& ex, Ex::iterator it1, Ex::iterator it2) {
		it1 = ex.child(it1, 1);
		it2 = ex.child(it2, 1);
		return ex.equal_subtree(it1, it2);
		};
	
	auto ex  = kernel.ex_from_string("{ [1, 2] = A_{m n}, [1, 3] = B_{m n}, [1, 4] = A_{m n}, [2, 3] = C_{m n}, [2, 4] = B_{m n} }");
	auto grp2 = group_by_equivalence(*ex, ex->begin(), component_equiv);
	
	REQUIRE( grp2.size() == 2 );
	REQUIRE( grp2[ex->child(ex->begin(), 2)].second == ex->child(ex->begin(), 0) );
	REQUIRE( grp2[ex->child(ex->begin(), 4)].second == ex->child(ex->begin(), 1) );
	}


// Regression tests for individual bug fixes.

#include "ExManip.hh"
#include "Exceptions.hh"
#include "Multiplier.hh"
#include "NInterpolatingFunction.hh"
#include "properties/Indices.hh"

#include <cmath>

TEST_CASE("C++: prod_unwrap_single_term keeps the child multiplier", "[regression]")
	{
	// 3 * \prod{2 A} must unwrap to 6 A, not 3 A.
	Kernel kernel(true);
	Ex ex("\\prod");
	auto top = ex.begin();
	multiply(top->multiplier, 3);
	auto a = ex.append_child(top, str_node("A"));
	multiply(a->multiplier, 2);

	ExManip manip(kernel, ex);
	REQUIRE( manip.prod_unwrap_single_term(top) );
	REQUIRE( *ex.begin()->name == "A" );
	REQUIRE( *ex.begin()->multiplier == 6 );
	}

TEST_CASE("C++: sum_unwrap_single_term keeps the child multiplier", "[regression]")
	{
	Kernel kernel(true);
	Ex ex("\\sum");
	auto top = ex.begin();
	multiply(top->multiplier, 3);
	auto a = ex.append_child(top, str_node("A"));
	multiply(a->multiplier, 2);

	ExManip manip(kernel, ex);
	REQUIRE( manip.sum_unwrap_single_term(top) );
	REQUIRE( *ex.begin()->name == "A" );
	REQUIRE( *ex.begin()->multiplier == 6 );
	}

TEST_CASE("C++: NInterpolatingFunction rejects NaN and stays usable", "[regression]")
	{
	// NaN used to pass the domain check, make the interval search fail
	// and leave last_index at the final point, so that the next call
	// read past the end of var_values.
	NInterpolatingFunction f;
	f.var_values = NTensor({0.0, 1.0, 2.0});
	f.fun_values = NTensor({0.0, 10.0, 30.0});

	REQUIRE_THROWS_AS( f.evaluate(std::nan("")), ArgumentException );
	REQUIRE( f.evaluate(1.5).real() == 20.0 );
	REQUIRE( f.evaluate(0.5).real() == 5.0 );
	REQUIRE( f.evaluate(2.0).real() == 30.0 );
	REQUIRE( f.evaluate(0.0).real() == 0.0 );
	}

TEST_CASE("C++: NInterpolatingFunction with a single point", "[regression]")
	{
	NInterpolatingFunction f;
	f.var_values = NTensor({1.0});
	f.fun_values = NTensor({5.0});

	REQUIRE_THROWS_AS( f.evaluate(1.0), ArgumentException );
	}

namespace {
	const Indices *declare_indices(Kernel& kernel, const std::string& args)
		{
		auto pat = kernel.ex_from_string("{a,b}");
		auto arg = kernel.ex_from_string(args);
		// The kernel takes ownership and keeps this very object.
		auto ind = new Indices();
		kernel.inject_property(ind, pat, arg);
		return ind;
		}
	}

TEST_CASE("C++: Indices strips quotes from parent without a name", "[regression]")
	{
	// Used to index parent_name with set_name.size()-1, i.e. SIZE_MAX here.
	Kernel kernel(true);
	auto ind = declare_indices(kernel, "parent=\"full\"");
	REQUIRE( ind != nullptr );
	REQUIRE( ind->parent_name == "full" );
	}

TEST_CASE("C++: Indices strips quotes from parent with a name", "[regression]")
	{
	// With set_name "sub" the old code tested parent_name[2] ('u'),
	// so the quotes were left in place.
	Kernel kernel(true);
	auto ind = declare_indices(kernel, "sub, parent=\"full\"");
	REQUIRE( ind != nullptr );
	REQUIRE( ind->set_name == "sub" );
	REQUIRE( ind->parent_name == "full" );
	}
