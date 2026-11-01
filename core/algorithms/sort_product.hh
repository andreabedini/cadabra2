
#pragma once

#include "Algorithm.hh"

namespace cadabra {

	class sort_product : public Algorithm {
		public:
			sort_product(const Kernel&, Ex&);

			virtual bool     can_apply(iterator) override;
			virtual result_t apply(iterator&) override;

			void dont_cleanup();

		private:
			//			bool ignore_numbers_;
			bool cleanup;
		};

	}
