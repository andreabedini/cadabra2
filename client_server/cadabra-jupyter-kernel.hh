#pragma once

// Native Jupyter kernel for Cadabra, built on xeus (3.x) and xeus-zmq.
//
// This is an alternative to the Python-based kernel in jupyterkernel/.
// It embeds the same Python-side Cadabra machinery as cadabra-server (by
// deriving from Server), but talks the Jupyter protocol instead of the
// Cadabra websocket protocol.
//
// All requests are handled synchronously on the kernel's shell thread,
// which is also the thread holding the Python interpreter.

#include <string>

#include "xeus/xinterpreter.hpp"
#include "Server.hh"

namespace cadabra {

	class CadabraJupyter : public xeus::xinterpreter, public Server {
		public:
			CadabraJupyter();
			virtual ~CadabraJupyter() = default;

			/// Called from the Python side (via `display`) to send output;
			/// publishes it as Jupyter display data for the current request.
			virtual uint64_t send(const std::string& output, const std::string& msg_type,
										 uint64_t parent_id=0, uint64_t cell_id=0,
										 bool last_in_sequence=false) override;

		private:
			void configure_impl() override;

			nl::json execute_request_impl(int execution_counter,
													const std::string& code,
													bool silent,
													bool store_history,
													nl::json user_expressions,
													bool allow_stdin) override;

			nl::json complete_request_impl(const std::string& code,
													 int cursor_pos) override;

			nl::json inspect_request_impl(const std::string& code,
													int cursor_pos,
													int detail_level) override;

			nl::json is_complete_request_impl(const std::string& code) override;

			nl::json kernel_info_request_impl() override;

			void shutdown_request_impl() override;

			/// Publish whatever Python wrote to stdout/stderr so far.
			void flush_output();

			/// Whether output should be published (false for silent requests).
			bool publishing=true;
		};

	}
