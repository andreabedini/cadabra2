
#pragma once

#include <memory>

#include <gtkmm/application.h>
#include <gtkmm/grid.h>
#include "NotebookWindow.hh"
#include "ComputeThread.hh"
#include "ScriptThread.hh"

/// The Cadabra notebook application.

class Cadabra : public Gtk::Application {
	public:
		/// Note: argc/argv are not passed here. GTK4 removed the
		/// Gtk::Application(argc, argv, ...) constructor, so they go to
		/// run(argc, argv) in main() instead, which works on both toolkits.
		static Glib::RefPtr<Cadabra> create();

		bool open_help(const std::string& filename, const std::string& title);

	protected:
		Cadabra();
		virtual ~Cadabra();

		virtual void on_startup() override;
		virtual void on_activate() override;
		virtual void on_open(const Gio::Application::type_vec_files& files, const Glib::ustring& hint) override;
		virtual int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine>& cmd) override;

	private:
		// Destroyed in ~Cadabra in a specific order (stop, join, delete),
		// so that no thread outlives the object it runs on.
		std::unique_ptr<cadabra::ComputeThread> compute;
		std::unique_ptr<std::thread>            compute_thread;

		std::unique_ptr<cadabra::ScriptThread>  script;
		std::unique_ptr<std::thread>            script_thread;

		int server_port;
		std::string server_token;
		std::string server_ip_address;
		std::string window_geometry;
		std::string window_title;
		bool        no_registration;
	};
