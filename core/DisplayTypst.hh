
#pragma once

#include "Props.hh"
#include "Storage.hh"
#include "DisplayBase.hh"
#include <ostream>
#include <map>
#include <set>

namespace cadabra {

	/// \ingroup display
	///
	/// Class to handle display of expressions using Typst math notation
	/// (the contents of a Typst `$ ... $` block, without the dollar
	/// signs). This follows the logic of DisplayTeX closely, and
	/// differs mainly in the syntax it emits:
	///
	///  - Multi-letter symbols are quoted, since Typst would otherwise
	///    interpret them as variables (e.g. `ab` prints as `italic("ab")`).
	///  - Grouping is done with round brackets, which Typst drops when
	///    they are used to group sub- and superscripts or function arguments
	///    (e.g. `A_(mu nu)`, `frac(a, b)`, `sqrt(x)`).
	///  - Symbols whose Typst names have changed between Typst versions
	///    (e.g. `⊗`, `⊕`, `ℏ`) are emitted as Unicode characters.
	///
	/// The LaTeXForm property is not used, since its contents are LaTeX.

	class DisplayTypst : public DisplayBase {
		public:
			DisplayTypst(const Kernel&, const Ex&);

		protected:
			virtual bool needs_brackets(Ex::iterator it) override;

		private:
			/// Print the multiplier of the node, followed by a thin space
			/// unless 'spacing' is false or the node is a bare number.
			void print_multiplier(std::ostream&, Ex::iterator, int mult=1, bool spacing=true);
			void print_opening_bracket(std::ostream&, str_node::bracket_t, str_node::parent_rel_t);
			void print_closing_bracket(std::ostream&, str_node::bracket_t, str_node::parent_rel_t);
			void print_parent_rel(std::ostream&, str_node::parent_rel_t, bool first);
			void print_children(std::ostream&, Ex::iterator, int skip=0);

			/// Convert a Cadabra symbol name to a Typst math expression.
			std::string typstify(std::string) const;

			/// Escape a string so that it can be put inside a Typst string literal.
			static std::string escape_string(const std::string&);

			bool print_star=false;
			bool tight_star=false;

			int bracket_level=0;

			/// For every object encountered, dispatch will figure out the
			/// most appropriate way to convert it into a Typst
			/// expression. See DisplayTeX::dispatch for details.

			virtual void dispatch(std::ostream&, Ex::iterator) override;

			/// Printing members for various standard constructions,
			/// mirroring those in DisplayTeX.

			void print_productlike(std::ostream&, Ex::iterator, const std::string& inbetween);
			void print_sumlike(std::ostream&, Ex::iterator);
			void print_fraclike(std::ostream&, Ex::iterator);
			void print_commalike(std::ostream&, Ex::iterator);
			void print_arrowlike(std::ostream&, Ex::iterator);
			void print_powlike(std::ostream&, Ex::iterator);
			void print_intlike(std::ostream&, Ex::iterator);
			void print_equalitylike(std::ostream&, Ex::iterator);
			void print_commutator(std::ostream& str, Ex::iterator it, bool comm);
			void print_components(std::ostream&, Ex::iterator);
			void print_conditional(std::ostream&, Ex::iterator);
			void print_relation(std::ostream&, Ex::iterator);
			void print_indexbracket(std::ostream&, Ex::iterator);
			void print_wedgeproduct(std::ostream&, Ex::iterator);
			void print_tableau(std::ostream&, Ex::iterator);
			void print_ftableau(std::ostream&, Ex::iterator);
			void print_dot(std::ostream& str, Ex::iterator it);
			void print_dots(std::ostream& str, Ex::iterator it);
			void print_other(std::ostream& str, Ex::iterator it);

			/// Handle unprintable constructions like `\pow{#}`, which need
			/// to print verbatim. Pass it the top-level iterator, will return
			/// true if printing has been taken care of.
			bool handle_unprintable_wildcards(std::ostream&, Ex::iterator it) const;

			/// Determine whether the indicated object 'obj', when acting on
			/// the single argument 'arg', reads as an operator, and therefore
			/// 'arg' does not need to be wrapped in brackets.
			/// Example: the tree `\partial{A}` can be displayed as `partial A`.

			bool reads_as_operator(Ex::iterator obj, Ex::iterator arg) const;

			/// Does the node have direct sub- or superscript children?
			bool has_scripts(Ex::iterator it) const;

			/// Map from Cadabra symbols to Typst symbols.
			std::map<std::string, std::string> symmap;

			/// List of operators which are Typst functions, and therefore need
			/// their arguments in round brackets directly following the name,
			/// even when they read as operators (e.g. `\sqrt`).
			std::set<std::string> call_operators;

		};

	}
