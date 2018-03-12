// Monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
namespace
{
    const filesystem::path root_path{ "D:/vr_log" };
}
template<typename Alternate, typename = std::enable_if_t<ipc::message::is_alternative<Alternate>::value>>
std::string_view type_shortname()
{
    auto type_name = std::string_view{ typeid(Alternate).name() };
    const auto begin_pos = std::find(type_name.crbegin(), type_name.crend(), ':');
    type_name.remove_prefix(std::distance(begin_pos, type_name.crend()));
    return type_name;
}
filesystem::path log_dirpath()
{   
    const auto make_dir = [](const filesystem::path& path)
    {
        core::verify(is_directory(path.root_path()));
        if (!is_directory(path))
        {
            remove_all(path);
            core::verify(create_directories(path));
        }
    };
    const auto log_dirname = []
    {
        const auto system_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto local_time = std::localtime(&system_time);
        return fmt::format("log@{}_{}_{}", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    };
    static const auto log_dirpath = root_path / log_dirname();
    make_dir(log_dirpath);
    return log_dirpath;
}
std::ofstream& log_sinker(size_t index)
{
    static auto vr_sinker = std::make_shared<std::ofstream>(log_dirpath() / "openvr.json", std::ios::trunc);
    static auto update_sinker = std::make_shared<std::ofstream>(log_dirpath() / "update.json", std::ios::trunc);
    static auto event_sinker = std::make_shared<std::ofstream>(log_dirpath() / "event.json", std::ios::trunc);
    static auto sinker_indexer = std::optional<std::array<std::shared_ptr<std::ofstream>, std::variant_size_v<ipc::message::value_type>>>{};
    core::verify(vr_sinker->good(), update_sinker->good(), event_sinker->good());
    if(!sinker_indexer.has_value())
    {
        sinker_indexer.emplace();
        sinker_indexer->at(ipc::message::index<vr::Compositor_FrameTiming>()) = vr_sinker;
        sinker_indexer->at(ipc::message::index<vr::Compositor_CumulativeStats>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::info_launch>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::info_started>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::info_exit>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::update_index>()) = update_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::tagged_pack>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::first_frame_available>()) = event_sinker;
        sinker_indexer->at(ipc::message::index<ipc::message::first_frame_updated>()) = event_sinker;
        if (std::any_of(sinker_indexer->begin(), sinker_indexer->end(), [](const auto& p) { return p == nullptr; }))
            throw std::range_error{ "partially initialize sinker indexer" };
    }
    return *sinker_indexer->at(index);
}

int main()
{
    //auto director = log_dirpath();
    ipc::channel recv_ch{ false };
    uint64_t count = 0;
    while (true)
    {
        auto result = recv_ch.async_receive();
        auto message = result.first.get();
        fmt::print("*received message, index{}, timing{}*\n", message.index(), std::divides<double>{}(message.timing().count(), 1000000000));
        auto& sinker = log_sinker(message.index());
        {
            cereal::JSONOutputArchive archive{ sinker };
            archive << message;
        }
        if (message.is<ipc::message::info_launch>()) fmt::print("$message info_launch\n");
        else if (message.is<ipc::message::info_started>()) fmt::print("$message info_start\n");
        else if (message.is<ipc::message::update_index>()); //fmt::print("$message update_index\n");
        else if (message.is<vr::Compositor_FrameTiming>());//fmt::print("$message Compositor_FrameTiming\n");
        else if (message.is<vr::Compositor_CumulativeStats>()) fmt::print("$message Compositor_CumulativeStats\n");
        else if (message.is<ipc::message::info_exit>())
        {
            fmt::print("$message info_exit\n");
            break;
        }
    }
    fmt::print("$$receiving finish$$\n");
    return 0;
}