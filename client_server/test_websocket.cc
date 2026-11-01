// Test for websocket_client / websocket_server threading and reconnection.
//
// - Server io loop on its own thread; replies to every message from a
//   separate "python" thread (like Server::send_json does).
// - Client io loop on its own thread; several threads call send()
//   concurrently (like the GUI thread and Snoop do).
// - The server drops the connection a few times; the client fail
//   handler reconnects from the io thread (like ComputeThread::on_fail).
//
// Pass criterion: every round gets all its replies, and each dropped
// connection leads to exactly one failure report and one reconnect.
// Most useful when built with -fsanitize=thread.

#include "websocket_server.hh"
#include "websocket_client.hh"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <thread>
#include <vector>

int main()
	{
	const int ROUNDS = 4, SENDERS = 4, PER_SENDER = 200;

	websocket_server server;
	std::mutex qm; std::condition_variable qcv;
	std::deque<std::pair<websocket_server::id_type, std::string>> q;
	std::atomic<websocket_server::id_type> last_id{0};
	std::atomic<int> server_received{0};

	server.set_connect_handler([&](websocket_server::id_type id) { last_id = id; });
	server.set_message_handler([&](websocket_server::id_type id, const std::string& m,
											 const websocket_server::request_type&, const std::string&) {
		++server_received;
		std::lock_guard<std::mutex> l(qm); q.emplace_back(id, m); qcv.notify_one();
		});
	server.listen(0);
	const auto port = server.get_local_port();
	std::thread server_thread([&]{ server.run(); });

	// "Python" thread: replies from outside the server io thread.
	std::atomic<bool> done{false};
	std::thread py_thread([&]{
		while(!done) {
			std::unique_lock<std::mutex> l(qm);
			qcv.wait_for(l, std::chrono::milliseconds(50), [&]{ return !q.empty(); });
			while(!q.empty()) {
				auto [id, m] = q.front(); q.pop_front();
				l.unlock(); server.send(id, "re:"+m); l.lock();
				}
			}
		});

	websocket_client client;
	std::atomic<int> replies{0}, connects{0}, fails{0};
	const std::string uri = "ws://127.0.0.1:" + std::to_string(port) + "/";
	client.set_message_handler([&](const std::string&) { ++replies; });
	client.set_connect_handler([&]{ ++connects; });
	client.set_fail_handler([&](const boost::beast::error_code&) {
		++fails;
		client.connect(uri); // reconnect on the io thread, like ComputeThread::on_fail
		});
	client.connect(uri);
	std::thread client_thread([&]{ client.run(); });

	bool ok = true;
	for(int round=0; round<ROUNDS; ++round) {
		while(connects < round+1) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		int before = replies;
		std::vector<std::thread> senders;
		for(int s=0; s<SENDERS; ++s)
			senders.emplace_back([&, s]{
				for(int i=0; i<PER_SENDER; ++i)
					client.send("r" + std::to_string(round) + "s" + std::to_string(s) + "i" + std::to_string(i));
				});
		for(auto& t: senders) t.join();
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while(replies - before < SENDERS*PER_SENDER && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		int got = replies - before;
		std::cout << "round " << round << ": " << got << "/" << SENDERS*PER_SENDER << " replies" << std::endl;
		if(got < SENDERS*PER_SENDER) ok = false;
		// Drop the connection from the server side (from a non-io thread),
		// while a burst of sends is in flight; the client must reconnect.
		std::thread burst([&]{ for(int i=0; i<100; ++i) client.send("burst"); });
		server.close(last_id);
		burst.join();
		}
	while(connects < ROUNDS+1) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // let any duplicate failures show up
	std::cout << "connects=" << connects << " fails=" << fails << std::endl;
	if(fails != ROUNDS || connects != ROUNDS+1) ok = false; // one failure, one reconnect per drop

	done = true;
	py_thread.join();
	client.stop();  client_thread.join();
	server.stop();  server_thread.join();
	std::cout << (ok ? "PASS" : "FAIL") << std::endl;
	return ok ? 0 : 1;
	}
