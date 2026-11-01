
#pragma once

#include "Props.hh"

namespace cadabra {

	class SortOrder : public list_property {
		public:
			virtual std::string name() const override;
			virtual match_t equals(const property *) const override;
		};

	}
