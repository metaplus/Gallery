// client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace
{
    std::ofstream ofs{ "C:/Media/Recv/recv.h264",std::ios::binary | std::ios::trunc | std::ios::out };
}
int main(int argc, char* argv[])
{
    const auto io_context_ptr = std::make_shared<boost::asio::io_context>();
    const auto work_guard = boost::asio::make_work_guard(*io_context_ptr);
    net::client::session_pool session_pool{ io_context_ptr };
    try
    {
        auto session_future = session_pool.make_session("localhost", "2721");
        //std::vector<std::thread> thread_pool(1);
        std::vector<std::thread> thread_pool(std::thread::hardware_concurrency());
        std::generate_n(thread_pool.begin(), thread_pool.size(),
            [io_context_ptr]{ return std::thread{ [io_context_ptr] { io_context_ptr->run(); } }; });
        const auto session_weak_ptr = session_future.get();
        fmt::print("start receiving\n");
        core::verify(!session_weak_ptr.expired());
        const auto session_ptr = session_weak_ptr.lock();
        while(true)
        {
            auto recv_buffer_future = session_ptr->receive("\r\n123"sv);
            auto recv_buffer = recv_buffer_future.get();
            fmt::print("recv_buffer size: {}\n", recv_buffer.size());
            std::copy(recv_buffer.cbegin(), recv_buffer.cend(), std::ostreambuf_iterator<char>{ofs});
            break;
        }
    }
    catch (const std::exception& e)
    {
        fmt::print(std::cerr, "exception detected: {}\n", e.what());
    }

    return 0;
}