
#include "properties/TypstForm.hh"

using namespace cadabra;

std::string TypstForm::name() const
	{
	return "TypstForm";
	}

std::string TypstForm::unnamed_argument() const
	{
	return "typst";
	}

bool TypstForm::parse(Kernel&, keyval_t& keyvals)
	{
	for(const auto& kv: keyvals)
		typst.push_back(kv.second);
	return true;
	}

std::string TypstForm::typst_form() const
	{
	return *(typst.begin()->begin()->name);
	}
