/** @file guiapp.cpp  Application with GUI support.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/GuiApp"
#include "de/graphics/opengl.h"
#include "de/ImageFile"
#include "de/GLWindow"

#include <de/CommandLine>
#include <de/Config>
#include <de/DisplayMode>
#include <de/EventLoop>
#include <de/FileSystem>
#include <de/Log>
#include <de/NativePath>
#include <de/ScriptSystem>
#include <de/Thread>

#include <SDL.h>

#include <fstream>
#include <c_plus/thread.h>

namespace de {

DE_PIMPL(GuiApp)
{
    EventLoop   eventLoop;
    GuiLoop     loop;
    thrd_t      renderThread = nullptr;
    double      dpiFactor = 1.0;
    SDL_Cursor *arrowCursor;
    SDL_Cursor *vsizeCursor;
    SDL_Cursor *hsizeCursor;

    Impl(Public *i) : Base(i)
    {
        // The default render thread is the main thread.
        renderThread = thrd_current();

        arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        vsizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        hsizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    }

    ~Impl() override
    {
        SDL_FreeCursor(arrowCursor);
        SDL_FreeCursor(vsizeCursor);
        SDL_FreeCursor(hsizeCursor);
        DisplayMode_Shutdown();
        SDL_Quit();
    }

    void determineDevicePixelRatio()
    {
#if defined (WIN32)
        // Use the Direct2D API to find out the desktop DPI factor.
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if (SUCCEEDED(hr))
        {
            FLOAT dpiX = 96;
            FLOAT dpiY = 96;
            d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
            dpiFactor = dpiX / 96.0;
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
#else
        // Use a hidden SDL window to determine pixel ratio.
        SDL_Window *temp =
            SDL_CreateWindow("",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             100,
                             100,
                             SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
        int points, pixels;
        SDL_GetWindowSize(temp, &points, nullptr);
        SDL_GL_GetDrawableSize(temp, &pixels, nullptr);
        SDL_DestroyWindow(temp);

        dpiFactor = double(pixels) / double(points);
#endif
    }

    /*void matchLoopRateToDisplayMode()
    {
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0)
        {
            LOG_GL_MSG("Current display mode refresh rate: %d Hz") << mode.refresh_rate;
            self().loop().setRate(mode.refresh_rate ? mode.refresh_rate : 60.0);
        }
    }*/

    DE_PIMPL_AUDIENCE(DisplayModeChange)
};
DE_AUDIENCE_METHOD(GuiApp, DisplayModeChange)

//void GuiApp::setDefaultOpenGLFormat() // static
//{
//    QSurfaceFormat fmt;
//#if defined (DE_OPENGL_ES)
//    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
//    fmt.setVersion(DE_OPENGL_ES / 10, DE_OPENGL_ES % 10);
//#else
//    fmt.setRenderableType(QSurfaceFormat::OpenGL);
//    fmt.setProfile(QSurfaceFormat::CoreProfile);
//    fmt.setVersion(3, 3);
//#endif
//    fmt.setDepthBufferSize(24);
//    fmt.setStencilBufferSize(8);
//    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
//#if defined (DE_DEBUG)
//    fmt.setOption(QSurfaceFormat::DebugContext, true);
//#endif
//    QSurfaceFormat::setDefaultFormat(fmt);
//}

GuiApp::GuiApp(const StringList &args)
    : App(args)
    , d(new Impl(this))
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
                          SDL_INIT_GAMECONTROLLER) != 0)
    {
        throw Error("GuiApp::GuiApp", stringf("Failed to initialize SDL: %s", SDL_GetError()));
    }
    if (SDL_GetNumVideoDisplays() == 0)
    {
        throw Error("GuiApp::GuiApp", "No video displays available");
    }

//    d->matchLoopRateToDisplayMode();
    d->determineDevicePixelRatio();

    static ImageFile::Interpreter intrpImageFile;
    fileSystem().addInterpreter(intrpImageFile);

    // Core packages for GUI functionality.
    addInitPackage("net.dengine.stdlib.gui");
}

void GuiApp::initSubsystems(SubsystemInitFlags subsystemInitFlags)
{
    App::initSubsystems(subsystemInitFlags);

    // The "-dpi" option overrides the detected DPI factor.
    if (auto dpi = commandLine().check("-dpi", 1))
    {
        d->dpiFactor = dpi.params.at(0).toDouble();
    }

    // Apply the overall UI scale factor.
    d->dpiFactor *= config().getd("ui.scaleFactor", 1.0);

    DisplayMode_Init();
    scriptSystem().nativeModule("DisplayMode").set("DPI_FACTOR", d->dpiFactor);
}

double GuiApp::dpiFactor() const
{
    return d->dpiFactor;
}

void GuiApp::setMetadata(String const &orgName, String const &orgDomain,
                         String const &appName, String const &appVersion)
{
    Record &amd = metadata();

    amd.set(APP_NAME,    appName);
    amd.set(APP_VERSION, appVersion);
    amd.set(ORG_NAME,    orgName);
    amd.set(ORG_DOMAIN,  orgDomain);
}

void GuiApp::notifyDisplayModeChanged()
{
    DE_FOR_AUDIENCE2(DisplayModeChange, i)
    {
        i->displayModeChanged();
    }
}

int GuiApp::exec(const std::function<void ()> &startup)
{
    LOGDEV_NOTE("Starting GuiApp event loop...");

    int code = d->eventLoop.exec([this, startup](){
        d->loop.start();
        if (startup) startup();
    });

    LOGDEV_NOTE("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::quit(int code)
{
    LOGDEV_MSG("Stopping GuiApp event loop");

    d->loop.stop();
    d->eventLoop.quit(code);
}

GuiLoop &GuiApp::loop()
{
    return d->loop;
}

void GuiApp::setMouseCursor(MouseCursor cursor)
{
    SDL_ShowCursor(cursor != None ? SDL_ENABLE : SDL_DISABLE);

    switch (cursor)
    {
    case None:             break;
    case Arrow:            SDL_SetCursor(d->arrowCursor); break;
    case ResizeHorizontal: SDL_SetCursor(d->hsizeCursor); break;
    case ResizeVertical:   SDL_SetCursor(d->vsizeCursor); break;
    }
}

bool GuiApp::inRenderThread()
{
    if (!App::appExists())
    {
        return false;
    }
    return thrd_current() == DE_GUI_APP->d->renderThread;
}

void GuiApp::setRenderThread()
{
    DE_GUI_APP->d->renderThread = thrd_current();
}

NativePath GuiApp::appDataPath() const
{
    const auto &amd = metadata();
    #if defined (WIN32)
    {
        return NativePath::homePath() / "AppData/Local" / amd.gets(ORG_NAME) / amd.gets(APP_NAME);
    }
    #elif defined (MACOSX)
    {
        return NativePath::homePath() / "Library/Application Support" / amd.gets(APP_NAME);
    }
    #else
    {
        return NativePath::homePath() / amd.gets(UNIX_HOME);
    }
    #endif
}

void GuiApp::revealFile(const NativePath &fileOrFolder) // static
{
    #if defined (MACOSX)
    {
        using namespace std;

        const NativePath scriptPath = cachePath() / "reveal-path.scpt";
        if (ofstream f{scriptPath.toStdString()})
        {
            // Apple Script to select a specific file.
            f << "on run argv" << endl
              << "  tell application \"Finder\"" << endl
              << "    activate" << endl
              << "    reveal POSIX file (item 1 of argv) as text" << endl
              << "  end tell" << endl
              << "end run" << endl;
            f.close();

            CommandLine{{"/usr/bin/osascript", scriptPath, fileOrFolder}}.execute();
        }
    }
    #else
    {
        DE_ASSERT_FAIL("File revealing not implemented on this platform");
    }
    #endif
}

} // namespace de
