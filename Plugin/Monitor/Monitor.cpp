// Monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
namespace filesystem = std::experimental::filesystem;

int main()
{
    {
        //std::this_thread::sleep_for(100ms);
        auto bb = std::thread{ []() mutable {
            auto time_mark = std::chrono::high_resolution_clock::now();
            ipc::channel recv_ch{ false };
            auto count = -1;
            while (++count != 4000) {
                fmt::print("$receiving {} message$\n", count);
                auto duration = std::chrono::high_resolution_clock::now() - time_mark;
                auto result = recv_ch.async_receive();
                auto data=result.first.get();
                auto& index = data.get<ipc::message::update_index>();
                fmt::print("**received {} message, value{}**\n", count, index.data);
            }
            fmt::print("$$receiving finish$$\n");
        } };
        //aa.join();
        bb.join();
    }
    auto dummy2 = 2;
    return 0;
}