#include "App.h"
#include "../Utils/Singleton.h"

namespace eacp::Apps
{
AppHandle& getGlobalApp()
{
    return Singleton::get<AppHandle>();
}

AppFactory& getAppFactory()
{
    return Singleton::get<AppFactory>();
}

void destroyApp()
{
    getGlobalApp().reset();
}

void quit()
{
    auto quitFunc = []
    {
        destroyApp();
        Threads::stopEventLoop();
    };

    Threads::callAsync(quitFunc);
}

void restart()
{
    auto restartFunc = []
    {
        destroyApp();

        auto& factory = getAppFactory();
        if (factory)
            factory();
    };

    Threads::callAsync(restartFunc);
}
} // namespace eacp::Apps