
#pragma once

#include "Algorithm.hh"

namespace cadabra {

	class flatten_product : public Algorithm {
		public:
			flatten_product(const Kernel&, Ex&);

			virtual bool     can_apply(iterator) override;
			virtual result_t apply(iterator&) override;

			bool make_consistent_only, is_diff;
		};
	}

