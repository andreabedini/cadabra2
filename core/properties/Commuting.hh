
#pragma once

#include "properties/CommutingBehaviour.hh"

namespace cadabra {

	class Commuting : virtual public CommutingBehaviour {
		public:
			virtual ~Commuting();
			virtual std::string name() const override;
			virtual int sign() const override
				{
				return 1;
				}
		};

	}
