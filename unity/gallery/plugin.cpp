#include "stdafx.h"
#include "export.h"
#include "context.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "graphic.h"
#include "database.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/time_clock.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/process/environment.hpp>

using net::component::dash_manager;
using net::component::frame_indexed_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using boost::beast::multi_buffer;
using boost::logic::tribool;
using boost::logic::indeterminate;

namespace config
{
    using concurrency = alternate<unsigned>;

    const concurrency asio_concurrency{ std::thread::hardware_concurrency() };
    const concurrency executor_concurrency{ std::thread::hardware_concurrency() };
    const concurrency decoder_concurrency{ 2u };

    std::filesystem::path workset_directory;
    std::filesystem::path database_directory;
    nlohmann::json detail;
    std::optional<folly::Uri> mpd_uri;
}

inline namespace resource
{
    std::shared_ptr<folly::ThreadPoolExecutor> compute_executor;
    std::shared_ptr<folly::ThreadedExecutor> session_executor;
    std::shared_ptr<folly::ThreadedExecutor> database_executor;
    std::shared_ptr<folly::ThreadedExecutor> update_executor;
    std::shared_ptr<database> dll_database;
    std::optional<graphic> dll_graphic;

    struct index_key {};
    struct coordinate_key {};
    struct sequence {};

    boost::multi_index_container<
        stream_context,
        boost::multi_index::indexed_by<
            boost::multi_index::sequenced<
                boost::multi_index::tag<sequence>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<index_key>,
                boost::multi_index::member<stream_context, decltype(stream_context::index), &stream_context::index>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<coordinate_key>,
                boost::multi_index::member<stream_context, decltype(stream_context::coordinate), &stream_context::coordinate>>
        >> tile_stream_table;

    folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();

    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
    IUnityGraphicsD3D11* unity_graphics_dx11 = nullptr;

    namespace description
    {
        std::pair<int, int> frame_grid;
        std::pair<int, int> frame_scale;
        auto tile_width = 0;
        auto tile_height = 0;
        auto tile_count = 0;
    }
}

namespace trace
{
    std::vector<tribool> initial;
    std::vector<tribool> release;
}

namespace debug
{
    constexpr auto enable_null_texture = true;
    constexpr auto enable_dump = false;
    constexpr auto enable_legacy = false;
}

struct frame_batch_index
{
    int64_t frame_index = 0;
    int64_t batch_index = 0;
};

struct frame_batch final : frame_batch_index
{
    using tile_iterator = std::remove_reference<decltype(tile_stream_table)>::type::iterator;

    boost::container::small_vector<tile_iterator, 32> tile_range;
    decltype(tile_range)::iterator tile_end_iterator;

    static inline boost::container::small_vector<tile_iterator, 32> tile_range_source;

    struct render_context final : frame_batch_index
    {
        const stream_context& tile_stream;
        media::frame tile_frame{ nullptr };
    };
};

namespace state
{
    auto unity_time = 0.f;
    std::optional<struct frame_batch> frame_batch;

    struct stream final
    {
        static inline std::atomic<bool> running{ false };

        static bool available(std::optional<media::frame*> frame = std::nullopt) {
            static auto end_of_stream = false;
            if (frame.has_value()) {
                if (auto& frame_ptr = frame.value(); frame_ptr != nullptr) {
                    end_of_stream = frame.value()->empty();
                } else {
                    end_of_stream = false;
                }
            }
            return !end_of_stream;
        }
    };

    std::unique_ptr<folly::UMPMCQueue<update_batch::tile_render_context, false>> render_tile_queue;
}

auto tile_index = [](int col, int row) constexpr {
    using resource::description::frame_grid;
    return col + row * frame_grid.first + 1;
};

auto tile_coordinate = [](int index) constexpr {
    using resource::description::frame_grid;
    const auto x = (index - 1) % frame_grid.first;
    const auto y = (index - 1) / frame_grid.first;
    return std::make_pair(x, y);
};

auto unmanaged_ansi_string = [](std::string_view string) {
    assert(!string.empty());
    const auto allocate = reinterpret_cast<LPSTR>(CoTaskMemAlloc(string.size() + 1));
    *std::copy(string.begin(), string.end(), allocate) = '\0';
    return allocate;
};

auto unmanaged_unicode_string = [](std::string_view string) {
    return SysAllocStringByteLen(string.data(),
        folly::to<UINT>(string.length()));
};

namespace unity
{
    LPSTR _nativeDashCreate() {
        auto url = config::mpd_uri->str();
        assert(!url.empty());
        manager = dash_manager::create_parsed(url, config::asio_concurrency.value(),
                                              compute_executor, dll_database->produce_callback());
        return unmanaged_ansi_string(url);
    }

    BOOL _nativeDashGraphicInfo(INT& col, INT& row,
                                INT& width, INT& height) {
        using namespace resource::description;
        if (manager.wait().hasValue()) {
            frame_grid = manager.value().grid_size();
            frame_scale = manager.value().scale_size();
            std::tie(col, row) = frame_grid;
            std::tie(width, height) = frame_scale;
            tile_width = frame_scale.first / frame_grid.first;
            tile_height = frame_scale.second / frame_grid.second;
            tile_count = frame_grid.first * frame_grid.second;
            config::decoder_concurrency.alter(
                std::max(1u, std::thread::hardware_concurrency() / tile_count));
            assert(config::decoder_concurrency.value() >= 1);
            assert(config::asio_concurrency.value() >= 4);
            assert(config::executor_concurrency.value() >= 4);
            return true;
        }
        return false;
    }

    void _nativeDashCreateTileStream(INT col, INT row, INT index,
                                     HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        using resource::description::tile_width;
        using resource::description::tile_height;
        assert(manager.isReady());
        assert(manager.hasValue());
        stream_context tile_stream;
        core::as_mutable(tile_stream.coordinate) = std::make_pair(col, row);
        core::as_mutable(tile_stream.index) = index;
        if (tex_y && tex_u && tex_v) {
            tile_stream.texture_array[0] = static_cast<ID3D11Texture2D*>(tex_y);
            tile_stream.texture_array[1] = static_cast<ID3D11Texture2D*>(tex_u);
            tile_stream.texture_array[2] = static_cast<ID3D11Texture2D*>(tex_v);
        }
        tile_stream.width_offset = col * tile_width;
        tile_stream.height_offset = row * tile_height;
        tile_stream_table.emplace_back(std::move(tile_stream));
    }

    auto stream_mpeg_dash = [](stream_context& tile_stream) {
        auto& dash_manager = manager.value();
        return [&tile_stream, &dash_manager] {
            auto [col, row] = tile_stream.coordinate;
            auto future_tile = dash_manager.request_tile_context(col, row);
            try {
                while (true) {
                    auto tile_context = std::move(future_tile).get();
                    future_tile = dash_manager.request_tile_context(col, row);
                    frame_segmentor frame_segmentor{
                        config::decoder_concurrency.value(),
                        tile_context.initial,
                        tile_context.data
                    };
                    assert(frame_segmentor.context_valid());
                    while (frame_segmentor.codec_valid()) {
                        auto running = false;
                        for (auto& frame : frame_segmentor.try_consume()) {
                            assert(frame->width > 400 && frame->height > 200);
                            do {
                                running = std::atomic_load(&state::stream::running);
                            }
                            while (running && !tile_stream.decode_queue
                                                          ->tryWriteUntil(std::chrono::steady_clock::now() + 80ms,
                                                                          std::move(frame)));
                            if (!running) {
                                throw std::runtime_error{ "abort running" };
                            }
                            tile_stream.decode_count++;
                        }
                    }
                }
            } catch (core::bad_response_error) {
                tile_stream.decode_queue->blockingWrite(media::frame{ nullptr });
            }
            catch (core::session_closed_error) {
                assert(!"session_executor catch unexpected session_closed_error");
            }
            catch (std::runtime_error) {
                assert(std::atomic_load(&state::stream::running) == false);
            }
            catch (...) {
                //auto diagnostic = boost::current_exception_diagnostic_information();
                assert(!"session_executor catch unexpected exception");
            }
            assert("session worker trap");
        };
    };

    void _nativeDashPrefetch() {
        using resource::description::frame_grid;
        assert(std::size(tile_stream_table) == frame_grid.first*frame_grid.second);
        assert(manager.hasValue());
        assert(state::stream::available());
        for (auto& tile_stream : tile_stream_table.get<coordinate_key>()) {
            session_executor->add(stream_mpeg_dash(core::as_mutable(tile_stream)));
        }
        database_executor->add(dll_database->consume_task());
    }

    BOOL _nativeDashAvailable() {
        return state::stream::available();
    }

    static_assert(std::is_move_constructible<update_batch::tile_render_context>::value);

    BOOL _nativeDashTilePollUpdate(const INT col, const INT row,
                                   const INT64 frame_index, const INT64 batch_index) {
        auto decode_success = false;
        auto& tile_stream_index = tile_stream_table.get<coordinate_key>();
        const auto modify_success = tile_stream_index.modify(
            tile_stream_index.find(std::make_pair(col, row)),
            [=, &decode_success](stream_context& tile_stream) {
                assert(tile_stream.coordinate.first == col);
                assert(tile_stream.coordinate.second == row);
                tile_stream.update.decode_try++;
                if (!tile_stream.render_queue->isFull()) {
                    media::frame frame{ nullptr };
                    decode_success = tile_stream.decode_queue->read(frame);
                    if (decode_success) {
                        tile_stream.update.decode_success++;
                        auto enqueue_try = 0;
                        while (!tile_stream.render_queue->write(std::move(frame))) {
                            assert(enqueue_try++ < 30 && !"poll_update_frame enqueue_try > 30 times");
                        }
                    }
                }
            });
        assert(modify_success && "tile_stream_table modify fail");
        return decode_success;
    }

    INT _nativeDashTilePollUpdateFrame(INT64 frame_index, INT64 batch_index) {
        assert(batch_index > 0);
        if (batch_index == 1) {
            frame_batch::tile_range_source.reserve(tile_stream_table.size());
            auto tile_stream_iterator = tile_stream_table.begin();
            while (tile_stream_iterator != tile_stream_table.end()) {
                frame_batch::tile_range_source.emplace_back(tile_stream_iterator);
                tile_stream_iterator.operator++();
            }
            assert(frame_batch::tile_range_source.size() == description::tile_count);
            assert(!state::frame_batch.has_value());
            state::frame_batch.emplace();
            state::frame_batch->tile_range = frame_batch::tile_range_source;
            state::frame_batch->tile_end_iterator = state::frame_batch->tile_range.end();
        }
        state::frame_batch->tile_end_iterator = std::remove_if(
            state::frame_batch->tile_range.begin(),
            state::frame_batch->tile_end_iterator,
            [=](decltype(tile_stream_table)::iterator& tile_iterator) {
                auto& tile_stream = core::as_mutable(*tile_iterator);
                if (!tile_stream.render_queue->isFull()) {
                    if (media::frame frame{ nullptr }; tile_stream.decode_queue->read(frame)) {
                        tile_stream.update.decode_success++;
                        frame_batch::render_context render_context{
                            frame_index, batch_index,
                            *tile_iterator, std::move(frame)
                        };
                        auto enqueue_try = 0;
                        //while (!state::batch_render_queue->write(std::move(render_context))) {
                        //    assert(enqueue_try++ < 30 && !"poll_update_frame enqueue_try > 30 times");
                        //}
                        return true;
                    }
                }
                return false;
            }
        );
        const auto tile_decode_ready = std::distance(state::frame_batch->tile_end_iterator,
                                                     state::frame_batch->tile_range.end());
        if (tile_decode_ready == description::tile_count) {
            state::frame_batch->tile_range = frame_batch::tile_range_source;
            state::frame_batch->tile_end_iterator = state::frame_batch->tile_range.end();
        }
        state::frame_batch->frame_index = frame_index;
        state::frame_batch->batch_index = batch_index;
        return folly::to<int>(tile_decode_ready);
    }

    void _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp) {
        if constexpr (!debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        if (temp) {
            dll_graphic->store_temp_textures(tex_y, tex_u, tex_v);
        } else {
            dll_graphic->store_textures(tex_y, tex_u, tex_v);
        }
    }

    HANDLE _nativeGraphicCreateTextures(INT width, INT height, CHAR value) {
        return dll_graphic->make_shader_resource(width, height, value, true)
                          .shader;
    }

    namespace test
    {
        void _nativeMockGraphic() {
            dll_graphic.emplace();
            assert(dll_graphic);
        }

        void _nativeConfigConcurrency(UINT codec, UINT net) {
            config::decoder_concurrency.alter(UINT{ codec });
            config::asio_concurrency.alter(UINT{ net });
        }

        void _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor) {
            codec = config::decoder_concurrency.value();
            net = config::asio_concurrency.value();
            executor = config::executor_concurrency.value();
        }

        LPSTR _nativeTestString() {
            return unmanaged_ansi_string("Hello World Test"s);
        }
    }

    auto database_date_suffix = [] {
        auto time = boost::date_time::second_clock<boost::posix_time::ptime>::local_time();
        auto date = time.date();
        auto time_of_day = time.time_of_day();
        return fmt::format("{:02}-{:02}-{:02} {:02}h{:02}m{:02}s",
                           date.year(), date.month(), date.day(),
                           time_of_day.hours(), time_of_day.minutes(), time_of_day.seconds());
    };

    BOOL _nativeLoadEnvConfig() {
        if (const auto workset_env = boost::this_process::environment()["GWorkSet"]; !workset_env.empty()) {
            config::workset_directory = workset_env.to_string();
            const auto detail_path = config::workset_directory / "config.json";
            if (is_directory(config::workset_directory) && is_regular_file(detail_path)) {
                config::detail.clear();
                if (std::ifstream reader{ detail_path }; reader >> config::detail) {
                    auto mpd_uri = config::detail["Dash"]["Uri"].get<std::string>();
                    auto database_name = config::detail["Database"]["Name"].get<std::string>();
                    if (!mpd_uri.empty() && !database_name.empty()) {
                        auto database_path = config::workset_directory / fmt::format("{} {}", database_name, database_date_suffix());
                        const auto database_create_result = create_directories(database_path);
                        assert(database_create_result);
                        config::database_directory = std::move(database_path);
                        config::mpd_uri = folly::Uri{ mpd_uri };
                        return true;
                    }
                }
            };
        }
        return false;
    }

    auto reset_resource = [] {
        tile_stream_table.clear();
        state::stream::available(nullptr);
        state::frame_batch = std::nullopt;
        //state::batch_render_queue = std::nullopt;
    };

    void _nativeLibraryInitialize() {
        trace::initial.emplace_back(indeterminate);
        reset_resource();
        state::stream::running = true;
        state::render_tile_queue = std::make_unique<decltype(state::render_tile_queue)::element_type>();
        //state::batch_render_queue.emplace(180);
        manager = folly::Future<dash_manager>::makeEmpty();
        dll_database = database::make_ptr(config::database_directory.string());
        compute_executor = core::make_pool_executor(config::executor_concurrency.value(), "PluginCompute");
        session_executor = core::make_threaded_executor("PluginSession");
        database_executor = core::make_threaded_executor("PluginDatabase");
        trace::initial.back() = true;
    }

    void _nativeLibraryRelease() {
        trace::release.emplace_back(indeterminate);
        state::stream::running = false;
        session_executor = nullptr; // join 1-1
        if (dll_database) {
            dll_database->stop_consume();
            dll_database = nullptr;
        }
        database_executor = nullptr;                        // join 1-2
        manager = folly::Future<dash_manager>::makeEmpty(); // join 2
        if (compute_executor) {
            compute_executor->join(); // join 3
            assert(compute_executor.use_count() == 1);
        }
        compute_executor = nullptr;
        reset_resource();
        trace::release.back() = true;
    }
}

auto dequeue_render_context = [](int count) {
    std::vector<update_batch::tile_render_context> tile_render_contexts;
    tile_render_contexts.reserve(count);
    std::generate_n(
        std::back_inserter(tile_render_contexts), count,
        [] {
            update_batch::tile_render_context context;
            state::render_tile_queue->dequeue(context);
            return context;
        });
    return tile_render_contexts;
};

namespace
{
    void __stdcall on_graphics_device_event(UnityGfxDeviceEventType eventType) {
        if (eventType == kUnityGfxDeviceEventInitialize) {
            assert(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
            state::unity_time = 0;
            dll_graphic.emplace();
            unity_device = kUnityGfxRendererD3D11;
        }
        if (dll_graphic) {
            dll_graphic->process_event(eventType, unity_interface);
        }
        if (eventType == kUnityGfxDeviceEventShutdown) {
            unity_device = kUnityGfxRendererNull;
            dll_graphic.reset();
        }
    }

    void __stdcall on_render_event(const int event_id) {
        auto graphic_context = folly::lazy([] {
            return dll_graphic->update_context();
        });
        if (const auto update_tile_count = std::abs(event_id); update_tile_count > 0) {
            for (auto& tile_render_context : dequeue_render_context(update_tile_count)) {
                if (!tile_render_context.frame.empty()) {
                    dll_graphic->update_frame_texture(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.frame);
                    dll_graphic->copy_temp_texture_region(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.width_offset,
                        tile_render_context.height_offset);
                }
            }
        }
        if (const auto update_main_texture = event_id <= 0; update_main_texture) {
            dll_graphic->overwrite_main_texture();
        }
    }

    void __stdcall on_texture_update_event(int event_id, void* data) {
        const auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParams*>(data);
        const auto stream_index = params->userData / 3 + 1;
        const auto planar_index = params->userData % 3;
        if (state::stream::available()) {
            auto& tile_stream_index = tile_stream_table.get<index_key>();
            const auto modify_success = tile_stream_index.modify(
                tile_stream_index.find(stream_index),
                [=](stream_context& stream) {
                    auto*& avail_frame = stream.avail_frame;
                    switch (static_cast<UnityRenderingExtEventType>(event_id)) {
                        case kUnityRenderingExtEventUpdateTextureBegin: {
                            if (stream.update.texture_state.none()) {
                                assert(planar_index == 0);
                                assert(avail_frame == nullptr);
                                avail_frame = stream.render_queue->frontPtr();
                                if (!state::stream::available(avail_frame)) {
                                    return;
                                }
                            }
                            assert(avail_frame != nullptr);
                            stream.update.event.begin++;
                            assert(!stream.update.texture_state.test(planar_index));
                            assert(params->format == UnityRenderingExtTextureFormat::kUnityRenderingExtFormatA8_UNorm);
                            stream.update.texture_state.set(planar_index);
                            params->texData = (*avail_frame)->data[planar_index];
                            break;
                        }
                        case kUnityRenderingExtEventUpdateTextureEnd: {
                            assert(avail_frame != nullptr);
                            stream.update.event.end++;
                            assert(stream.update.event.end <= stream.update.event.begin);
                            assert(stream.update.texture_state.test(planar_index));
                            if (stream.update.texture_state.all()) {
                                stream.update.render_finish++;
                                stream.render_queue->popFront();
                                stream.update.texture_state.reset();
                                stream.avail_frame = nullptr;
                                assert(planar_index == 2);
                            }
                            break;
                        }
                        default:
                            assert(!"on_texture_update_event unexpected UnityRenderingExtEventType");
                    }
                });
            assert(modify_success && "tile_stream_table modify fail");
        }
    }
}

EXTERN_C void DLL_EXPORT __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics_dx11 = unity_interface->Get<IUnityGraphicsD3D11>();
    assert(unity_graphics);
    assert(unity_graphics_dx11);
    unity_graphics->RegisterDeviceEventCallback(on_graphics_device_event);
    on_graphics_device_event(kUnityGfxDeviceEventInitialize);
}

EXTERN_C void DLL_EXPORT __stdcall UnityPluginUnload() {
    unity_graphics->UnregisterDeviceEventCallback(on_graphics_device_event);
}

namespace unity
{
    UnityRenderingEvent _nativeGraphicGetRenderCallback() {
        return on_render_event;
    }

    UnityRenderingEventAndData _nativeGraphicGetUpdateCallback() {
        return on_texture_update_event;
    }
}
