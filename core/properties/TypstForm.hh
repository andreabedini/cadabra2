#pragma once

#include "Props.hh"

namespace cadabra {

	class TypstForm : virtual public property {
		public:
			virtual std::string name() const override;
			virtual bool parse(Kernel&, keyval_t&) override;
			virtual std::string unnamed_argument() const override;

			/// Return the Typst string which should be used to display
			/// the object to which this property is associated. Will be
			/// interpreted by DisplayTypst because it can contain patterns
			/// (as e.g. in `a{b??}::TypstForm("|", b??, "⟩")`).
			std::string typst_form() const;

			std::vector<Ex> typst;
		};

	}
