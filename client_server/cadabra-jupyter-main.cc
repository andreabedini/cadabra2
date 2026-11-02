// Entry point of the native (xeus-based) Cadabra Jupyter kernel.
//
// Jupyter starts it as `cadabra-jupyter-kernel -f <connection_file>`
// (see kernel.json).

#include <iostream>
#include <memory>
#include <string>

#include "cadabra-jupyter-kernel.hh"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"
#include "xeus-zmq/xzmq_context.hpp"
#include "xeus-zmq/xserver_shell_main.hpp"

int main(int argc, char* argv[])
	{
	std::string file_name;
	for(int i=1; i<argc; ++i) {
		std::string arg(argv[i]);
		if((arg=="-f" || arg=="--connection-file") && i+1<argc)
			file_name=argv[++i];
		}
	if(file_name.empty()) {
		std::cerr << "Usage: " << argv[0] << " -f <connection_file>" << std::endl;
		return 1;
		}

	xeus::xconfiguration config = xeus::load_configuration(file_name);

	// The interpreter owns the embedded Python interpreter (through
	// Server), so create it here, on the main thread. The shell requests,
	// which run Python code, are then handled on this same thread
	// (make_xserver_shell_main), so that Python signal handling, and with
	// it kernel interrupts, works.
	auto interpreter = std::make_unique<cadabra::CadabraJupyter>();

	xeus::xkernel kernel(config,
								xeus::get_user_name(),
								xeus::make_zmq_context(),
								std::move(interpreter),
								xeus::make_xserver_shell_main);
	kernel.start();

	return 0;
	}
