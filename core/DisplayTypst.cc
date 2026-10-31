
#include "Config.hh"
#include "Exceptions.hh"
#include "Symbols.hh"
#include "Grouping.hh"
#include "DisplayTypst.hh"
#include "Algorithm.hh"
#include "algorithms/substitute.hh"
#include "properties/TypstForm.hh"
#include "properties/Derivative.hh"
#include "properties/Accent.hh"
#include "properties/Tableau.hh"
#include "properties/FilledTableau.hh"
#include "NTensor.hh"
#include "NInterpolatingFunction.hh"

#include <regex>

using namespace cadabra;

DisplayTypst::DisplayTypst(const Kernel& k, const Ex& e)
	: DisplayBase(k, e)
	{
	symmap = {
			// Greek letters. Note that LaTeX and Typst disagree on which
			// variant of epsilon, theta, phi etc. is the 'default' one.
			{"\\alpha", "alpha"},
			{"\\beta", "beta"},
			{"\\gamma", "gamma"},
			{"\\delta", "delta"},
			{"\\epsilon", "epsilon.alt"},
			{"\\varepsilon", "epsilon"},
			{"\\zeta", "zeta"},
			{"\\eta", "eta"},
			{"\\theta", "theta"},
			{"\\vartheta", "theta.alt"},
			{"\\iota", "iota"},
			{"\\kappa", "kappa"},
			{"\\varkappa", "kappa.alt"},
			{"\\lambda", "lambda"},
			{"\\mu", "mu"},
			{"\\nu", "nu"},
			{"\\xi", "xi"},
			{"\\omicron", "omicron"},
			{"\\pi", "pi"},
			{"\\varpi", "pi.alt"},
			{"\\rho", "rho"},
			{"\\varrho", "rho.alt"},
			{"\\sigma", "sigma"},
			{"\\varsigma", "sigma.alt"},
			{"\\tau", "tau"},
			{"\\upsilon", "upsilon"},
			{"\\phi", "phi.alt"},
			{"\\varphi", "phi"},
			{"\\chi", "chi"},
			{"\\psi", "psi"},
			{"\\omega", "omega"},
			{"\\Gamma", "Gamma"},
			{"\\Delta", "Delta"},
			{"\\Theta", "Theta"},
			{"\\Lambda", "Lambda"},
			{"\\Xi", "Xi"},
			{"\\Pi", "Pi"},
			{"\\Sigma", "Sigma"},
			{"\\Upsilon", "Upsilon"},
			{"\\Phi", "Phi"},
			{"\\Psi", "Psi"},
			{"\\Omega", "Omega"},
			// Operators and other symbols.
			{"\\partial", "partial"},
			{"\\nabla", "nabla"},
			{"\\infty", "infinity"},
			{"\\hbar", "ℏ"},
			{"\\ell", "ell"},
			{"\\dagger", "dagger"},
			{"\\prime", "prime"},
			{"\\star", "star"},
			{"\\ast", "ast"},
			{"\\pm", "plus.minus"},
			{"\\mp", "minus.plus"},
			{"\\times", "times"},
			{"\\cdot", "dot.op"},
			{"\\otimes", "⊗"},
			{"\\oplus", "⊕"},
			{"\\wedge", "∧"},
			{"\\square", "square"},
			{"\\Box", "square"},
			{"\\ldots", "dots.h"},
			{"\\cdots", "dots.c"},
			{"\\int", "integral"},
			{"\\sum", "sum"},
			{"\\prod", "product"},
			// Functions and operators which Typst typesets upright.
			{"\\sin", "sin"},
			{"\\cos", "cos"},
			{"\\tan", "tan"},
			{"\\cot", "cot"},
			{"\\sec", "sec"},
			{"\\csc", "csc"},
			{"\\arcsin", "arcsin"},
			{"\\arccos", "arccos"},
			{"\\arctan", "arctan"},
			{"\\sinh", "sinh"},
			{"\\cosh", "cosh"},
			{"\\tanh", "tanh"},
			{"\\coth", "coth"},
			{"\\exp", "exp"},
			{"\\log", "log"},
			{"\\ln", "ln"},
			{"\\det", "det"},
			{"\\dim", "dim"},
			{"\\ker", "ker"},
			{"\\lim", "lim"},
			{"\\max", "max"},
			{"\\min", "min"},
			{"\\sup", "sup"},
			{"\\inf", "inf"},
			{"\\arg", "arg"},
			{"\\deg", "deg"},
			{"\\gcd", "gcd"},
			{"\\Pr", "Pr"},
			{"\\tr", "tr"},
			// Typst functions, see call_operators.
			{"\\sqrt", "sqrt"},
			{"\\hat", "hat"},
			{"\\widehat", "hat"},
			{"\\tilde", "tilde"},
			{"\\widetilde", "tilde"},
			{"\\bar", "overline"},
			{"\\overline", "overline"},
			{"\\underline", "underline"},
			{"\\dot", "dot"},
			{"\\ddot", "dot.double"},
			{"\\vec", "arrow"}
		};

	call_operators = {
		"\\sqrt",
		"\\hat",
		"\\widehat",
		"\\tilde",
		"\\widetilde",
		"\\bar",
		"\\overline",
		"\\underline",
		"\\dot",
		"\\ddot",
		"\\vec"
		};
	}

bool DisplayTypst::needs_brackets(Ex::iterator it)
	{
	// This follows DisplayTeX::needs_brackets.
	if(tree.is_head(it)) return false;

	std::string parent=*tree.parent(it)->name;
	std::string child =*it->name;

	if(parent=="\\partial" && (child=="\\sum" || child=="\\oplus")) return false;

	if(parent=="\\int" && (child=="\\sum" || child=="\\oplus")) return true;

	if(parent=="\\indexbracket" && child=="\\prod") return false;

	const Derivative *der=kernel.properties.get<Derivative>(it);

	if(parent=="\\pow") {
		Ex::sibling_iterator sib=it;
		++sib;
		if(tree.index(it)==0 && *sib->name=="1" && *sib->multiplier==multiplier_t(1)/2) return false; // sqrt(this)
		if(tree.index(it)==0 && !it->is_integer() && *it->multiplier!=1) return true;
		int nc = Ex::number_of_children(it);
		int ic = Algorithm::number_of_direct_indices(it);
		if(nc-ic>0) {
			Ex::sibling_iterator ch = Ex::begin(it);
			while(ch->is_index())
				++ch;
			bool rao = reads_as_operator(it, ch);
			if(rao)
				return true;
			}
		}

	if(parent=="\\oplus" && child=="\\otimes") return true;

	if(parent=="\\pow" && (child=="\\prod" || child=="\\sum" || child=="\\oplus" || der)) return  true;

	if(parent=="\\wedge" && child=="\\prod") return true;

	if(parent=="\\prod" || parent=="\\frac" || parent=="\\pow" || parent=="\\wedge") {
		if(*tree.parent(it)->name!="\\frac" && (*it->name=="\\sum" || *it->name=="\\oplus")) return true;
		}
	else if(it->fl.parent_rel==str_node::p_none) {   // function argument
		if(*it->name=="\\sum" || *it->name=="\\oplus" || *it->name=="\\pow") return false;
		}
	else {
		if(*it->name=="\\sum")  return true;
		if(*it->name=="\\oplus")  return true;
		if(*it->name=="\\prod") return true;
		}
	return false;
	}

bool DisplayTypst::reads_as_operator(Ex::iterator obj, Ex::iterator arg) const
	{
	const Derivative *der=kernel.properties.get<Derivative>(obj);
	if(der) {
		if(*arg->name=="\\pow") return true;
		const TypstForm *tf = kernel.properties.get<TypstForm>(arg);
		if((*arg->name).size()==1 || tf || cadabra::symbols::greekmap.find(*arg->name)!=cadabra::symbols::greekmap.end()) return true;
		}

	if(*obj->name=="\\cos" || *obj->name=="\\sin" || *obj->name=="\\tan" || *obj->name=="\\exp") {
		const TypstForm *tf = kernel.properties.get<TypstForm>(arg);
		if(*arg->multiplier==1)
			if((*arg->name).size()==1 || tf || cadabra::symbols::greekmap.find(*arg->name)!=cadabra::symbols::greekmap.end()) return true;
		}

	if(call_operators.find(*obj->name)!=call_operators.end()) return true;

	return false;
	}

bool DisplayTypst::has_scripts(Ex::iterator it) const
	{
	Ex::sibling_iterator sib=tree.begin(it);
	while(sib!=tree.end(it)) {
		if(sib->fl.parent_rel==str_node::p_sub || sib->fl.parent_rel==str_node::p_super)
			return true;
		++sib;
		}
	return false;
	}

void DisplayTypst::print_other(std::ostream& str, Ex::iterator it)
	{
	if(needs_brackets(it))
		str << "(";

	// print multiplier and object name
	if(*it->multiplier!=1)
		print_multiplier(str, it);

	if(*it->name=="1") {
		if(*it->multiplier==1 || (*it->multiplier==-1)) // this would print nothing altogether.
			str << "1";
		if(needs_brackets(it))
			str << ")";
		return;
		}

	Ex_comparator comp(kernel.properties);
	int num;
	auto prop=kernel.properties.get_with_pattern_ext<TypstForm>(it, comp, num, "", false, true);  // get property, ignore parent rel!
	const TypstForm *tf=prop.first;

	if(tf) {
		// As for LaTeXForm in DisplayTeX: use the pattern as the lhs, and
		// one-by-one the elements in the property as the rhs, e.g.
		//
		//     ket(A??) -> "|"
		//     ket(A??) -> A??
		//     ...
		//
		// Strings are copied verbatim, and the pieces are separated by
		// spaces, which is how Typst separates tokens.

		bool first=true;
		for(auto tt: tf->typst) {
			if(!first) str << " ";
			first=false;
			auto s = *(tt.begin()->name);
			if(s[0]=='\"') {
				s=s.substr(1,s.size()-2);
				str << s;
				if(tf->typst.size()==1)
					print_children(str, it);
				}
			else {
				Ex replacement("\\arrow");
				replacement.append_child(replacement.begin(), prop.second->obj.begin());
				replacement.append_child(replacement.begin(), tt.begin());
				Ex tmp(it);
				// The overall multiplier has already been printed, so set to one now.
				one(tmp.begin()->multiplier);
				substitute subs(kernel, tmp, replacement);
				auto tti=tmp.begin();
				if(subs.can_apply(tti))
					subs.apply(tti);
				dispatch(str, tmp.begin());
				}
			}
		}
	else {
		std::string name = typstify(*it->name);
		// Names like `a13` print as `a_13`, which cannot take any further
		// subscripts; box them so they act as a single base.
		if(name.find('_')!=std::string::npos && has_scripts(it))
			str << "#box($" << name << "$)";
		else
			str << name;
		print_children(str, it);
		}

	if(needs_brackets(it))
		str << ")";
	}

void DisplayTypst::print_tableau(std::ostream& str, Ex::iterator it)
	{
	if(needs_brackets(it))
		str << "(";

	if(*it->multiplier!=1)
		print_multiplier(str, it);

	// Rows are given by the multipliers of the children; the first row is the longest.
	std::vector<long> rows;
	size_t columns=0;
	auto sib=tree.begin(it);
	while(sib!=tree.end(it)) {
		long len = sib->multiplier->get_rational().get_num().get_si();
		rows.push_back(len);
		columns=std::max(columns, (size_t)len);
		++sib;
		}

	// No styling arguments, so that `#set table(...)` and `#show table`
	// rules apply; with the default inset empty cells are square.
	str << "#table(columns: " << columns;
	for(auto len: rows) {
		for(size_t c=0; c<columns; ++c) {
			if((long)c<len) str << ", []";
			else            str << ", table.cell(stroke: none)[]";
			}
		}
	str << ")";

	if(needs_brackets(it))
		str << ")";
	}

void DisplayTypst::print_ftableau(std::ostream& str, Ex::iterator it)
	{
	if(needs_brackets(it))
		str << "(";

	if(*it->multiplier!=1)
		print_multiplier(str, it);

	// Every child is a row; rows with more than one box are \comma nodes.
	std::vector<std::vector<Ex::iterator>> rows;
	size_t columns=0;
	auto sib=tree.begin(it);
	while(sib!=tree.end(it)) {
		std::vector<Ex::iterator> row;
		if(*sib->name!="\\comma") {
			row.push_back(sib);
			}
		else {
			auto sib2=tree.begin(sib);
			while(sib2!=tree.end(sib)) {
				row.push_back(sib2);
				++sib2;
				}
			}
		columns=std::max(columns, row.size());
		rows.push_back(row);
		++sib;
		}

	str << "#table(columns: " << columns;
	for(auto& row: rows) {
		for(size_t c=0; c<columns; ++c) {
			if(c<row.size()) {
				str << ", $";
				dispatch(str, row[c]);
				str << "$";
				}
			else str << ", table.cell(stroke: none)[]";
			}
		}
	str << ")";

	if(needs_brackets(it))
		str << ")";
	}

std::string DisplayTypst::escape_string(const std::string& s)
	{
	std::string res;
	for(auto c: s) {
		if(c=='\\' || c=='"') res+='\\';
		res+=c;
		}
	return res;
	}

std::string DisplayTypst::typstify(std::string str) const
	{
	auto rn = symmap.find(str);
	if(rn!=symmap.end())
		return rn->second;

	if(str.size()==0)
		return "";

	// Floating point numbers stored as names (e.g. from `Ex(2.5e-10)`).
	static const std::regex number("[+-]?(\\d+\\.?\\d*|\\.\\d+)(e[+-]?\\d+)?");
	if(std::regex_match(str, number))
		return std::regex_replace(str, std::regex("(\\d+\\.?\\d*|\\.\\d+)e([+-]?\\d+)"), "$1 times 10^($2)");

	// Make symbols like "a13" print as "a_13" automatically.
	if(str.size()>1) {
		std::string nondigit;
		std::string digit;
		bool nd=true;
		for(size_t i=0; i<str.size(); ++i) {
			if(nd) {
				if(isdigit(str[i])) {
					nd=false;
					digit+=str[i];
					}
				else nondigit+=str[i];
				}
			else {
				if(isdigit(str[i])==false) {
					digit="";
					break; // nothing we can do here
					}
				else digit+=str[i];
				}
			}
		if(digit.size()>0 && nondigit.size()>0)
			return typstify(nondigit)+"_"+digit;
		}

	// Unknown LaTeX commands print upright, without the backslash.
	if(str[0]=='\\' && str.size()>1)
		return "\""+escape_string(str.substr(1))+"\"";

	// Count the number of UTF-8 code points.
	size_t codepoints=0;
	for(unsigned char c: str)
		if((c & 0xC0)!=0x80)
			++codepoints;

	if(codepoints==1) {
		// Escape characters which have a special meaning in Typst math.
		static const std::string special="\\#$_^&\"*/;,~@'`<>|=+-!()[]{}";
		if(str.size()==1 && special.find(str[0])!=std::string::npos)
			return "\\"+str;
		return str;
		}

	bool all_digits=true;
	for(auto c: str)
		if(!isdigit(c))
			all_digits=false;
	if(all_digits)
		return str;

	// Typst interprets multi-letter words as variables, so these need
	// to be quoted. Use italics to match LaTeX output.
	return "italic(\""+escape_string(str)+"\")";
	}

void DisplayTypst::print_children(std::ostream& str, Ex::iterator it, int skip)
	{
	str_node::bracket_t    previous_bracket_   =str_node::b_invalid;
	str_node::parent_rel_t previous_parent_rel_=str_node::p_none;

	int number_of_nonindex_children=0;
	Ex::sibling_iterator ch=tree.begin(it);
	while(ch!=tree.end(it)) {
		if(ch->is_index()==false) {
			++number_of_nonindex_children;
			if(*ch->name=="\\prod")
				++number_of_nonindex_children;
			}
		++ch;
		}

	const Accent *is_accent=kernel.properties.get<Accent>(it);
	bool is_call = is_accent || call_operators.find(*it->name)!=call_operators.end();

	// How the currently open group of children was opened, so that
	// we close it in the same way.
	enum class group_t { script, call, bracket, op };
	group_t group=group_t::op;
	str_node::bracket_t group_bracket=str_node::b_none;

	ch=tree.begin(it);
	ch+=skip;
	while(ch!=tree.end(it)) {
		str_node::bracket_t    current_bracket_   =(*ch).fl.bracket;
		str_node::parent_rel_t current_parent_rel_=(*ch).fl.parent_rel;

		bool function_bracket_needed=true;
		if(current_bracket_==str_node::b_none) {
			if(previous_bracket_==str_node::b_none && current_parent_rel_==previous_parent_rel_ && current_parent_rel_==str_node::p_none)
				str << ", ";
			function_bracket_needed=!reads_as_operator(it, ch);
			}

		if(current_bracket_!=str_node::b_none || previous_bracket_!=current_bracket_ || previous_parent_rel_!=current_parent_rel_) {
			// A script following another script needs a zero-width space to
			// prevent Typst from stacking them on top of each other.
			bool previous_is_script=(ch!=tree.begin(it)) &&
				(previous_parent_rel_==str_node::p_sub || previous_parent_rel_==str_node::p_super);
			if(previous_is_script && (current_parent_rel_==str_node::p_sub || current_parent_rel_==str_node::p_super))
				str << " zws";
			print_parent_rel(str, current_parent_rel_, ch==tree.begin(it));

			if(current_parent_rel_==str_node::p_sub || current_parent_rel_==str_node::p_super) {
				// Typst strips the outer round brackets of a script.
				group=group_t::script;
				group_bracket=current_bracket_;
				str << "(";
				if(group_bracket!=str_node::b_none)
					print_opening_bracket(str, group_bracket, str_node::p_none);
				}
			else if(is_call) {
				group=group_t::call;
				str << "(";
				}
			else if(function_bracket_needed) {
				group=group_t::bracket;
				group_bracket=(number_of_nonindex_children>1 ? str_node::b_round : current_bracket_);
				print_opening_bracket(str, group_bracket, current_parent_rel_);
				}
			else {
				group=group_t::op;
				str << " ";
				}
			}

		// print this child depending on its name or meaning
		dispatch(str, ch);

		++ch;
		if(ch==tree.end(it) || current_bracket_!=str_node::b_none || current_bracket_!=(*ch).fl.bracket || current_parent_rel_!=(*ch).fl.parent_rel) {
			switch(group) {
				case group_t::script:
					if(group_bracket!=str_node::b_none)
						print_closing_bracket(str, group_bracket, str_node::p_none);
					str << ")";
					break;
				case group_t::call:
					str << ")";
					break;
				case group_t::bracket:
					print_closing_bracket(str, group_bracket, current_parent_rel_);
					break;
				case group_t::op:
					str << " ";
					break;
				}
			}
		else str << " ";

		previous_bracket_=current_bracket_;
		previous_parent_rel_=current_parent_rel_;
		}
	}

void DisplayTypst::print_multiplier(std::ostream& str, Ex::iterator it, int mult, bool spacing)
	{
	if(it->multiplier->is_rational()) {
		mpq_class q = it->multiplier->get_rational();
		mpz_class denom=q.get_den();
		mpz_class numer=q.get_num();

		if(denom!=1) {
			if(mult*numer<0) {
				str << " - ";
				mult *= -1;
				}
			str << "frac(" << mult * numer << ", " << denom << ")";
			}
		else if(mult * q==-1) {
			str << "-";
			spacing=false;
			}
		else {
			str << mult * q;
			}
		}
	else {
		std::ostringstream str2;
		str2 << it->multiplier->get_double();
		std::string s = str2.str();
		std::string s2 = std::regex_replace(s, std::regex("(\\d+\\.?\\d*|\\.\\d+)e([+-]?\\d+)"), "$1 times 10^($2)");
		str << s2;
		}
	// No space after a bare number, e.g. in exponents.
	if(spacing && *it->name!="1")
		str << " thin ";
	}

void DisplayTypst::print_opening_bracket(std::ostream& str, str_node::bracket_t br, str_node::parent_rel_t)
	{
	// Typst scales matching brackets automatically; non-round ones
	// are wrapped in `lr` so that commas inside them do not get
	// interpreted as argument separators of an enclosing function.
	switch(br) {
		case str_node::b_none:
		case str_node::b_round:
			str << "(";
			break;
		case str_node::b_pointy:
			str << "lr(⟨";
			break;
		case str_node::b_curly:
			str << "lr(\\{";
			break;
		case str_node::b_square:
			str << "lr([";
			break;
		default :
			return;
		}
	++(bracket_level);
	}

void DisplayTypst::print_closing_bracket(std::ostream& str, str_node::bracket_t br, str_node::parent_rel_t)
	{
	switch(br) {
		case str_node::b_none:
		case str_node::b_round:
			str << ")";
			break;
		case str_node::b_pointy:
			str << "⟩)";
			break;
		case str_node::b_curly:
			str << "\\})";
			break;
		case str_node::b_square:
			str << "])";
			break;
		default :
			return;
		}
	--(bracket_level);
	}

void DisplayTypst::print_parent_rel(std::ostream& str, str_node::parent_rel_t pr, bool)
	{
	switch(pr) {
		case str_node::p_super:
			str << "^";
			break;
		case str_node::p_sub:
			str << "_";
			break;
		case str_node::p_property:
			str << "\\$";
			break;
		case str_node::p_exponent:
			str << "^";
			break;
		case str_node::p_none:
			break;
		case str_node::p_components:
			break;
		case str_node::p_invalid:
			throw std::logic_error("DisplayTypst: p_invalid not handled.");
		}
	}

void DisplayTypst::dispatch(std::ostream& str, Ex::iterator it)
	{
	if(handle_unprintable_wildcards(str, it))
		return;

	if(std::holds_alternative<std::shared_ptr<NTensor>>(it->content)) {
		std::ostringstream str2;
		auto nt = std::get<std::shared_ptr<NTensor>>(it->content);
		str2 << *nt;
		std::string s = str2.str();
		std::string s2 = std::regex_replace(s, std::regex("(\\d+\\.?\\d*|\\.\\d+)e([+-]?\\d+)"), "$1 times 10^($2)");
		if(nt->shape.size()==1 && nt->shape[0]==1) {
			if(!nt->is_real())
				str << "(" << s2.substr(1, s2.size()-2) << ")";
			else
				str << s2.substr(1, s2.size()-2);
			}
		else
			str << "lr(" << s2 << ")";

		if(*it->name=="1" && tree.number_of_children(it)==0)
			return;
		}
	else if(std::holds_alternative<std::shared_ptr<NInterpolatingFunction>>(it->content)) {
		str << "square";
		auto ni = std::get<std::shared_ptr<NInterpolatingFunction>>(it->content);
		str << "(";
		dispatch(str, ni->var.begin());
		str << ")";
		return;
		}

	if(*it->name=="\\prod")                                   print_productlike(str, it, "dot.op");
	else if(*it->name=="\\sum" || *it->name=="\\oplus")       print_sumlike(str, it);
	else if(*it->name=="\\frac")                              print_fraclike(str, it);
	else if(*it->name=="\\comma")                             print_commalike(str, it);
	else if(*it->name=="\\arrow")                             print_arrowlike(str, it);
	else if(*it->name=="\\inner")                             print_dot(str, it);
	else if(*it->name=="\\pow")                               print_powlike(str, it);
	else if(*it->name=="\\int")                               print_intlike(str, it);
	else if(*it->name=="\\equals" || *it->name=="\\unequals") print_equalitylike(str, it);
	else if(*it->name=="\\commutator")                        print_commutator(str, it, true);
	else if(*it->name=="\\anticommutator")                    print_commutator(str, it, false);
	else if(*it->name=="\\components")                        print_components(str, it);
	else if(*it->name=="\\wedge")                             print_wedgeproduct(str, it);
	else if(*it->name=="\\conditional")                       print_conditional(str, it);
	else if(*it->name=="\\greater" || *it->name=="\\less")    print_relation(str, it);
	else if(*it->name=="\\indexbracket")                      print_indexbracket(str, it);
	else if(*it->name=="\\ldots")                             print_dots(str, it);
	else if(kernel.properties.get<Tableau>(it))               print_tableau(str, it);
	else if(kernel.properties.get<FilledTableau>(it))         print_ftableau(str, it);
	else                                                      print_other(str, it);
	}

void DisplayTypst::print_commalike(std::ostream& str, Ex::iterator it)
	{
	Ex::sibling_iterator sib=tree.begin(it);
	bool first=true;
	str << "lr([";
	while(sib!=tree.end(it)) {
		if(first)
			first=false;
		else
			str << ", ";
		dispatch(str, sib);
		++sib;
		}
	str << "])";
	}

void DisplayTypst::print_wedgeproduct(std::ostream& str, Ex::iterator it)
	{
	if(*it->multiplier!=1) {
		print_multiplier(str, it);
		}

	if(needs_brackets(it))
		str << "(";

	Ex::sibling_iterator sib=tree.begin(it);
	dispatch(str, sib);
	++sib;
	while(sib!=tree.end(it)) {
		str << " ∧ ";
		dispatch(str, sib);
		++sib;
		}

	if(needs_brackets(it))
		str << ")";
	}

void DisplayTypst::print_arrowlike(std::ostream& str, Ex::iterator it)
	{
	Ex::sibling_iterator sib=tree.begin(it);
	dispatch(str, sib);
	str << " -> ";
	++sib;
	dispatch(str, sib);
	}

void DisplayTypst::print_dot(std::ostream& str, Ex::iterator it)
	{
	Ex::sibling_iterator sib=tree.begin(it);
	dispatch(str, sib);
	str << " dot.op ";
	++sib;
	dispatch(str, sib);
	}

void DisplayTypst::print_fraclike(std::ostream& str, Ex::iterator it)
	{
	Ex::sibling_iterator num=tree.begin(it), den=num;
	++den;

	int mult=1;
	if(*it->multiplier<0) {
		str << " - ";
		mult=-1;
		}

	bool unit_multiplier = it->multiplier->is_rational() && (mult * (it->multiplier->get_rational()))==1;
	bool print_num = num->is_rational()==false || unit_multiplier;

	str << "frac(";
	if(!unit_multiplier) {
		if(print_num) {
			print_multiplier(str, it, mult);
			}
		else {
			// The multiplier replaces the (rational) numerator, so it
			// should not be followed by a space.
			print_multiplier(str, it, mult, false);
			}
		}

	if(print_num)
		dispatch(str, num);
	str << ", ";
	dispatch(str, den);
	str << ")";
	}

void DisplayTypst::print_productlike(std::ostream& str, Ex::iterator it, const std::string& inbetween)
	{
	if(needs_brackets(it))
		str << "(";

	if (kernel.display_fractions) {
		// If one (or more) of the factors is a negative power, split into top and
		// bottom parts and print as a fraction
		Ex pos("\\prod"), neg("\\prod");
		for (Ex::sibling_iterator beg = it.begin(), end = it.end(); beg != end; ++beg) {
			bool is_negexp = false;
			if (*beg->name == "\\pow") {
				Ex::sibling_iterator exponent = beg.begin();
				++exponent;
				if (*exponent->name == "1" && *exponent->multiplier < 0) {
					is_negexp = true;
					if (*exponent->multiplier == -1) {
						neg.append_child(neg.begin(), (Ex::iterator)beg.begin());
						}
					else {
						auto pos = neg.append_child(neg.begin(), (Ex::iterator)beg);
						exponent = pos.begin();
						++exponent;
						multiply(exponent->multiplier, -1);
						}
					}
				}
			if (!is_negexp) {
				pos.append_child(pos.begin(), (Ex::iterator)beg);
				}
			}

		if (neg.begin().begin() != neg.begin().end()) {
			auto mult = *it->multiplier;
			if (mult < 0) {
				str << "-";
				mult *= -1;
				}
			if (mult.is_rational()) {
				auto rmult = mult.get_rational();
				if(mult.get_rational().get_den() == 1) {
					multiply(pos.begin()->multiplier, rmult);
					}
				else {
					multiply(pos.begin()->multiplier, rmult.get_num());
					multiply(neg.begin()->multiplier, rmult.get_den());
					}
				}
			else {
				multiply(pos.begin()->multiplier, mult);
				}

			str << "frac(";
			if (pos.begin().begin() == pos.begin().end()) {
				pos.begin()->name = name_set.insert("1").first;
				print_other(str, pos.begin());
				}
			else {
				print_productlike(str, pos.begin(), inbetween);
				}
			str << ", ";
			print_productlike(str, neg.begin(), inbetween);
			str << ")";

			if (needs_brackets(it))
				str << ")";
			return;
			}
		}

	// The multiplier needs to be inside the brackets, otherwise things like
	// \pow{ 2/3 \prod{a}{b} }{c} do not print correctly.

	if(*it->multiplier!=1)
		print_multiplier(str, it);

	str_node::bracket_t previous_bracket_=str_node::b_invalid;
	Ex::sibling_iterator ch=tree.begin(it);
	bool prev_is_tableau=false;
	if(ch!=tree.end(it)) {
		const Tableau        *tab =kernel.properties.get<Tableau>(ch);
		const FilledTableau  *ftab=kernel.properties.get<FilledTableau>(ch);
		if(tab || ftab)
			prev_is_tableau=true;
		}
	while(ch!=tree.end(it)) {
		str_node::bracket_t current_bracket_=(*ch).fl.bracket;
		if(previous_bracket_!=current_bracket_) {
			if(current_bracket_!=str_node::b_none) {
				print_opening_bracket(str, current_bracket_, str_node::p_none);
				}
			}
		dispatch(str, ch);
		++ch;
		if(ch==tree.end(it)) {
			if(current_bracket_!=str_node::b_none)
				print_closing_bracket(str, current_bracket_, str_node::p_none);
			}
		else {
			const Tableau       *tab =kernel.properties.get<Tableau>(ch);
			const FilledTableau *ftab=kernel.properties.get<FilledTableau>(ch);
			if(tab || ftab) {
				if(prev_is_tableau)
					str << " ⊗ ";
				else
					str << " ";
				prev_is_tableau=true;
				}
			else {
				prev_is_tableau=false;
				if(print_star) {
					if(tight_star) str << inbetween;
					else str << " " << inbetween << " ";
					}
				else {
					str << " ";
					}
				}
			}
		previous_bracket_=current_bracket_;
		}

	if(needs_brackets(it))
		str << ")";
	}

void DisplayTypst::print_sumlike(std::ostream& str, Ex::iterator it)
	{
	assert(*it->multiplier==1);

	if(needs_brackets(it))
		str << "(";

	Ex::sibling_iterator ch=tree.begin(it);
	bool prev_is_tableau=false;
	if(ch!=tree.end(it)) {
		const Tableau        *tab =kernel.properties.get<Tableau>(ch);
		const FilledTableau  *ftab=kernel.properties.get<FilledTableau>(ch);
		if(tab || ftab)
			prev_is_tableau=true;
		}
	while(ch!=tree.end(it)) {
		if(*ch->multiplier>=0 && ch!=tree.begin(it)) {
			if(*it->name=="\\sum") {
				const Tableau       *tab =kernel.properties.get<Tableau>(ch);
				const FilledTableau *ftab=kernel.properties.get<FilledTableau>(ch);
				if(tab || ftab) {
					if(prev_is_tableau)
						str << " ⊕ ";
					else
						str << " + ";
					prev_is_tableau=true;
					}
				else
					str << " + ";
				}
			else
				str << " " << typstify(*it->name) << " ";
			}
		else if(ch!=tree.begin(it))
			str << " ";

		dispatch(str, ch);
		++ch;
		}

	if(needs_brackets(it))
		str << ")";
	str << std::flush;
	}

bool DisplayTypst::handle_unprintable_wildcards(std::ostream& str, Ex::iterator it) const
	{
	// Catch `\pow{#}` and other wildcard constructions, these
	// need to print verbatim.

	if(it.number_of_children()==1) {
		const std::string& name = (*it->name);
		if(name.size()>0 && name[0]=='\\' && *(it.begin()->name)=="#") {
			str << "mono(\"" << escape_string(name) << "{#}\")";
			return true;
			}
		}
	return false;
	}

void DisplayTypst::print_powlike(std::ostream& str, Ex::iterator it)
	{
	auto arg=tree.begin(it);
	assert(arg!=tree.end(it));
	auto exp=arg;
	++exp;

	assert(exp!=tree.end(it));

	if (kernel.display_fractions && exp->is_rational() && *exp->multiplier < 0) {
		auto mult = *it->multiplier;
		bool mult_is_int = mult.is_rational() && (mult.get_rational().get_den() == 1);
		if (mult < 0) {
			str << "-";
			mult *= -1;
			}
		str << "frac(";
		if (mult_is_int)
			str << mult;
		else {
			if(mult.is_rational())
				str << mult.get_rational().get_num();
			else
				str << mult;
			}
		str << ", ";
		if (*exp->multiplier == -1) {
			Ex copy(arg);
			if(mult.is_rational()) {
				if (!mult_is_int) {
					multiply(copy.begin()->multiplier, mult.get_rational().get_den());
					}
				}
			else multiply(copy.begin()->multiplier, mult);

			dispatch(str, copy.begin());
			}
		else {
			Ex copy(it);
			exp = copy.begin().begin();
			++exp;
			multiply(exp->multiplier, -1);
			if(mult.is_rational()) {
				if (!mult_is_int)
					copy.begin()->multiplier = rat_set.insert(mult.get_rational().get_den()).first;
				}
			else copy.begin()->multiplier = rat_set.insert(mult).first;

			print_powlike(str, copy.begin());
			}
		str << ")";
		return;
		}

	if(*it->multiplier!=1)
		print_multiplier(str, it);

	if(exp->is_rational() && *exp->multiplier==multiplier_t(1)/2) {
		str << "sqrt(";
		dispatch(str, arg);
		str << ")";
		return;
		}

	dispatch(str, arg);
	// Prevent the exponent from getting stacked on top of a subscript.
	if(has_scripts(arg))
		str << " zws";
	str << "^(";
	dispatch(str, exp);
	str << ")";
	}

void DisplayTypst::print_intlike(std::ostream& str, Ex::iterator it)
	{
	if(*it->multiplier!=1)
		print_multiplier(str, it);
	std::string name = typstify(*it->name);
	str << name;

	// The first argument is the integrand. Subsequent arguments are
	// either integration variables, or lists consisting of an
	// integration variable, a start value and an end value.
	// Since the integration ranges need to be attached to the
	// integral symbols, we need to scan for them first.

	auto sib=tree.begin(it);
	++sib;
	while(sib!=tree.end(it)) {
		if(*sib->name=="\\comma") {
			auto bvalue = tree.child(sib, 1);
			auto evalue = tree.child(sib, 2);
			str << "_(";
			dispatch(str, bvalue);
			str << ")^(";
			dispatch(str, evalue);
			str << ")";
			}
		++sib;
		if(sib!=tree.end(it))
			str << " " << name;
		}

	str << " ";
	sib=tree.begin(it);
	dispatch(str, sib);
	++sib;

	while(sib!=tree.end(it)) {
		str << " dif ";
		if(*sib->name=="\\comma") {
			dispatch(str, tree.child(sib,0));
			}
		else {
			dispatch(str, sib);
			}
		++sib;
		}
	}

void DisplayTypst::print_equalitylike(std::ostream& str, Ex::iterator it)
	{
	Ex::sibling_iterator sib=tree.begin(it);
	dispatch(str, sib);
	if(*it->name=="\\unequals") str << " != ";
	else                        str << " = ";
	++sib;
	if(sib==tree.end(it))
		throw ConsistencyException("Found equals node with only one child node.");
	dispatch(str, sib);
	}

void DisplayTypst::print_commutator(std::ostream& str, Ex::iterator it, bool comm)
	{
	if(*it->multiplier!=1)
		print_multiplier(str, it);

	if(comm) str << "lr([";
	else     str << "lr(\\{";
	auto sib=tree.begin(it);
	bool first=true;
	while(sib!=tree.end(it)) {
		if(!first) str << ", ";
		else       first=false;
		dispatch(str, sib);
		++sib;
		}
	if(comm) str << "])";
	else     str << "\\})";
	}

void DisplayTypst::print_dots(std::ostream& str, Ex::iterator it)
	{
	if(tree.is_head(it)==false) {
		if(*tree.parent(it)->name=="\\sum")
			str << " dots.h ";
		else
			str << " dots.c ";
		}
	else
		str << " dots.h ";
	}

void DisplayTypst::print_components(std::ostream& str, Ex::iterator it)
	{
	assert(*it->multiplier==1);

	auto ind_names=tree.begin(it);
	auto ind_values=tree.end(it);
	--ind_values;

	// Print a sequence of indices attached to a square, with
	// zero-width spaces to keep them in order.
	auto print_indices = [this, &str](Ex::sibling_iterator in, Ex::sibling_iterator iv, Ex::sibling_iterator iv_end) {
		str << "square";
		bool first=true;
		while(iv!=iv_end) {
			if(!first) str << " zws";
			first=false;
			if(in->fl.parent_rel==str_node::p_sub)   str << "_(";
			if(in->fl.parent_rel==str_node::p_super) str << "^(";
			dispatch(str, iv);
			str << ")";
			++in;
			++iv;
			}
		};

	print_indices(ind_names, ind_names, ind_values);

	// Find duplicates.
	equiv_fun_t component_equiv = [](const Ex& ex, Ex::iterator it1, Ex::iterator it2) {
		it1 = ex.child(it1, 1);
		it2 = ex.child(it2, 1);
		return ex.equal_subtree(it1, it2);
		};
	auto grp = group_by_equivalence(tree, ind_values, component_equiv);

	// Function to display anonymous tensor.
	auto display_anon = [this, &ind_names, &print_indices](Ex::sibling_iterator c) {
		print_indices(ind_names, tree.begin(c), tree.end(c));
		};

	// Display all unique components, one per row of a matrix,
	// aligned on the first equals sign.
	str << " mat(delim: \"{\"";
	auto sib=tree.begin(ind_values);
	while(sib!=tree.end(ind_values)) {
		if( grp.find( sib ) == grp.end() ) {
			str << (sib==tree.begin(ind_values) ? ", " : "; ");
			Ex::sibling_iterator c=tree.begin(sib);
			display_anon(c);
			str << " &= ";

			for(const auto& equal_tensor: grp) {
				if( equal_tensor.second.second == sib ) {
					display_anon(tree.begin(equal_tensor.first));
					str << " = ";
					}
				}

			++c;
			dispatch(str, c);
			}
		++sib;
		}
	str << ")";
	}

void DisplayTypst::print_conditional(std::ostream& str, Ex::iterator it)
	{
	auto sib=tree.begin(it);
	dispatch(str, sib);
	str << " quad \"with\" quad ";
	++sib;
	dispatch(str, sib);
	}

void DisplayTypst::print_relation(std::ostream& str, Ex::iterator it)
	{
	auto sib=tree.begin(it);
	dispatch(str, sib);
	if(*it->name=="\\greater") str << " > ";
	if(*it->name=="\\less")    str << " < ";
	++sib;
	dispatch(str, sib);
	}

void DisplayTypst::print_indexbracket(std::ostream& str, Ex::iterator it)
	{
	if(*it->multiplier!=1)
		print_multiplier(str, it);

	auto sib=tree.begin(it);
	str << "(";
	dispatch(str, sib);
	str << ")";
	print_children(str, it, 1);
	}
