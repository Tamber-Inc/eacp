#include <eacp/WebView/Test/Process.h>

#include <NanoTest/NanoTest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

using namespace nano;
using namespace eacp::WebView::Test;
using namespace std::chrono_literals;

namespace
{

// Per-platform shell + script helpers. The Process abstraction
// itself is what's under test; we use the system shell as a
// convenient driver that produces controllable stdout/stderr/exit
// codes/env reads on both Apple and Windows. The scripts are
// intentionally minimal so the assertions stay about Process, not
// shell semantics.
#ifdef _WIN32
constexpr auto shellExe = "cmd.exe";
const auto shellFlag = std::string {"/C"};

std::string echoOnStdout(const std::string& msg)
{
    return "echo " + msg;
}

std::string echoOnStderr(const std::string& msg)
{
    return "echo " + msg + " 1>&2";
}

std::string echoEnvVar(const std::string& name)
{
    return "echo " + name + "=%" + name + "%";
}

std::string runSleep(int seconds)
{
    // cmd.exe has no built-in sleep on older Windows. ping to
    // 127.0.0.1 with -n N waits roughly (N-1) seconds.
    return "ping -n " + std::to_string(seconds + 1) + " 127.0.0.1 > NUL";
}

std::string runExitCode(int code)
{
    return "exit /B " + std::to_string(code);
}
#else
constexpr auto shellExe = "/bin/sh";
const auto shellFlag = std::string {"-c"};

std::string echoOnStdout(const std::string& msg)
{
    return "echo " + msg;
}

std::string echoOnStderr(const std::string& msg)
{
    return "echo " + msg + " >&2";
}

std::string echoEnvVar(const std::string& name)
{
    return "echo " + name + "=$" + name;
}

std::string runSleep(int seconds)
{
    return "sleep " + std::to_string(seconds);
}

std::string runExitCode(int code)
{
    return "exit " + std::to_string(code);
}
#endif

Process startShell(const std::string& script, ProcessOptions opts = {})
{
    opts.args = {shellFlag, script};
    return Process {shellExe, opts};
}

void waitForExit(Process& p, std::chrono::milliseconds timeout = 5s)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!p.hasExited() && std::chrono::steady_clock::now() < deadline)
        p.pollOutput(100ms);
    // The child may have exited a beat before the reader thread
    // flushed its last buffer; drain once more so the asserting
    // caller sees a complete picture.
    p.pollOutput(50ms);
}

} // namespace

auto tProcessReadsStdout = test("Process/readsStdout") = []
{
    auto p = startShell(echoOnStdout("hello-world"));
    waitForExit(p);
    check(p.hasExited());
    check(p.stdoutBuffer().find("hello-world") != std::string::npos);
    check(p.exitCode() == 0);
};

auto tProcessReadsStderrSeparately = test("Process/readsStderrSeparately") = []
{
    auto p = startShell(echoOnStderr("only-on-stderr"));
    waitForExit(p);
    check(p.stderrBuffer().find("only-on-stderr") != std::string::npos);
    check(p.stdoutBuffer().find("only-on-stderr") == std::string::npos);
};

auto tProcessNonZeroExitCode = test("Process/nonZeroExitCodePropagates") = []
{
    auto p = startShell(runExitCode(7));
    waitForExit(p);
    check(p.exitCode() == 7);
};

auto tProcessTerminateKillsRunaway = test("Process/terminateKillsRunaway") = []
{
    auto p = startShell(runSleep(30));
    // Give the shell a moment to start so the SIGTERM target exists.
    std::this_thread::sleep_for(100ms);
    check(!p.hasExited());

    p.terminate(1000ms);
    check(p.hasExited());
};

auto tProcessEnvOverridesPropagate = test("Process/envOverridesPropagate") = []
{
    auto opts = ProcessOptions {};
    opts.env["EACP_PROC_TEST_VAR"] = "from-parent";
    auto p = startShell(echoEnvVar("EACP_PROC_TEST_VAR"), opts);
    waitForExit(p);
    check(p.stdoutBuffer().find("EACP_PROC_TEST_VAR=from-parent")
          != std::string::npos);
};

auto tProcessAnnouncedPortCaptured = test("Process/announcedPortLineIsCaptured") = []
{
    // Smoke test for the line pattern Launch.cpp's findAnnouncedPort
    // greps for. If we can capture this from a child we control,
    // we'll be able to capture it from the WebView app under test.
    auto p = startShell(echoOnStdout("EACP_RPC_PORT=54321"));
    waitForExit(p);
    check(p.stdoutBuffer().find("EACP_RPC_PORT=54321") != std::string::npos);
};

auto tProcessThrowsOnMissingExecutable =
    test("Process/throwsOnMissingExecutable") = []
{
    auto threw = false;
    try
    {
        auto p = Process {"/this/path/does/not/exist", ProcessOptions {}};
        (void) p;
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    check(threw);
};
