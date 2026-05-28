eacp_add_webview_test(WebViewTodoTests
        APP     WebViewTodo
        SOURCES WebViewTodoTests.cpp)

# Test source pulls in the app's Types.h (TodoState, AddTodoRequest)
# for typed invoke<T>() calls. Skip when the helper above no-op'd
# (e.g. iOS).
if (TARGET WebViewTodoTests)
    target_include_directories(WebViewTodoTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif ()
