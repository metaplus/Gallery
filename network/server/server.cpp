// server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <folly/Function.h>
namespace http = boost::beast::http;

int main(int argc, char* argv[])
{
    try
    {
        auto x = sizeof folly::Function<void()>;
        auto const port = net::config().get<uint16_t>("net.server.port");
        auto const directory = net::config().get<std::string>("net.server.directories.root");
        std::vector<boost::thread*> threads(boost::thread::hardware_concurrency());
        std::multimap<
            boost::asio::ip::tcp::endpoint,
            std::unique_ptr<net::server::session<net::protocal::http>>
        > sessions;
        boost::thread_group thread_group;
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::endpoint const endpoint{ boost::asio::ip::tcp::v4(),port };
        net::server::acceptor<boost::asio::ip::tcp> acceptor{ endpoint, net::executor_guard{ thread_group,io_context } };
        std::generate_n(threads.begin(), threads.size(),
                        [&] { return thread_group.create_thread([&] { io_context.run(); }); });
        net::executor_guard executor_guard{ acceptor };

        while (true)
        {
            fmt::print("app: server session waiting\n");
            auto session_ptr = acceptor.listen_session<net::protocal::http>(directory, net::use_chunk);
            if (!session_ptr) continue;
            auto session_endpoint = session_ptr->remote_endpoint();
            auto& session = sessions.emplace(std::move(session_endpoint), std::move(session_ptr))->second->async_run();
            fmt::print("app: server session monitored\n\n");
            break;
        }
    } catch (std::exception const& exp)
    {
        core::inspect_exception(exp);
    } catch (boost::exception const& exp)
    {
        core::inspect_exception(exp);
    }
    fmt::print("app: application quit\n");
    return 0;
}