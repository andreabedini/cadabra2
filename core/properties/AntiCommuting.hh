
#pragma once

#include "properties/CommutingBehaviour.hh"

namespace cadabra {

	class AntiCommuting : virtual public CommutingBehaviour {
		public:
			virtual ~AntiCommuting();
			virtual std::string name() const override;
			virtual int sign() const override
				{
				return -1;
				}
		};

	}
