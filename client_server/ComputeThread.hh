
#pragma once

#include <signal.h>
#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <set>
#include <glibmm/spawn.h>
#include "websocket_client.hh"

#ifdef _WIN32
// Boost.Asio (via websocket_client.hh) pulls in <windows.h>, which defines
// macros that clash with enum member names in gtkmm4
// (Gio::Converter::Result::ERROR, Gtk::MessageType::ERROR,
// Gdk::TitlebarGesture::DOUBLE_CLICK, Gsk::BlendMode::DIFFERENCE,
// Gsk::FillRule::WINDING, ...). Nothing in Cadabra uses these macros, so
// drop them here; websocket_client.hh itself is kept as upstream ships it.
#undef ERROR
#undef DIFFERENCE
#undef DOUBLE_CLICK
#undef WINDING
#endif

#include "DataCell.hh"

namespace cadabra {

	class GUIBase;
	class DocumentThread;

	/// \ingroup clientserver
	///
	/// Base class which talks to the server and sends Action objects back to the
	/// DocumentThread.
	///
	/// ComputeThread is the base class which takes care of doing actual
	/// computations with the cells in a document. It handles talking to
	/// the server backend. It knows how to pass cells to the server and
	/// ask them to be executed. Results are reported back to the GUI by
	/// putting ActionBase objects onto its todo stack. ComputeThread never
	/// directly modifies the document tree.

	class ComputeThread {
		public:
			/// If the ComputeThread is constructed with a null pointer to the
			/// gui, there will be no gui updates, just DTree updates.

			ComputeThread(int server_port=0, std::string token="", std::string ip_address="127.0.0.1");
			ComputeThread(const ComputeThread& )=delete; // You cannot copy this object
			~ComputeThread();

			/// Determine the objects that this compute thread should be
			/// talking to.
			void set_master(GUIBase *, DocumentThread *);

			/// Main entry point, which will connect to the server and
			/// then start an event loop to handle communication with the
			/// server. Only terminates when the connection drops, so run
			/// your GUI on a different thread.

			void run();

			/// In order to execute code on the server, call the
			/// following from the GUI thread.  This method returns as
			/// soon as the request has been put on the network queue. If
			/// no communication with the server is necessary, this
			/// returns immediately. The ComputeThread will report the
			/// result of the computation/processing by adding actions to
			/// the DocumentThread owned pending_actions stack, by
			/// calling queue_action. It will never modify the cell
			/// directly, and will also never modify any other cells in
			/// the document tree.

			void execute_cell(DTree::iterator, std::string no_assign="",
									std::vector<uint64_t> output_cell_ids=std::vector<uint64_t>() );

			void execute_interactive(uint64_t id, const std::string& code);

			/// Update a variable in the kernel. This does essentially the
			/// same thing as execute_cell, but will not refer to any
			/// cell as it creates the code itself.

			void update_variable_on_server(std::string variable, double value);
			
			/// Stop the current cell execution on the server and remove
			/// all other cells from the run queue as well.

			void stop();

			/// Restart the kernel.

			void restart_kernel();

			/// Request completion of a string. Returns `false` if the current
			/// cell cannot be completed, in which case the TAB which led to
			/// the request should be interpreted literally and used for spacing.
			/// The `alternative` argument is the serial number of the requested
			/// completion, in case there is more than one possible completion.

			bool complete(DTree::iterator, int pos, int alternative);

			// Determine if there are still cells running on the server.
			// FIXME: this does not guarantee thread-safety but at the moment
			// is only used for updating status bars etc.
			// FIXME: can be moved to DocumentThread.

			int number_of_cells_executing(void) const;

			/// Terminate the compute thread, in preparation for shutting
			/// down the client altogether. Makes run() return; the caller
			/// should then join the thread. A server we started ourselves
			/// is killed when this object is destroyed.

			void terminate();

			/// Return the status of the connection to the kernel.
			bool kernel_is_connected() const;
			
		private:
			GUIBase        *gui;
			DocumentThread *docthread;

			// For debugging purposes, we keep record of the gui thread id,
			// so that we can flag when code runs on the wrong thread.
			// Gets initialised in the ComputeThread constructor.
			std::thread::id  gui_thread_id;

			// Keeping track of cells which are running on the server, in
			// a form which allows us to look them up quickly based only
			// on the id (which is all that the server knows about). A
			// cell can be queued multiple times for running; this is the
			// integer in the map.

			//
			// Threading: the GUI thread calls execute_cell, complete, stop
			// etc., while the websocket handlers (on_open, on_message, ...)
			// run on the thread which calls run(). State shared between
			// the two is either atomic or guarded by `state_mutex`. Never
			// call into the GUI or DocumentThread while holding it.

			mutable std::mutex            state_mutex;
			std::map<DataCell::id_t, int> running_cells;        // guarded by state_mutex

			websocket_client              wsclient;
			std::atomic<bool>             connection_is_open, restarting_kernel;
			int                           consecutive_failures; // network thread only

			/// Snapshot of the authentication token of the current server.
			std::string auth_token() const;
			/// Send a request to the server, logging it if CADABRA_SHOW_SENT is set.
			void send_request(const nlohmann::json& req);

			void init();
			void try_connect();
			void try_spawn_server();
			void on_open();
			void on_fail(const boost::beast::error_code&);
			void on_close();
			void on_message(const std::string& msg);
			void handle_message(const std::string& msg);

			void cell_finished_running(DataCell::id_t);

			/// Set all cells to be non-running (e.g. after a kernel failure) and
			/// report the status of each cell to the GUI.
			void all_cells_nonrunning();

			std::set<uint64_t> interactive_cells;  // guarded by state_mutex

			/// Terminate the server we spawned ourselves, if any, and release
			/// its handles. Safe to call repeatedly, and a no-op when we did not
			/// start the server (--server-port), so we never kill someone
			/// else's process.
			void kill_server();

			// Self-started server. Only touched on the network thread, and
			// in the destructor after that thread has been joined; except
			// for `port` and `authentication_token`, which are also read on
			// the GUI thread and therefore guarded by state_mutex.
			Glib::Pid       server_pid;
			int             server_stdout, server_stderr;
			FILE           *server_stdout_file;
			unsigned short  port;
			std::string     authentication_token;
			int             forced_server_port;
			std::string     forced_server_token;
			std::string     forced_server_ip_address;
		};

	}
