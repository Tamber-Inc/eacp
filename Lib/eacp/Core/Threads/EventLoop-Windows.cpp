#include "../Utils/WinInclude.h"

#include "EventLoop.h"
#include "ThreadUtils-Windows.h"
#include "../Utils/Singleton.h"

#include <ea_data_structures/Structures/Vector.h>
#include <atomic>
#include <chrono>
#include <mutex>

namespace eacp::Threads
{

// Custom message that stopEventLoop posts to wake / break the
// innermost runFor. Independent of WM_QUIT (which signals "exit the
// whole process / outer run") so an inner pump can settle without
// tearing down the program.
constexpr UINT WM_EACP_STOP_LOOP = WM_APP + 0x42E0;

static std::atomic<DWORD> mainThreadId {0};
static thread_local int runForDepth = 0;

struct PendingCallbacks
{
    void run()
    {
        auto guard = std::lock_guard(mutex);
        auto queue = getDispatcherQueue();

        for (auto& cb: pendingCallbacks)
            queue.TryEnqueue(cb);

        pendingCallbacks.clear();
    }

    void add(const Callback& cb)
    {
        auto guard = std::lock_guard(mutex);
        pendingCallbacks.add(cb);
    }

    std::recursive_mutex mutex;
    EA::Vector<Callback> pendingCallbacks;
};

PendingCallbacks& getPendingCallbacks()
{
    return Singleton::get<PendingCallbacks>();
}

namespace
{
void initLoopThread()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    initMainThread();
    mainThreadId = GetCurrentThreadId();
    getPendingCallbacks().run();
}
} // namespace

void EventLoop::run()
{
    initLoopThread();

    // Outer run exits only on WM_QUIT — never on WM_EACP_STOP_LOOP, so
    // nested runFor calls can settle without taking the whole process
    // down with them.
    while (true)
    {
        auto msg = MSG();
        auto result = GetMessage(&msg, nullptr, 0, 0);

        if (result == 0 || result == -1)
            break;

        if (msg.message == WM_EACP_STOP_LOOP)
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mainThreadId = 0;

    // Tear down the dispatcher queue while COM is still healthy. If
    // we leave its WinRT smart pointers alive until static destruction
    // their Release runs after the apartment is gone and the process
    // crashes with STATUS_ACCESS_VIOLATION on exit (turning a passing
    // test run into a failure in CTest's eyes).
    shutdownMainThread();
}

bool EventLoop::runFor(std::chrono::milliseconds timeout)
{
    initLoopThread();

    runForDepth++;
    auto popDepth = std::shared_ptr<void> {nullptr,
                                            [](void*) { runForDepth--; }};

    auto deadline = std::chrono::steady_clock::now() + timeout;
    auto timedOut = false;
    auto running = true;

    while (running)
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
        {
            timedOut = true;
            break;
        }

        auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now)
                .count();

        auto wait = MsgWaitForMultipleObjectsEx(
            0, nullptr, (DWORD) remaining, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

        if (wait == WAIT_TIMEOUT)
        {
            timedOut = true;
            break;
        }

        auto msg = MSG();
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                // Re-post so the outer run() also sees it and shuts the
                // process down cleanly; meanwhile break out of this
                // inner pump.
                PostQuitMessage(static_cast<int>(msg.wParam));
                running = false;
                break;
            }

            if (msg.message == WM_EACP_STOP_LOOP)
            {
                running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return !timedOut;
}

void EventLoop::quit()
{
    // Outer-run quit: post WM_QUIT so GetMessage in run() returns 0
    // and the loop exits. Inner runFor calls will also see it (via
    // PeekMessage) and unwind, re-posting WM_QUIT on their way out so
    // the outer run still gets it.
    auto id = mainThreadId.load();
    if (id != 0)
        PostThreadMessageW(id, WM_QUIT, 0, 0);
}

void EventLoop::call(Callback func)
{
    if (auto queue = getDispatcherQueue())
        queue.TryEnqueue([func] { func(); });
    else
        getPendingCallbacks().add(func);
}

// Consumed by async callbacks that want to unblock the blocking caller
// of runEventLoopFor (e.g. an evaluateJavaScript completion handler
// signalling that the result is ready). When called from inside a
// nested runFor we only unwind that inner pump, leaving the outer
// run() alive so Apps::quit's queued teardown still has a chance to
// run; when called from outside any runFor we PostQuitMessage so the
// outer run exits.
void stopEventLoop()
{
    auto id = mainThreadId.load();
    if (id == 0)
        return;

    if (runForDepth > 0)
        PostThreadMessageW(id, WM_EACP_STOP_LOOP, 0, 0);
    else
        PostThreadMessageW(id, WM_QUIT, 0, 0);
}

} // namespace eacp::Threads
