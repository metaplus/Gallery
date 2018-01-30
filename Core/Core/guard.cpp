#include "stdafx.h"
#include "guard.h"
#include <thread>
#include <iostream>

core::time_guard::time_guard() :
    time_mark_{ std::chrono::steady_clock::now() }
{
}

core::time_guard::~time_guard()
{
    std::cout.setf(std::ios::hex);
    std::cout
        << "thread@" << std::this_thread::get_id() << ' '
        << std::divides<double>{}(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - time_mark_).count(), 1000)
        << " secs\n";
    std::cout.unsetf(std::ios::hex);
}

core::scope_guard::~scope_guard() noexcept(false)
{
    release_();
}