#include "websocket_server.hh"
#include <iostream>

websocket_server::connection::connection(boost::asio::io_context& ioc, websocket_server& server, id_type id)
	: socket_(ioc)
	, server_(server)
	, id_(id)
	{
	}

void websocket_server::connection::start()
	{
	// First read as HTTP
	boost::beast::http::async_read(
		socket_,
		buffer_,
		http_request_,
		[self = shared_from_this()](
			boost::beast::error_code ec, std::size_t bytes_transferred) {
		self->on_read_request(ec, bytes_transferred);
		});
	}

void websocket_server::connection::on_read_request(boost::beast::error_code ec, std::size_t)
	{
	if (ec) {
		server_.remove_connection(id_);
		return;
		}

	if (boost::beast::websocket::is_upgrade(http_request_)) {
		// Handle as WebSocket
		is_websocket_ = true;
		ws_stream_.emplace(socket_);
		ws_stream_->async_accept(
			http_request_,
			[self = shared_from_this()](boost::beast::error_code ec) {
			self->on_websocket_accept(ec);
			});
		}
	else {
		// Handle as HTTP
		handle_http_request();
		}
	}

void websocket_server::connection::handle_http_request()
	{
	// Create response that lives through the async operation
	auto response = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>();
   
	if (server_.http_handler_) {
		server_.http_handler_(http_request_, *response);
		}
	else {
		response->result(boost::beast::http::status::not_found);
		response->version(http_request_.version());
		response->set(boost::beast::http::field::server, "Beast");
		response->set(boost::beast::http::field::content_type, "text/plain");
		response->body() = "404 Not Found\r\n";
		}
	
	response->prepare_payload();
   
	boost::beast::http::async_write(
		socket_,
		*response,
		[self = shared_from_this(), response](  // Keep response alive in lambda
			boost::beast::error_code ec, std::size_t) {
		if (ec) {
			self->server_.remove_connection(self->id_);
			return;
			}
		// HTTP is done, close the connection
		boost::beast::error_code sec;
		self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, sec);
		self->server_.remove_connection(self->id_);
		});
	}

void websocket_server::connection::on_websocket_accept(boost::beast::error_code ec)
	{
	if (ec) {
		server_.remove_connection(id_);
		return;
		}
	
	if (server_.connect_handler_) {
		server_.connect_handler_(id_);
		}
	
	do_read_websocket();
	}

void websocket_server::connection::do_read_websocket()
	{
	ws_stream_->async_read(
		buffer_,
		[self = shared_from_this()](
			boost::beast::error_code ec, std::size_t bytes_transferred) {
		self->on_read_websocket(ec, bytes_transferred);
		});
	}

void websocket_server::connection::on_read_websocket(boost::beast::error_code ec, std::size_t)
	{
	if (ec) {
		server_.remove_connection(id_);
		return;
		}
	
	if (server_.message_handler_) {
		// Use the non-throwing overload: if the peer has already reset
		// the connection, an exception here would escape ioc_.run().
		boost::beast::error_code rec;
		auto endpoint = socket_.remote_endpoint(rec);
		server_.message_handler_(id_,
										 boost::beast::buffers_to_string(buffer_.data()),
										 http_request_,
										 rec ? std::string() : endpoint.address().to_string());
		}
	
	buffer_.consume(buffer_.size());
	do_read_websocket();
	}

void websocket_server::connection::send(const std::string& message)
	{
	// May be called from any thread; the queue and the stream are only
	// touched on the io thread.
	queued_message msg;
	msg.data   = message;
	msg.buffer = std::make_shared<boost::beast::flat_buffer>();
	boost::beast::ostream(*msg.buffer) << msg.data;

	boost::asio::post(socket_.get_executor(),
							[self = shared_from_this(), msg = std::move(msg)]() mutable {
							if (!self->is_websocket_) return;
							self->message_queue_.push(std::move(msg));
							if (!self->writing_)
								self->do_write();
							});
	}

void websocket_server::connection::do_write()
	{
	if(message_queue_.empty() || !is_websocket_) {
		writing_ = false;
		return;
		}
	writing_ = true;

	ws_stream_->async_write(
		message_queue_.front().buffer->data(),
		[self = shared_from_this(), buf = message_queue_.front().buffer](
			boost::beast::error_code ec, std::size_t bytes_transferred) {
		self->on_write(ec, bytes_transferred);
		});
	}

void websocket_server::connection::close()
	{
	// May be called from any thread.
	boost::asio::post(socket_.get_executor(), [self = shared_from_this()]() {
		if (!self->is_websocket_) {
			boost::beast::error_code ec;
			self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			self->server_.remove_connection(self->id_);
			return;
			}
		self->ws_stream_->async_close(
			boost::beast::websocket::close_code::normal,
			[self](boost::beast::error_code ec) {
			self->on_close(ec);
			});
		});
	}

void websocket_server::connection::on_write(boost::beast::error_code ec, std::size_t)
	{
	if(ec) {
		writing_ = false;
		server_.remove_connection(id_);
		return;
		}
	message_queue_.pop();
	do_write();
	}

void websocket_server::connection::on_close(boost::beast::error_code ec)
	{
	server_.remove_connection(id_);
	}

websocket_server::websocket_server(uint16_t port)
	{
	listen(port);
	}

websocket_server::~websocket_server()
	{
	stop();
	}

void websocket_server::listen(uint16_t port)
	{
	boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), port};
   
	acceptor_.emplace(ioc_);
	acceptor_->open(endpoint.protocol());
	acceptor_->set_option(boost::asio::socket_base::reuse_address(true));
	acceptor_->bind(endpoint);
	acceptor_->listen(64 /* backlog */);
	
	do_accept();
	}

uint16_t websocket_server::get_local_port() const
	{
	return acceptor_->local_endpoint().port();
	}

void websocket_server::set_message_handler(message_handler h)
	{
	message_handler_ = std::move(h);
	}

void websocket_server::set_connect_handler(connect_handler h)
	{
	connect_handler_ = std::move(h);
	}

void websocket_server::set_disconnect_handler(disconnect_handler h)
	{
	disconnect_handler_ = std::move(h);
	}

void websocket_server::set_http_handler(http_handler h)
	{
	http_handler_ = std::move(h);
	}

void websocket_server::do_accept()
	{
	if(!acceptor_) return;
	
	acceptor_->async_accept(
		[this](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
		if (!ec) {
			auto id = next_connection_id_++;
			auto conn = std::make_shared<connection>(ioc_, *this, id);
			socket.set_option(boost::asio::ip::tcp::no_delay(true));
			conn->socket_ = std::move(socket);
				{
				std::lock_guard<std::mutex> lock(connections_mutex_);
				connections_[id] = conn;
				}
			conn->start();
			}
		else {
			std::cerr << "websocket::server::do_accept: error on accept, " << ec << std::endl;
			}

		// restart for the next connection
		do_accept();
		});
	}

std::shared_ptr<websocket_server::connection> websocket_server::find_connection(id_type id)
	{
	std::lock_guard<std::mutex> lock(connections_mutex_);
	auto it = connections_.find(id);
	if(it==connections_.end()) return nullptr;
	return it->second;
	}

void websocket_server::send(id_type id, const std::string& message)
	{
	if (auto conn = find_connection(id))
		conn->send(message);
	}

void websocket_server::close(id_type id)
	{
	if (auto conn = find_connection(id))
		conn->close();
	}

void websocket_server::remove_connection(id_type id)
	{
	// Runs on the io thread. A failing connection can get here more than
	// once (e.g. from both a failed read and a failed write); only report
	// the disconnect the first time.
	size_t erased;
		{
		std::lock_guard<std::mutex> lock(connections_mutex_);
		erased = connections_.erase(id);
		}
	if (erased && disconnect_handler_) {
		disconnect_handler_(id);
		}
	}

void websocket_server::run()
	{
	static int calls=0;
	// std::cerr << "RUN CALLED on thread " << pthread_self() << std::endl;

	if(++calls>1)
		throw std::logic_error("Cannot call websocket_server::run multiple times.");
	
	running_ = true;
	ioc_.run();
	running_ = false;
	}

void websocket_server::stop()
	{
	// The acceptor and the connections may only be touched on the io
	// thread. If that thread is running (and this is not it), hand the
	// work over to it; otherwise (not running, or called from the
	// destructor after run() has returned) do it here.
	auto teardown = [this]() {
		boost::beast::error_code ec;
		if(acceptor_)
			acceptor_->close(ec);
		std::unordered_map<id_type, std::shared_ptr<connection>> conns;
			{
			std::lock_guard<std::mutex> lock(connections_mutex_);
			conns.swap(connections_);
			}
		for (auto& [_, conn] : conns) {
			boost::beast::error_code sec;
			conn->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, sec);
			conn->socket_.close(sec);
			}
		ioc_.stop();
		};

	if(running_ && !ioc_.get_executor().running_in_this_thread())
		boost::asio::post(ioc_, teardown);
	else
		teardown();
	}
