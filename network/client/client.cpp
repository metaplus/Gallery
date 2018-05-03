// client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

namespace
{
    std::ofstream ofs{ "C:/Media/Recv/recv.h264",std::ios::binary | std::ios::trunc | std::ios::out };
}

int main(int argc, char* argv[])
{
    try
    {   
        const auto io_context_ptr = std::make_shared<boost::asio::io_context>();
        //std::vector<std::thread> thread_pool(std::thread::hardware_concurrency());
        std::vector<std::thread> thread_pool(2);
        auto client_ptr = std::make_shared<net::client>(io_context_ptr);
        auto session_future = client_ptr->establish_session("localhost", "6666");
        {
            auto work_guard = make_work_guard(*io_context_ptr);
            std::generate_n(thread_pool.begin(), thread_pool.size(),
                [io_context_ptr] { return std::thread{ [io_context_ptr] { io_context_ptr->run(); } }; });
            fmt::print("start receiving\n");
            const std::string_view delim{ "(delim)" };
            auto session_ptr = session_future.get();
            while (session_ptr)
            {
                auto recvbuf = session_ptr->receive(delim);
                //fmt::print("\t recv buffer size: {}\n", recvbuf.size());
                //std::copy_n(recv_buffer.cbegin(), recv_buffer.size(), std::ostreambuf_iterator<char>{ofs});
                if (recvbuf.size() > 32) continue;
                session_ptr->close_socket();
                session_ptr = nullptr;
            }
        }
        for (auto& thread : thread_pool)
            if (thread.joinable()) thread.join();
    }
    catch (const std::exception& e)
    {
        fmt::print(std::cerr, "exception detected: {}\n", e.what());
    }
    return 0;
}