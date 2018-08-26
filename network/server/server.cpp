// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace http = boost::beast::http;


int main(int argc, char* argv[]) {
	try {
		auto const port = net::config_entry<uint16_t>("net.server.port");
		auto const directory = net::config_entry<std::string>("net.server.directories.root");
		std::multimap<
			boost::asio::ip::tcp::endpoint,
			net::server::session_ptr<net::protocal::http>
		> sessions;
		boost::thread_group thread_group;
		boost::asio::io_context io_context;
		auto context_guard = boost::asio::make_work_guard(io_context);
		net::server::acceptor<boost::asio::ip::tcp> acceptor{ port, io_context };
		auto asio_threads = net::create_asio_threads(io_context, thread_group, std::thread::hardware_concurrency() / 2);
		auto asio_threads_guard = core::make_guard(
			[&thread_group, &context_guard] {
				context_guard.reset();
				thread_group.join_all();
			});
		while (true) {
			fmt::print("app: server session waiting\n");
			if (auto session_ptr = acceptor.listen_session<net::protocal::http>(directory).get(); session_ptr != nullptr) {
				auto session_endpoint = session_ptr->remote_endpoint();
				auto& session = sessions.emplace(std::move(session_endpoint), std::move(session_ptr))->second;
				session->async_run();
			}
			fmt::print("app: server session monitored\n\n");
		}
	} catch (std::exception const& exp) {
		core::inspect_exception(exp);
	} catch (boost::exception const& exp) {
		core::inspect_exception(exp);
	}
	fmt::print("app: application quit\n");
	return 0;
}
