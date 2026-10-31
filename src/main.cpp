#include "game/game.h"
#include "game/launch_options.h"
#include "platform/platform_debug.h"
#include "platform/window.h"
#include "render/render_device.h"

int main(int argc, char** argv)
{
    PlatformInstallCrashHandler();

    const LaunchOptions options = ParseLaunchOptions(argc, argv);

    std::unique_ptr<IWindow>       window = CreatePlatformWindow(options.window);
    std::unique_ptr<IRenderDevice> device = CreateRenderDevice(*window, options.renderDevice);

    Game game(*window, *device, options);
    return game.Run();
}
