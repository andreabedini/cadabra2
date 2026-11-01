
#pragma once

#include "Algorithm.hh"

namespace cadabra {

	class keep_terms : public Algorithm {
		public:
			keep_terms(const Kernel&, Ex&, std::vector<int> terms);

			virtual bool     can_apply(iterator) override;
			virtual result_t apply(iterator&) override;

		private:
			std::vector<int> terms_;
		};

	}
