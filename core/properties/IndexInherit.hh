
#pragma once

#include "Props.hh"
#include "properties/TableauBase.hh"

namespace cadabra {

	class IndexInherit : virtual public property {
		public:
			virtual std::string name() const override
				{
				return std::string("IndexInherit");
				};

		};

	}
