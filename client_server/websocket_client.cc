#include "websocket_client.hh"
#include <iostream>

websocket_client::websocket_client()
	: ssl_ctx_(boost::asio::ssl::context::sslv23_client) // tlsv12_client)
	, resolver_(ioc_)
	, buffer_(std::make_shared<boost::beast::flat_buffer>())
	, is_ssl_(false)
	{
	ssl_ctx_.set_default_verify_paths();
#ifdef __APPLE__
  #include <TargetConditionals.h>
  #if TARGET_OS_IPHONE
	ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_none);
  #else
	// on MacOS or related, include the system certificate chain.
	ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_none);
//	ssl_ctx_.load_verify_file("/etc/ssl/cert.pem");
//	ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_peer);
	// Configure SSL context to be more permissive
  #endif
//  ssl_ctx_.set_options(boost::asio::ssl::context::default_workarounds |
//							  boost::asio::ssl::context::no_sslv2 |
//							  boost::asio::ssl::context::no_sslv3);
#else
	ssl_ctx_.set_verify_mode(boost::asio::ssl::verify_peer);
#endif
//	ssl_ctx_.set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
//		// Log verification details for debugging
//		char subject_name[256];
//		X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
//		X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
//		std::cerr << "websocket_client::verify_callback: " << subject_name << "\n";
//		return preverified;
//		});
	}

websocket_client::~websocket_client()
	{
	// By now run() has returned (or never ran), so nothing can complete a
	// websocket close handshake; a blocking stream close() would wait
	// forever for the peer's reply. Just close the underlying sockets.
	boost::beast::error_code ec;  // ignored errors
	
	if (ws_stream_) {
		boost::beast::get_lowest_layer(*ws_stream_).shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		boost::beast::get_lowest_layer(*ws_stream_).close(ec);
		}
	
	if (wss_stream_) {
		boost::beast::get_lowest_layer(*wss_stream_).shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		boost::beast::get_lowest_layer(*wss_stream_).close(ec);
		}
	
	ioc_.stop();
	}

void websocket_client::set_message_handler(message_handler h) { message_handler_ = std::move(h); }
void websocket_client::set_connect_handler(connect_handler h) { connect_handler_ = std::move(h); }
void websocket_client::set_close_handler(close_handler h) { close_handler_ = std::move(h); }
void websocket_client::set_fail_handler(fail_handler h) { fail_handler_ = std::move(h); }

void websocket_client::reset_connection_state()
	{
	// Close (and thereby cancel all operations on) any previous stream.
	// Their handlers will still run, with operation_aborted, but they
	// carry the old generation number and are ignored.
	boost::beast::error_code ec;
	if(ws_stream_)
		boost::beast::get_lowest_layer(*ws_stream_).close(ec);
	if(wss_stream_)
		boost::beast::get_lowest_layer(*wss_stream_).close(ec);

	++generation_;
	message_queue_ = std::queue<queued_message>();
	writing_   = false;
	connected_ = false;
	failed_    = false;
	buffer_    = std::make_shared<boost::beast::flat_buffer>();
	}

void websocket_client::connect(const std::string& uri_string)
	{
	reset_connection_state();
	const auto gen = generation_;

	// Parse URI (basic)
	Uri uri(uri_string);
	is_ssl_ = uri.protocol=="wss";
	host_   = uri.host;
	port_   = uri.port.empty() ? (is_ssl_ ? "443" : "80") : uri.port;
	path_   = uri.path;
	
	// Create appropriate stream
	if (is_ssl_) {
		wss_stream_ = std::make_unique<boost::beast::websocket::stream<
			boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>(ioc_, ssl_ctx_);
		wss_stream_->binary(false);  // Set to text mode
		wss_stream_->auto_fragment(false);  // Don't fragment messages
		wss_stream_->read_message_max(64 * 1024 * 1024);  // 64MB max message size
		wss_stream_->set_option(boost::beast::websocket::stream_base::decorator(
											[](boost::beast::websocket::request_type& req) {
											req.version(11); 
											req.set(boost::beast::http::field::user_agent, "WebSocket-Client/1.0");
											}));
		}
	else {
		ws_stream_ = std::make_unique<boost::beast::websocket::stream<
			boost::asio::ip::tcp::socket>>(ioc_);
		ws_stream_->binary(false);  // Set to text mode
		ws_stream_->auto_fragment(false);  // Don't fragment messages
		ws_stream_->read_message_max(64 * 1024 * 1024);  // 64MB max message size
		}
	
	// Start the connection process
	resolver_.async_resolve(host_, port_,
									[this, gen](const boost::beast::error_code& ec,
													boost::asio::ip::tcp::resolver::results_type results) {
									if(gen!=generation_) return;
									on_resolve(ec, results);
									});
	}

void websocket_client::on_resolve(const boost::beast::error_code& ec,
											 boost::asio::ip::tcp::resolver::results_type results)
	{
	if (ec) return fail(ec);
	const auto gen = generation_;
	
	auto handler = [this, gen](const boost::beast::error_code& ec,
										const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
		if(gen!=generation_) return;
		on_connect(ec);
		};
	if (is_ssl_)
		boost::asio::async_connect(wss_stream_->next_layer().next_layer(), results, handler);
	else
		boost::asio::async_connect(ws_stream_->next_layer(), results, handler);
	}

void websocket_client::on_connect(const boost::beast::error_code& ec)
	{
	if (ec) return fail(ec);
	const auto gen = generation_;
	
	if (is_ssl_) {
		if (!SSL_set_tlsext_host_name(
				 wss_stream_->next_layer().native_handle(), host_.c_str())) {
			return fail(boost::beast::error_code{
					static_cast<int>(::ERR_get_error()),
					boost::beast::net::error::get_ssl_category()});
			}
		
		wss_stream_->next_layer().async_handshake(
			boost::asio::ssl::stream_base::client,
			[this, gen](const boost::beast::error_code& ec) {
			if(gen!=generation_) return;
			on_ssl_handshake(ec);
			});
		}
	else {
		ws_stream_->async_handshake(host_, path_,
											 [this, gen](const boost::beast::error_code& ec) {
											 if(gen!=generation_) return;
											 on_handshake(ec);
											 });
		}
	}

void websocket_client::on_ssl_handshake(const boost::beast::error_code& ec)
	{
	if (ec) return fail(ec);
	const auto gen = generation_;
	
	wss_stream_->async_handshake(host_, path_,
										  [this, gen](const boost::beast::error_code& ec) {
										  if(gen!=generation_) return;
										  on_handshake(ec);
										  });
	}

void websocket_client::on_handshake(const boost::beast::error_code& ec)
	{
	if (ec) return fail(ec);

	connected_ = true;
	const auto gen = generation_;
	
	if (connect_handler_) {
		connect_handler_();
		}
	// The connect handler may have started a new connection.
	if(gen!=generation_) return;
	
	do_read();
	// Flush anything that was sent while the connection was being set up.
	if(!writing_)
		do_write();
	}

void websocket_client::send(const std::string& message)
	{
	// May be called from any thread. The queue, and the stream, are
	// only touched on the io thread, so hand the message over to it.
	// Beast does not allow two `async_write` at the same time, so
	// messages are queued and written one after the other.
	
	queued_message msg;
	msg.data   = message;
	msg.buffer = std::make_shared<boost::beast::flat_buffer>();
	boost::beast::ostream(*msg.buffer) << msg.data;

	boost::asio::post(ioc_, [this, msg=std::move(msg)]() mutable {
		message_queue_.push(std::move(msg));
		if(connected_ && !writing_)
			do_write();
		});
	}

void websocket_client::on_write(const boost::beast::error_code& ec, std::size_t /* bytes_transferred */)
	{
	if(ec) return fail(ec);
	
	// Remove the message and the associated beast buffer.
	message_queue_.pop();  
	
	// Write next message, if any.
	do_write();
	}

void websocket_client::do_write()
	{
	if (!connected_ || message_queue_.empty()) {
		writing_ = false;
		return;
		}
	
	writing_ = true;
	const auto gen = generation_;
	auto& msg = message_queue_.front();
	// The handler keeps the buffer alive: the queue may be cleared (on
	// failure or reconnect) while this write is still in flight.
	auto handler = [this, gen, buf=msg.buffer](boost::beast::error_code ec, std::size_t bytes_transferred) {
		if(gen!=generation_) return;
		on_write(ec, bytes_transferred);
		};
   
	if (is_ssl_)
		wss_stream_->async_write(msg.buffer->data(), handler);
	else
		ws_stream_->async_write(msg.buffer->data(), handler);
	}

void websocket_client::do_read()
	{
	const auto gen = generation_;
	auto buf = buffer_;
	auto handler = [this, gen, buf](const boost::beast::error_code& ec, std::size_t bytes) {
		if(gen!=generation_) return;
		on_read(ec, bytes);
		};
	if (is_ssl_)
		wss_stream_->async_read(*buf, handler);
	else
		ws_stream_->async_read(*buf, handler);
	}

void websocket_client::on_read(const boost::beast::error_code& ec, std::size_t /* bytes_transferred */)
	{
	if (ec) return fail(ec);

	const auto gen = generation_;
	std::string message = boost::beast::buffers_to_string(buffer_->data());
	buffer_->consume(buffer_->size());
	
	if (message_handler_) {
		message_handler_(message);
		}
	// The message handler may have started a new connection.
	if(gen!=generation_) return;
	
	do_read();
	}

void websocket_client::close()
	{
	// May be called from any thread.
	boost::asio::post(ioc_, [this]() {
		if(!connected_ || failed_) return;
		const auto gen = generation_;
		auto handler = [this, gen](const boost::beast::error_code& ec) {
			if(gen!=generation_) return;
			on_close(ec);
			};
		if (is_ssl_)
			wss_stream_->async_close(boost::beast::websocket::close_code::normal, handler);
		else
			ws_stream_->async_close(boost::beast::websocket::close_code::normal, handler);
		});
	}

void websocket_client::on_close(const boost::beast::error_code& ec)
	{
	if (ec) return fail(ec);

	// The connection is finished; the pending read will now complete
	// with an error, which must not be reported as a failure.
	failed_    = true;
	connected_ = false;
	
	if (close_handler_) {
		close_handler_();
		}
	}

void websocket_client::fail(const boost::beast::error_code& ec)
	{
	// Several outstanding operations (read, write, close) can fail for
	// the same underlying reason; only report the first one.
	if(failed_) return;
	failed_    = true;
	connected_ = false;
	writing_   = false;
	message_queue_ = std::queue<queued_message>();

	if (fail_handler_) {
		fail_handler_(ec);
		}
	}

void websocket_client::run()
	{
	ioc_.run();
	}

void websocket_client::stop()
	{
	ioc_.stop();
	}


Uri::Uri(const std::string& uri)
	{
	path = "/";  // default path
	std::regex pattern("^([^:]+)://([^/:]+)(?::(\\d+))?(/.*)?");
	std::smatch matches;
   
	if (std::regex_match(uri, matches, pattern)) {
		protocol = matches[1];
		host = matches[2];
		if (matches[3].matched) port = matches[3];
		if (matches[4].matched) path = matches[4];
		}
	}

std::string Uri::to_string() const
	{
	std::string result = protocol + "://" + host;
	if (!port.empty()) result += ":" + port;
	result += path;
	return result;
	}
