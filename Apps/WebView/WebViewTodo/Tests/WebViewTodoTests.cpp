#include "Types.h"

#include <eacp/WebView/Test/Launch.h>

#include <NanoTest/NanoTest.h>

#include <Miro/Miro.h>

#include <string>

using namespace nano;
using namespace eacp::WebView::Test;

namespace
{

constexpr auto inputSelector = "[data-testid=\"todo-input\"]";
constexpr auto addSelector = "[data-testid=\"todo-add\"]";
constexpr auto itemSelector = "[data-testid=\"todo-item\"]";
constexpr auto textSelector = "[data-testid=\"todo-text\"]";
constexpr auto toggleSelector = "[data-testid=\"todo-toggle\"]";
constexpr auto removeSelector = "[data-testid=\"todo-remove\"]";
constexpr auto remainingSelector = "[data-testid=\"todo-remaining\"]";

std::string lastItemDescendant(const std::string& child)
{
    return std::string {itemSelector} + ":last-child " + child;
}

std::string firstItemDescendant(const std::string& child)
{
    return std::string {itemSelector} + ":first-child " + child;
}

// Spawns WebViewTodo, waits for the input field to appear (proxy for
// "first render complete"), and returns the LaunchedApp. The
// destructor terminates the spawned child, so each test gets a fresh
// process with the app's seeded three-todos baseline.
LaunchedApp openApp()
{
    auto opts = LaunchOptions {};
    opts.bundle = EACP_WEBVIEW_TEST_APP_BINARY;
    auto app = launchApp(opts);
    app.driver->waitFor(inputSelector);
    return app;
}

} // namespace

auto tSeedsThreeTodos = test("WebViewTodo/seedsThreeTodosOnStartup") = []
{
    auto app = openApp();
    check(app.driver->count(itemSelector) == 3);
};

auto tAddsNewTodo = test("WebViewTodo/addsNewTodoViaForm") = []
{
    auto app = openApp();
    auto before = app.driver->count(itemSelector);

    app.driver->fill(inputSelector, "Buy milk");
    app.driver->click(addSelector);

    check(app.driver->count(itemSelector) == before + 1);
    check(app.driver->text(lastItemDescendant(textSelector)) == "Buy milk");
};

auto tToggleFlipsCompletion =
    test("WebViewTodo/toggleFlipsCompletionAndUpdatesFooter") = []
{
    auto app = openApp();
    auto before = std::stoi(app.driver->text(remainingSelector));

    app.driver->click(firstItemDescendant(toggleSelector));

    auto remaining = std::stoi(app.driver->text(remainingSelector));
    check(remaining == before - 1);
};

auto tRemovingTodo = test("WebViewTodo/removingTodoDecrementsCount") = []
{
    auto app = openApp();
    auto before = app.driver->count(itemSelector);

    app.driver->click(firstItemDescendant(removeSelector));

    check(app.driver->count(itemSelector) == before - 1);
};

auto tDomainRpcsReachable =
    test("WebViewTodo/domainRpcsReachableThroughSameBridge") = []
{
    auto app = openApp();

    // The bridge is shared with WebViewBridge, so the production
    // commands the React app calls (addTodo / getTodos) are also
    // reachable from the harness — handy for setting up state
    // without going through the UI.
    auto before = app.driver->invoke<TodoState>("getTodos");

    auto req = AddTodoRequest {};
    req.text = "Direct add via bridge";
    app.driver->invoke("addTodo", Miro::toJSON(req));

    auto after = app.driver->invoke<TodoState>("getTodos");

    check(after.items.size() == before.items.size() + 1);
    check(after.items[after.items.size() - 1].text == "Direct add via bridge");
};
