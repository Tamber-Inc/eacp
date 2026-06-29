if ((APPLE OR WIN32) AND NOT IOS AND EACP_BUILD_WEBVIEW)
    add_subdirectory(AppHub)
endif ()

if ((APPLE OR WIN32) AND NOT IOS AND EACP_BUILD_GRAPHICS)
    add_subdirectory(RealUpdateDemo)
endif ()
