#include "Config.hh"
#include <iostream>
#include <vector>
#include "cadabra-jupyter-kernel.hh"
#include "CdbPython.hh"
#include "xeus/xhelper.hpp"
#include <boost/algorithm/string.hpp>

using namespace cadabra;

namespace {

	// Jupyter positions count unicode code points; our strings are UTF-8.

	size_t codepoints_to_bytes(const std::string& s, size_t cp)
		{
		size_t i=0;
		while(i<s.size() && cp>0) {
			++i;
			while(i<s.size() && (static_cast<unsigned char>(s[i]) & 0xC0)==0x80)
				++i;
			--cp;
			}
		return i;
		}

	size_t bytes_to_codepoints(const std::string& s, size_t bytes)
		{
		size_t cp=0;
		for(size_t i=0; i<bytes && i<s.size(); ++i)
			if((static_cast<unsigned char>(s[i]) & 0xC0)!=0x80)
				++cp;
		return cp;
		}

	// Split an error message as produced by Server::run_string, e.g.
	// "NameError: name 'x' is not defined\n...", into name, value and
	// traceback lines.
	void split_error(const std::string& msg, std::string& ename, std::string& evalue,
						  std::vector<std::string>& traceback)
		{
		boost::split(traceback, msg, boost::is_any_of("\n"));
		const std::string first = traceback.empty() ? std::string() : traceback[0];
		if(first.rfind("Interrupted", 0)==0) {
			ename  = "KeyboardInterrupt";
			evalue = first;
			return;
			}
		auto colon = first.find(':');
		std::string name = first.substr(0, colon);
		bool is_identifier = colon!=std::string::npos && !name.empty()
			&& name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")==std::string::npos;
		if(is_identifier) {
			ename  = name;
			evalue = boost::trim_copy(first.substr(colon+1));
			}
		else {
			ename  = "Error";
			evalue = first;
			}
		}

	}

CadabraJupyter::CadabraJupyter()
	: Server()
	{
	// The Server constructor has set up Python, including the hooks which
	// make `display` call our `send`. Everything runs on the thread which
	// handles the shell requests, so there is no block-running thread.
	}

void CadabraJupyter::configure_impl()
	{
	}

void CadabraJupyter::flush_output()
	{
	std::string out = catchOut.str();
	catchOut.clear();
	std::string err = catchErr.str();
	catchErr.clear();
	if(!publishing) return;
	if(!out.empty()) publish_stream("stdout", out);
	if(!err.empty()) publish_stream("stderr", err);
	}

nl::json CadabraJupyter::execute_request_impl(int execution_counter,
															 const std::string& code,
															 bool silent,
															 bool /* store_history */,
															 nl::json /* user_expressions */,
															 bool /* allow_stdin */)
	{
	publishing = !silent;
	current_id = execution_counter;

	try {
		// Do not let run_string collect stdout/stderr (it would print the
		// latter to our own stderr); flush_output publishes both.
		run_string(code, false);
		flush_output();
		return xeus::create_successful_reply();
		}
	catch(std::exception& ex) {
		// Server::run_string restores the Python error before rethrowing
		// (see the comment there), so it is still pending; clear it, or
		// the next call into Python raises it again.
		PyErr_Clear();
		flush_output();
		std::string ename, evalue;
		std::vector<std::string> traceback;
		split_error(ex.what(), ename, evalue, traceback);
		if(publishing)
			publish_execution_error(ename, evalue, traceback);
		// Built by hand: xeus 3.1's create_error_reply swaps ename and evalue.
		nl::json reply;
		reply["status"]    = "error";
		reply["ename"]     = ename;
		reply["evalue"]    = evalue;
		reply["traceback"] = traceback;
		return reply;
		}
	}

uint64_t CadabraJupyter::send(const std::string& output, const std::string& msg_type,
										uint64_t /* parent_id */, uint64_t /* cell_id */, bool /* last */)
	{
	// Anything printed before this display call should appear before it.
	flush_output();
	if(!publishing || output.empty())
		return current_id;

	nl::json data;
	if(msg_type=="latex_view") {
		// Cadabra wraps display maths in dmath*; Jupyter frontends render
		// standard $$...$$ through MathJax.
		std::string tex = output;
		boost::replace_all(tex, "\\begin{dmath*}", "$$");
		boost::replace_all(tex, "\\end{dmath*}", "$$");
		// Line-breaking hints for the LaTeX renderer; MathJax does not know
		// \discretionary. (A build with ENABLE_JUPYTER does not emit them,
		// but do not depend on that.)
		boost::replace_all(tex, "\\discretionary{}{}{}", "");
		boost::trim(tex);
		data["text/latex"] = tex;
		data["text/plain"] = tex;
		}
	else if(msg_type=="image_png") {
		data["image/png"]  = output; // already base64 encoded
		data["text/plain"] = "<image>";
		}
	else if(msg_type=="image_svg") {
		data["image/svg+xml"] = output; // base64 encoded
		data["text/plain"]    = "<image>";
		}
	else {
		// verbatim, output, input_form and anything else.
		data["text/plain"] = output;
		}
	display_data(std::move(data), nl::json::object(), nl::json::object());
	return current_id;
	}

nl::json CadabraJupyter::complete_request_impl(const std::string& code, int cursor_pos)
	{
	// Complete the token in front of the cursor, using the same Python
	// completer as the other frontends.
	static const std::string delimiters="\n(){}[]:\t ,=+-*/";
	size_t end = codepoints_to_bytes(code, cursor_pos>0 ? cursor_pos : 0);
	size_t start = end;
	while(start>0 && delimiters.find(code[start-1])==std::string::npos)
		--start;
	std::string token = code.substr(start, end-start);

	nl::json matches = nl::json::array();
	if(!token.empty()) {
		try {
			pybind11::object completer =
				main_namespace["__cdbkernel__"].attr("completer");
			for(int i=0; i<500; ++i) {
				pybind11::object res = completer.attr("complete")(token, i);
				if(res.is_none()) break;
				matches.push_back(res.cast<std::string>());
				}
			}
		catch(std::exception&) {
			// No completions; never let an exception escape to xeus (it
			// would not send a reply at all).
			PyErr_Clear();
			}
		}
	return xeus::create_complete_reply(matches,
												  bytes_to_codepoints(code, start),
												  bytes_to_codepoints(code, end));
	}

nl::json CadabraJupyter::inspect_request_impl(const std::string& /* code */,
															 int /* cursor_pos */,
															 int /* detail_level */)
	{
	return xeus::create_inspect_reply(false);
	}

nl::json CadabraJupyter::is_complete_request_impl(const std::string& code)
	{
	// Check the Python code which the Cadabra input turns into. Never let
	// an exception escape to xeus: it would not send a reply at all.
	try {
		std::string error;
		std::string pycode = cdb2python_string(code, true, error);
		if(!error.empty())
			return xeus::create_is_complete_reply("invalid");
		// compile_command wants no trailing newline on the last line.
		while(!pycode.empty() && pycode.back()=='\n')
			pycode.pop_back();
		switch(is_python_code_complete(pycode, error)) {
			case 1:  return xeus::create_is_complete_reply("complete");
			case 0:  return xeus::create_is_complete_reply("incomplete", "    ");
			default: return xeus::create_is_complete_reply("invalid");
			}
		}
	catch(std::exception&) {
		PyErr_Clear();
		return xeus::create_is_complete_reply("unknown");
		}
	}

nl::json CadabraJupyter::kernel_info_request_impl()
	{
	return xeus::create_info_reply(
		"5.3",                          // protocol version
		"cadabra_xeus_kernel",          // implementation
		std::string(CADABRA_VERSION_SEM),
		"cadabra2",                     // language name
		std::string(CADABRA_VERSION_SEM),
		"text/cadabra2",                // mimetype
		".cdb",                         // file extension
		"cadabra",                      // pygments lexer
		"cadabra",                      // codemirror mode
		"python",                       // nbconvert exporter
		"Cadabra " + std::string(CADABRA_VERSION_SEM) + " (native xeus kernel)\n"
		"More info at https://cadabra.science/");
	}

void CadabraJupyter::shutdown_request_impl()
	{
	}
