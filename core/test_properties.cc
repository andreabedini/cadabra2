// Ownership of properties held by the kernel.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include "Kernel.hh"
#include "properties/Indices.hh"
#include "properties/Symmetric.hh"

using namespace cadabra;

TEST_CASE("C++: a redeclared property is released unless someone else holds it", "[properties]")
	{
	Kernel kernel(true);
	auto pat = [&]{ return kernel.ex_from_string("A_{m n}"); };

	std::weak_ptr<const property> first, second;
		{
		auto p1 = std::make_shared<Symmetric>();
		first = p1;
		kernel.inject_property(p1, pat(), nullptr);
		}
	REQUIRE( !first.expired() ); // the kernel holds it

	// Someone else (e.g. a Python object) keeps a reference to the first.
	auto holder = first.lock();
		{
		auto p2 = std::make_shared<Symmetric>();
		second = p2;
		kernel.inject_property(p2, pat(), nullptr);
		}
	REQUIRE( !second.expired() );
	// Replaced in the kernel, but still alive and usable for the holder.
	REQUIRE( !first.expired() );
	REQUIRE( holder->name() == "Symmetric" );

	holder.reset();
	REQUIRE( first.expired() );
	}

TEST_CASE("C++: properties are released with the kernel", "[properties]")
	{
	std::weak_ptr<const property> w;
		{
		Kernel kernel(true);
		auto p = std::make_shared<Symmetric>();
		w = p;
		kernel.inject_property(p, kernel.ex_from_string("A_{m n}"), nullptr);
		}
	REQUIRE( w.expired() );
	}

TEST_CASE("C++: a property that fails to parse is not leaked", "[properties]")
	{
	Kernel kernel(true);
	std::weak_ptr<const property> w;
		{
		auto p = std::make_shared<Indices>();
		w = p;
		REQUIRE_THROWS( kernel.inject_property(p, kernel.ex_from_string("{a,b}"),
															kernel.ex_from_string("bogus=1")) );
		}
	REQUIRE( w.expired() );
	}

TEST_CASE("C++: the shared_ptr given to Python aliases the kernel's", "[properties]")
	{
	Kernel kernel(true);
	auto p = std::make_shared<Symmetric>();
	kernel.inject_property(p, kernel.ex_from_string("A_{m n}"), nullptr);
	auto ex = kernel.ex_from_string("A_{m n}");
	const Symmetric *found = kernel.properties.get<Symmetric>(ex->begin());
	REQUIRE( found == p.get() );
	// What the Python bindings do to co-own a property found in the kernel.
	std::shared_ptr<const Symmetric> shared(found->shared_from_this(), found);
	REQUIRE( shared.use_count() == p.use_count() );
	}
