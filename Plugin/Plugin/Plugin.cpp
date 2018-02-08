// Plugin.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Gallery/interface.h"
#include "Core/guard.h"

namespace filesystem = std::experimental::filesystem;


int main(int argc, char* argv[])
{
    auto time_1 = dll::timer_elapsed();
    std::stringstream stream;
    auto msg_body = ipc::message::update_index{ 200 };
    auto msg = ipc::message{ std::move(msg_body), std::move(time_1)};
    decltype(msg) msg2;
    {
        cereal::BinaryOutputArchive oa{ stream };
        oa << msg;
    }
    {
        cereal::BinaryInputArchive ia{ stream };
        ia >> msg2;
    }
    std::packaged_task<void()> task;

    bool laji = task.valid();
    auto future = task.get_future();
    //ipc::message msg{ vr::Compositor_CumulativeStats{} };
    auto sssz = msg.valid_size();
    auto aa = std::thread{ []() mutable{
        auto time_mark = std::chrono::high_resolution_clock::now();
        ipc::channel send_ch{ false };
        auto count = -1;
        while (++count != 100) {
            fmt::print("!sending {} message!\n", count);
            auto duration = std::chrono::high_resolution_clock::now() - time_mark;
            send_ch.async_send(vr::Compositor_CumulativeStats{}, duration);
        }
        fmt::print("!!sending finish!!\n");
        std::this_thread::sleep_for(1h);
        fmt::print("!!sending finish future!!\n");
    } };
    aa.join();
}

