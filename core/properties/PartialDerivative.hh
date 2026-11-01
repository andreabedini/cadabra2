
#pragma once

#include "properties/Derivative.hh"
#include "properties/Spinor.hh"

namespace cadabra {

	class PartialDerivative : public Derivative, public Inherit<Spinor>, virtual public property {
		public :
			virtual ~PartialDerivative();
			virtual std::string name() const override;

			virtual unsigned int size(const Properties&, Ex&, Ex::iterator) const override;
			virtual tab_t        get_tab(const Properties&, Ex&, Ex::iterator, unsigned int) const override;
		};

	}
