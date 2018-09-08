// monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define CPPCONN_PUBLIC_FUNC 

//#pragma warning(push)
//#pragma warning(disable: 5040)
//#pragma comment(lib,"mysqlcppconn")

#ifdef MONITOR_USE_LEGACY
namespace
{
    const filesystem::path root_path{ "D:/VrLog" };
}

filesystem::path make_log_directory() {
    const auto dirname = core::time_string("%b%d_%H%M%S"sv);
    const auto dirpath = root_path / dirname;
    if (!is_directory(dirpath)) {
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
    friend std::ostream& operator<<(std::ostream& os, const duration_notation& dn) {
        using namespace std::chrono;
        return
            dn.dura < 1ms ? os << duration_cast<duration<double, std::micro>>(dn.dura).count() << "us" :
            dn.dura < 1s ? os << duration_cast<duration<double, std::milli>>(dn.dura).count() << "ms" :
            dn.dura < 1min ? os << duration_cast<duration<double>>(dn.dura).count() << "s" :
            dn.dura < 1h ? os << duration_cast<duration<double, std::ratio<60>>>(dn.dura).count() << "min" :
            os << duration_cast<duration<double, std::ratio<3600>>>(dn.dura).count() << "h";
    }
};

void probe_critical_message(const ipc::message& message) {
    if (!message.is<ipc::info_launch>() && !message.is<ipc::info_started>() && !message.is<ipc::info_exit>()
        && !message.is<ipc::media_format>() && !message.is<vr::Compositor_CumulativeStats>()
        && !message.is<ipc::first_frame_available>() && !message.is<ipc::first_frame_updated>())
        return;
    std::cout
        << rang::fgB::yellow << "[critical] " << message.description() << " "
        << duration_notation{ message.timing() } << rang::fg::reset << "\n";
    if (message.is<ipc::media_format>()) {
        const auto& mformat = message.get<ipc::media_format>();
        std::transform(mformat.data.cbegin(), mformat.data.cend(), std::ostream_iterator<std::string>(std::cout, "\n"),
                       [](const std::pair<const std::string, std::string>& pair) { return "\t" + pair.first + " : " + pair.second; });
    }
}

int main() {
    const auto pcondition = std::make_shared<std::condition_variable>();
    const auto pmutex = std::make_shared<std::mutex>();
    const auto event_sinker = std::make_shared<sinker>(); event_sinker->filename = "event.json";
    const auto vr_sinker = std::make_shared<sinker>(); vr_sinker->filename = "openvr.json";
    const auto update_sinker = std::make_shared<sinker>(); update_sinker->filename = "update.json";
    const auto dirpath = std::make_shared<const filesystem::path>();
    std::cout << rang::fg::cyan << "[Debug] console launched, default logging root directory at "
        << root_path.generic_string() << " folder" << rang::fg::reset << "\n";
    const auto start_barrier = std::make_shared<concurrent::barrier>(4, [dirpath]() {
        const_cast<filesystem::path&>(*dirpath) = make_log_directory();
        std::cout << rang::fg::cyan << "[Debug] new logs created at " << dirpath->generic_string()
            << " folder" << rang::fg::reset << "\n";
                                                                     });
    auto sinker_associate = [=](const size_t index) mutable->sinker& {
        static std::array<std::shared_ptr<sinker>, ipc::message::index_size()> associate;
        //if (std::any_of(associate.cbegin(), associate.cend(), [](const auto& p) { return p == nullptr; }))
        if (!associate.front()) {
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
    core::repeat_each([dirpath, start_barrier](std::shared_ptr<sinker> psinker) {
        psinker->task = std::async([&dirpath, start_barrier, psinker]() {
            auto& sinker = *psinker;
            while (true) {
                start_barrier->arrive_and_wait();
                std::ofstream ofstream;
                cereal::JSONOutputArchive archive{ ofstream };
                ofstream.open(*dirpath / sinker.filename, std::ios::trunc);
                core::verify(ofstream.good());
                std::unique_lock<std::mutex> exlock{ sinker.mutex,std::defer_lock };
                auto next_iteration = false;
                while (!next_iteration) {
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
    core::verify(SIG_ERR != signal(SIGINT, [](int signum) {
        recv_channel->clean_shared_memory();
        std::cout << rang::fg::cyan << "[Debug] capture SIGTERM siganl, clean up shared memory"
            << rang::fg::reset << std::endl;
                                   }));
    std::optional<std::chrono::duration<double>> start_time;
    uint64_t count_frame_timing = 0, count_gpu_update = 0;
    while (true) {
        auto message = recv_channel->receive();
        if (message.is<ipc::media_format>())            //if (message.is<ipc::message::info_launch>())
        {
            start_barrier->arrive_and_wait();
            start_time = message.timing();
        } else if (message.is<ipc::update_index>())
            ++count_gpu_update;
        else if (message.is<vr::Compositor_FrameTiming>())
            ++count_frame_timing;
        if (std::chrono::duration<double> current_time = message.timing(); !start_time.has_value())
            start_time = current_time;
        else if (current_time - *start_time > 1s && (count_frame_timing || count_gpu_update)) {
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
        if (message.is<ipc::info_exit>()) {
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

#if 0
using namespace std;

namespace av
{
    struct command
    {
        static void thumbnail(const filesystem::path input, filesystem::path output_dir = {},
                              const std::tuple<unsigned, unsigned, double> seek_time = { 0, 0, 10 }) {
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

#include <folly/File.h>
#include <folly/FileUtil.h>
#endif

namespace test
{
#if 0
    void mock_player_context() {
        auto thread_count = unity::_nativeConfigureMedia(6);
        unity::_nativeConfigureNet(2);
        unity::_nativeLibraryInitialize();
        //unity::_nativeMediaSessionCreateNetStream("www.techslides.com/demos/sample-videos/small.mp4", 0, 0);
        //unity::_nativeMediaSessionCreateNetStream("http://localhost:8900/Burj_Khalifa_Pinnacle_7680x3840.webm", 0, 0);
        //unity::_nativeMediaSessionCreateNetStream("http://localhost:8900/AerialCity_3840x1920_h264_8bit_301frame.mp4", 0, 0);
        unity::_nativeMediaSessionCreateNetStream("http://localhost:8900/tile1_test.mp4", 0, 0);
        //std::this_thread::sleep_for(1h);
        int w = 0, h = 0, decode_count = 0;
        unity::_nativeMediaSessionGetResolution(0, w, h);
        while (unity::debug::_nativeMediaSessionDropFrame(0)) {
            ++decode_count;
        }
        unity::_nativeLibraryRelease();
    }

    void mock_player_context_dash() {
        auto thread_count = unity::_nativeConfigureMedia(6);
        unity::_nativeConfigureNet(2);
        unity::_nativeLibraryInitialize();
        unity::_nativeMediaSessionCreateDashStream("http://localhost:8900/dash/full/tile1-576p-5000kbps_dash", 0, 0, 20);
        int w = 0, h = 0, decode_count = 0;
        unity::_nativeMediaSessionGetResolution(0, w, h);
        while (unity::debug::_nativeMediaSessionDropFrame(0)) {
            ++decode_count;
        }
        unity::_nativeLibraryRelease();
    }


    void mock_multi_player_context() {
        auto thread_count = unity::_nativeConfigureMedia(6);
        unity::_nativeConfigureNet(2);

        struct info { int id; int w; int h; int decode_count; bool finish = false; };
        std::map<std::pair<int, int>, info> infos;
        unity::_nativeLibraryInitialize();

        for (auto i = 0; i != 2; ++i) {
            for (auto j = 0; j != 3; ++j) {
                auto url = fmt::format("http://localhost:8900/tile/AerialCity/t6_1_{}_{}.mp4", i, j);
                auto id = unity::_nativeMediaSessionCreateNetStream(url.c_str(), i, j);
                //infos.emplace(std::make_pair(i,j), info{});
                infos[std::make_pair(i, j)].id = id;
            }
        }
        for (auto i = 0; i != 2; ++i) {
            for (auto j = 0; j != 3; ++j) {
                auto& info = infos[std::make_pair(i, j)];
                unity::_nativeMediaSessionGetResolution(info.id, info.w, info.h);
            }
        }
        int finish_count = 0;
        while (finish_count != 6) {
            for (auto i = 0; i != 2; ++i) {
                for (auto j = 0; j != 3; ++j) {
                    auto& info = infos[std::make_pair(i, j)];
                    if (!info.finish) {
                        if (unity::debug::_nativeMediaSessionDropFrame(info.id))
                            info.decode_count++;
                        else {
                            info.finish = true;
                            finish_count++;
                        }
                    }
                }
            }
        }
        unity::_nativeLibraryRelease();
    }

    void mock_mutil_player_context_dash() {   // w 1280 h 576
        auto thread_count = unity::_nativeConfigureMedia(9);
        unity::_nativeConfigureNet(3);
        struct info { int id; int w; int h; int decode_count; bool finish = false; };
        std::map<std::pair<int, int>, info> infos;
        unity::_nativeLibraryInitialize();
        for (auto i = 0; i != 3; ++i) {
            for (auto j = 0; j != 3; ++j) {
                auto url = fmt::format("http://localhost:8900/dash/full/tile{}-576p-5000kbps_dash", i * 3 + j + 1);
                auto id = unity::_nativeMediaSessionCreateDashStream(url.c_str(), i, j, 20);
                infos[std::make_pair(i, j)].id = id;
            }
        }
        for (auto i = 0; i != 3; ++i) {
            for (auto j = 0; j != 3; ++j) {
                auto& info = infos[std::make_pair(i, j)];
                unity::_nativeMediaSessionGetResolution(info.id, info.w, info.h);
            }
        }
        int finish_count = 0;
        while (finish_count != 9) {
            for (auto i = 0; i != 3; ++i) {
                for (auto j = 0; j != 3; ++j) {
                    auto& info = infos[std::make_pair(i, j)];
                    if (!info.finish) {
                        if (unity::debug::_nativeMediaSessionDropFrame(info.id))
                            info.decode_count++;
                        else {
                            info.finish = true;
                            finish_count++;
                        }
                    }
                }
            }
        }
        unity::_nativeLibraryRelease();
    }

    void mock_stream_cursor() {
        std::filesystem::path const dash_root{ "F:/Tile/test-many" };
        //auto const last_index = 705;
        auto const last_index = 10;
        folly::Function<boost::future<media::cursor_base::buffer_type>()> on_future_cursor{
            [&,index = -1]() mutable
            {
                if (++index <= last_index) {
                    return boost::async([&,index] {
                        auto file_tile_path = index > 0 ?
                            dash_root / fmt::format("tile1-576p-5000kbps_dash{}.m4s", index) :
                            dash_root / "tile1-576p-5000kbps_dashinit.mp4";
                        auto file_tile_size = std::filesystem::file_size(file_tile_path);
                        std::ifstream file_tile{ file_tile_path,std::ios::in | std::ios::binary };
                        boost::beast::multi_buffer tile_buffer;
                        boost::beast::ostream(tile_buffer) << file_tile.rdbuf();
                        assert(file_tile_size == boost::asio::buffer_size(tile_buffer.data()));
                        if (index < 50)
                            fmt::print(">buffer index {} size {}\n", index, boost::asio::buffer_size(tile_buffer.data()));
                        return tile_buffer;
                    });
                }
                return boost::make_exceptional_future<media::cursor_base::buffer_type>(std::runtime_error{ __PRETTY_FUNCTION__ });
            } };
        auto cursor_stream = media::forward_cursor_stream::create(std::move(on_future_cursor));
        media::io_context io{ cursor_stream };
        media::format_context format{ io, media::source::format{ "" } };
        auto media_type = media::type::video;
        fmt::print(">>>>>>>>>> start read\n");
        media::packet packet = format.read(media_type);
        auto read_count = 0;
        while (!packet.empty() && ++read_count) {
            fmt::print("read_count {}\n", read_count);
            packet = format.read(media_type);
        }
        fmt::print("read_count total {}\n", read_count);
    }
#endif

#if 1
    void dash_stream() {
        auto manager = net::component::dash_manager::async_create_parsed("http://localhost:8900/dash/tos_srd_4K.mpd").get();
        //manager.wait();
       // auto grid_size = manager.grid_size();
        auto spatial_size = manager.scale_size();
        auto grid_size = manager.grid_size();
        int y = 1;
    }
#endif
}

int main() {
    const auto logger = spdlog::stdout_color_mt("monitor");
    //test::mock_player_context();
    //test::mock_player_context_dash();
    //test::mock_multi_player_context();
    //test::mock_mutil_player_context_dash();
    //test::mock_stream_cursor();
    try {
        test::dash_stream();
    } catch (...) {
        logger->error(boost::current_exception_diagnostic_information());
    }
    return EXIT_SUCCESS;
}
