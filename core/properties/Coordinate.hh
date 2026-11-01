
#pragma once

#include "Props.hh"

namespace cadabra {

	class Coordinate : public property {
		public:
			virtual std::string name() const override;
			virtual void        validate(Kernel&, std::shared_ptr<Ex>) const override;
		};

	}
