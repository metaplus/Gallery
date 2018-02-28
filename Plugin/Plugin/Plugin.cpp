// Plugin.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


namespace filesystem = std::experimental::filesystem;

namespace metric
{
    void evaluate_playback(const filesystem::path& logfile)
    {
        core::verify(is_regular_file(logfile));
        std::ifstream ifs{ logfile };
        std::string line;
        std::vector<double> frame_timings;
        frame_timings.reserve(7200);
        while (std::getline(ifs, line))
        {
            size_t pos = 0;
            const auto keyword = std::string_view{ "\"SystemTimeInSeconds\": " };
            if (pos = line.find(keyword.data()); pos == std::string::npos)
                continue;
            pos += keyword.size();
            const auto pos_end = line.find(',', pos);
            static std::stringstream convert;
            convert.clear();
            convert.str(std::string_view{ std::next(line.data(),pos), pos_end - pos }.data());
            double value = 0;
            if (convert >> value; value == 0)
                continue;
            frame_timings.push_back(value);
        }
        //const auto iter_end = std::unique(frame_timings.begin(), frame_timings.end());
        //frame_timings.erase(iter_end, frame_timings.end());
        std::sort(frame_timings.begin(), frame_timings.end());
        //const auto min_max = std::minmax_element(frame_timings.begin(), frame_timings.end());
        std::vector<double> frame_latency;
        frame_latency.reserve(frame_timings.size());
        const auto fps = std::divides<void>{}(frame_timings.size() - 1, frame_timings.back() - frame_timings.front());
        std::adjacent_difference(frame_timings.begin(), frame_timings.end(), std::back_inserter(frame_latency));
        const auto stuck = std::count_if(frame_latency.begin() + 1, frame_latency.end(), [fps](auto latency)
        {
            return fps * latency > 2;
        });
        const auto stuck_percent = std::divides<double>{}(stuck, frame_timings.size());
        int dummy = 0;
    }
    
}
int main(int argc, char* argv[])
{
    //metric::evaluate_playback("D:/vr_log/log@21_51_34 8k h264/openvr.json");
    //metric::evaluate_playback("D:/vr_log/log@21_56_4 4k h264/openvr.json");
    //metric::evaluate_playback("D:/vr_log/log@22_5_1 4k h264 2/openvr.json");
    metric::evaluate_playback("D:/vr_log/log@22_15_21 8k hevc/openvr.json");
    //metric::evaluate_playback("D:/vr_log/log@22_10_11 4k hevc/openvr.json");
#if 0
    auto aa = std::thread{ []() mutable{
        auto time_mark = std::chrono::high_resolution_clock::now();
        ipc::channel send_ch{ true };
        auto count = -1;
        while (++count != 2000) {
            fmt::print("!sending {} message!\n", count);
            auto duration = std::chrono::high_resolution_clock::now() - time_mark;
            auto timing = vr::Compositor_CumulativeStats{};
            auto msg = ipc::message{ std::move(timing), std::move(duration) };
            send_ch.async_send(timing, duration);
        }
        fmt::print("!!sending finish!!\n");
        std::this_thread::sleep_for(1h);
        fmt::print("!!sending finish future!!\n");
    } };
    aa.join();
#endif
}

