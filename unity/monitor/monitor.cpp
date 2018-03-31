// monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define CPPCONN_PUBLIC_FUNC 
#include "gallery/interface.h"

#pragma warning(push)
#pragma warning(disable: 5040)
#pragma comment(lib,"mysqlcppconn")

#ifdef MONITOR_USE_LEGACY
namespace
{
    const filesystem::path root_path{ "D:/VrLog" };
}

filesystem::path make_log_directory()
{
    const auto dirname = core::time_string("%b%d_%H%M%S"sv);
    const auto dirpath = root_path / dirname;
    if (!is_directory(dirpath))
    {
        core::verify(is_directory(dirpath.root_path()));
        remove_all(dirpath);
        core::verify(create_directories(dirpath));
    }
    return dirpath;
}

struct sinker
{
    std::vector<ipc::message> messages;
    std::mutex mutex;
    std::condition_variable condition;
    std::future<void> task;
    std::string filename;
    bool next_iteration = false;
};

struct duration_notation
{
    const std::chrono::high_resolution_clock::duration& dura;
    friend std::ostream& operator<<(std::ostream& os, const duration_notation& dn)
    {
        using namespace std::chrono;
        return
            dn.dura < 1ms ? os << duration_cast<duration<double, std::micro>>(dn.dura).count() << "us" :
            dn.dura < 1s ? os << duration_cast<duration<double, std::milli>>(dn.dura).count() << "ms" :
            dn.dura < 1min ? os << duration_cast<duration<double>>(dn.dura).count() << "s" :
            dn.dura < 1h ? os << duration_cast<duration<double, std::ratio<60>>>(dn.dura).count() << "min" :
            os << duration_cast<duration<double, std::ratio<3600>>>(dn.dura).count() << "h";
    }
};

void probe_critical_message(const ipc::message& message)
{
    if (!message.is<ipc::info_launch>() && !message.is<ipc::info_started>() && !message.is<ipc::info_exit>()
        && !message.is<ipc::media_format>() && !message.is<vr::Compositor_CumulativeStats>()
        && !message.is<ipc::first_frame_available>() && !message.is<ipc::first_frame_updated>())
        return;
    std::cout
        << rang::fgB::yellow << "[critical] " << message.description() << " "
        << duration_notation{ message.timing() } << rang::fg::reset << "\n";
    if (message.is<ipc::media_format>())
    {
        const auto& mformat = message.get<ipc::media_format>();
        std::transform(mformat.data.cbegin(), mformat.data.cend(), std::ostream_iterator<std::string>(std::cout, "\n"),
            [](const std::pair<const std::string, std::string>& pair) { return "\t" + pair.first + " : " + pair.second; });
    }
}

int main()
{
    const auto pcondition = std::make_shared<std::condition_variable>();
    const auto pmutex = std::make_shared<std::mutex>();
    const auto event_sinker = std::make_shared<sinker>(); event_sinker->filename = "event.json";
    const auto vr_sinker = std::make_shared<sinker>(); vr_sinker->filename = "openvr.json";
    const auto update_sinker = std::make_shared<sinker>(); update_sinker->filename = "update.json";
    const auto dirpath = std::make_shared<const filesystem::path>();
    std::cout << rang::fg::cyan << "[Debug] console launched, default logging root directory at "
        << root_path.generic_string() << " folder" << rang::fg::reset << "\n";
    const auto start_barrier = std::make_shared<concurrent::barrier>(4, [dirpath]()
    {
        const_cast<filesystem::path&>(*dirpath) = make_log_directory();
        std::cout << rang::fg::cyan << "[Debug] new logs created at " << dirpath->generic_string()
            << " folder" << rang::fg::reset << "\n";
    });
    auto sinker_associate = [=](const size_t index) mutable->sinker&
    {
        static std::array<std::shared_ptr<sinker>, ipc::message::index_size()> associate;
        //if (std::any_of(associate.cbegin(), associate.cend(), [](const auto& p) { return p == nullptr; }))
        if (!associate.front())
        {
            associate.fill(nullptr);
            associate.at(ipc::message::index<vr::Compositor_FrameTiming>()) = vr_sinker;
            associate.at(ipc::message::index<vr::Compositor_CumulativeStats>()) = event_sinker;
            associate.at(ipc::message::index<ipc::info_launch>()) = event_sinker;
            associate.at(ipc::message::index<ipc::info_started>()) = event_sinker;
            associate.at(ipc::message::index<ipc::info_exit>()) = event_sinker;
            associate.at(ipc::message::index<ipc::media_format>()) = event_sinker;
            associate.at(ipc::message::index<ipc::update_index>()) = update_sinker;
            associate.at(ipc::message::index<ipc::tagged_pack>()) = event_sinker;
            associate.at(ipc::message::index<ipc::first_frame_available>()) = event_sinker;
            associate.at(ipc::message::index<ipc::first_frame_updated>()) = event_sinker;
            core::verify(std::none_of(associate.cbegin(), associate.cend(), [](const auto& p) { return p == nullptr; }));
        }
        return *associate.at(index);
    };
    core::repeat_each([dirpath, start_barrier](std::shared_ptr<sinker> psinker)
    {
        psinker->task = std::async([&dirpath, start_barrier, psinker]()
        {
            auto& sinker = *psinker;
            while (true)
            {
                start_barrier->arrive_and_wait();
                std::ofstream ofstream;
                cereal::JSONOutputArchive archive{ ofstream };
                ofstream.open(*dirpath / sinker.filename, std::ios::trunc);
                core::verify(ofstream.good());
                std::unique_lock<std::mutex> exlock{ sinker.mutex,std::defer_lock };
                auto next_iteration = false;
                while (!next_iteration)
                {
                    exlock.lock();
                    sinker.condition.wait(exlock);
                    next_iteration = std::exchange(sinker.next_iteration, false);
                    auto messages = std::exchange(sinker.messages, {});
                    exlock.unlock();
                    for (auto& msg : messages)
                        archive << std::move(msg);
                }
                ofstream << std::flush;
            }
        });
    }, event_sinker, vr_sinker, update_sinker);
    static auto recv_channel = std::make_unique<ipc::channel>(false);
    core::verify(SIG_ERR != signal(SIGINT, [](int signum)
    {
        recv_channel->clean_shared_memory();
        std::cout << rang::fg::cyan << "[Debug] capture SIGTERM siganl, clean up shared memory"
            << rang::fg::reset << std::endl;
    }));
    std::optional<std::chrono::duration<double>> start_time;
    uint64_t count_frame_timing = 0, count_gpu_update = 0;
    while (true)
    {
        auto message = recv_channel->receive();
        if (message.is<ipc::media_format>())            //if (message.is<ipc::message::info_launch>())
        {
            start_barrier->arrive_and_wait();
            start_time = message.timing();
        }
        else if (message.is<ipc::update_index>())
            ++count_gpu_update;
        else if (message.is<vr::Compositor_FrameTiming>())
            ++count_frame_timing;
        if (std::chrono::duration<double> current_time = message.timing(); !start_time.has_value())
            start_time = current_time;
        else if (current_time - *start_time > 1s && (count_frame_timing || count_gpu_update))
        {
            std::cout
                << "[info] message " << start_time->count() << "sec -> " << current_time.count() << "sec, "
                << "gpu update " << count_gpu_update << " / openvr frame timing " << count_frame_timing << "\n";
            start_time = std::nullopt;
        }
        probe_critical_message(message);
        auto& sinker = sinker_associate(message.index());
        {
            std::lock_guard<std::mutex> exlock{ sinker.mutex };
            sinker.messages.push_back(std::move(message));
            if (message.is<ipc::info_exit>())
                sinker.next_iteration = true;
        }
        if (message.is<ipc::info_exit>())
        {
            count_frame_timing = 0; count_gpu_update = 0;
            std::scoped_lock<std::mutex, std::mutex> exlocks{ vr_sinker->mutex,update_sinker->mutex };
            vr_sinker->next_iteration = true;
            vr_sinker->condition.notify_one();
            update_sinker->next_iteration = true;
            update_sinker->condition.notify_one();
        }
        sinker.condition.notify_one();
    }
    return 0;
}
#endif // define MONITOR_USE_LEGACY

using namespace std;
namespace av
{
    struct command
    {
        static void thumbnail(const filesystem::path input, filesystem::path output_dir = {},
            const std::tuple<unsigned, unsigned, double> seek_time = { 0,0,10 })
        {
            std::stringstream ss;
            if (output_dir.empty())
                output_dir = input.parent_path() / "thumbnail";
            if (!is_directory(output_dir))
                create_directories(output_dir);
            ss << "ffmpeg -i " << input << " -ss " << std::get<0>(seek_time) << ":" << std::get<1>(seek_time) << ":" << std::get<2>(seek_time)
                << " -frames:v 1 " << (output_dir / input.filename()).replace_extension("png").generic_string() << " -y";
            std::system(ss.str().c_str());
        }
    };

}

namespace test
{
    void routine_decode()
    {
        size_t hash_id = 0;
        unity::_nativeMediaCreate();
        hash_id = unity::_nativeMediaSessionCreate("C:/Media/NewYork.mp4");
        auto frame_count = unity::debug::_nativeMediaSessionGetFrameCount(hash_id);
        uint64_t decode_count = 0;
        while (unity::_nativeMediaSessionHasNextFrame(hash_id))
        {
            const auto res = unity::debug::_nativeMediaSessionDropFrame(hash_id, 1);
            std::cout << "decode count" << (decode_count += res) << "\n";
            if (decode_count > 600)
            {
                unity::_nativeMediaSessionRelease(hash_id);
                hash_id = unity::_nativeMediaSessionCreate("C:/Media/MercedesBenz4096x2048.mp4");
                decode_count = 0;
                int dummy = 1;
            }
        }
        unity::_nativeMediaSessionRelease(hash_id);
        unity::_nativeMediaRelease();
    }
}
int main()
{
    //const auto session = std::make_shared<dll::media_session>("C:/Media/MercedesBenz4096x2048.mp4"s);
    test::routine_decode();
    return EXIT_SUCCESS;
}

#pragma warning(pop)