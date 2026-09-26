#pragma once
#include <Windows.h>

namespace dlps
{
    bool Start();
    void Stop();

    // Thread-safe requests. Work is executed on the game's main thread from
    // the validated ModeSwitch tick.
    void RequestOpen();
    void RequestClose();
    void RequestToggle();

    bool Ready();
    bool IsOpen();
}
