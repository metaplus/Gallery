// Gallery.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "interface.h"

BOOL unity::global_create()
{
#ifdef NDEBUG                       
    try
    {
#endif
        dll::media_prepare();
        dll::interprocess_create();
        dll::media_create();
        dll::interprocess_async_send(ipc::message{}.emplace(ipc::info_launch{}));
#ifdef NDEBUG
    }
    catch (...) { return false; }
#endif
    return true;
}
void unity::global_release()
{
    dll::interprocess_send(ipc::message{}.emplace(ipc::info_exit{}));
    std::this_thread::yield();
    dll::media_release();
    dll::interprocess_release();
    dll::graphics_release();
}

namespace
{
    float unity_time = 0;
    std::unique_ptr<graphic> dll_graphic = nullptr;
    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
}

void unity::store_time(FLOAT t)
{
    unity_time = t;
}

void unity::store_alpha_texture(HANDLE texY, HANDLE texU, HANDLE texV)
{
    core::verify(texY != nullptr, texU != nullptr, texV != nullptr);
    dll_graphic->store_textures(texY, texU, texV);
    dll::interprocess_async_send(ipc::message{}.emplace(ipc::info_started{}));
}

UINT32 unity::store_vr_frame_timing(HANDLE vr_timing)
{
    ipc::message msg;
    auto msg_body = *static_cast<vr::Compositor_FrameTiming*>(vr_timing);
    dll::interprocess_async_send(std::move(msg.emplace(msg_body)));
    return msg_body.m_nFrameIndex;
}

UINT32 unity::store_vr_cumulative_status(HANDLE vr_status)
{
    ipc::message msg;
    auto msg_body = *static_cast<vr::Compositor_CumulativeStats*>(vr_status);
    dll::interprocess_send(std::move(msg.emplace(msg_body)));
    return msg_body.m_nNumFramePresents;
}

static void __stdcall OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        core::verify(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
        unity_time = 0;
        dll_graphic = std::make_unique<graphic>();
        unity_device = kUnityGfxRendererD3D11;
    }
    if (dll_graphic != nullptr)
    {
        dll_graphic->process_event(eventType, unity_interface);
    }
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        unity_device = kUnityGfxRendererNull;
        //dll_graphic.reset();
        dll_graphic = nullptr;
    }
}

EXTERN void UNITYAPI UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
    //GlobalCreate();
}

EXTERN void UNITYAPI UnityPluginUnload()
{
    //GlobalRelease();
    unity_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void __stdcall OnRenderEvent(int eventID)
{
    if (auto frame = dll::media_retrieve_frame(); frame.has_value())
        dll_graphic->update_textures(frame.value());
}

EXTERN UnityRenderingEvent UNITYAPI GetRenderEventFunc()
{
    return OnRenderEvent;
}

void dll::graphics_release()
{
    if (dll_graphic)
        dll_graphic->clean_up();
}

namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::future<void> initial;
}

void dll::interprocess_create() {
    return;
    initial = std::async(std::launch::async, []()
    {
        const auto[url, codec] = dll::media_retrieve_format();
        try
        {
            channel = std::make_shared<ipc::channel>(true);
            std::map<std::string, std::string> mformat;
            mformat["url"] = url;
            mformat["codec_name"] = codec->codec->long_name;
            mformat["resolution"] = std::to_string(codec->width) + 'x' + std::to_string(codec->height);
            mformat["gop_size"] = std::to_string(codec->gop_size);
            mformat["pixel_format"] = av_get_pix_fmt_name(codec->pix_fmt);
            mformat["frames_count"] = std::to_string(codec.frame_count());
            channel->send(ipc::message{}.emplace(ipc::media_format{ std::move(mformat) }));
        }
        catch (...)
        {
            channel = nullptr;
        }
    });
}

void dll::interprocess_release() {
    return;
    if (initial.valid())
        initial.get();
    channel = nullptr;
}

void dll::interprocess_async_send(ipc::message message)
{
    return;
    static struct
    {
        std::mutex mutex;
        std::vector<ipc::message> container;
    }temp_mvec;
    static thread_local std::vector<ipc::message> local_mvec;
    if (initial.wait_for(0ns) != std::future_status::ready)
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        return temp_mvec.container.push_back(std::move(message));
    }
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        if (!channel) {
            if (!temp_mvec.container.empty())
                temp_mvec.container.clear();
            return;
        }
        if (!temp_mvec.container.empty())
            std::swap(local_mvec, temp_mvec.container);
    }
    if (!local_mvec.empty())
    {
        for (auto& msg : local_mvec)
            channel->async_send(std::move(msg));
        local_mvec.clear();
    }
    channel->async_send(std::move(message));
}

void dll::interprocess_send(ipc::message message)
{
    return;
    if (initial.wait(); channel == nullptr)
        return;
    channel->send(std::move(message));
}