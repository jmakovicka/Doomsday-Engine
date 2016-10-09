/** @file clientapp.h  The client application.
 *
 * @authors Copyright © 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENTAPP_H
#define CLIENTAPP_H

#include <de/BaseGuiApp>
#include <doomsday/doomsdayapp.h>
#include <QUrl>

class BusyRunner;
class ClientPlayer;
class ClientResources;
class ClientServerWorld;
class ClientWindowSystem;
class ConfigProfiles;
class InFineSystem;
class InputSystem;
class RenderSystem;
class ServerLink;
class Updater;

namespace audio { class System; }

/**
 * The client application.
 */
class ClientApp : public de::BaseGuiApp, public DoomsdayApp
{
    Q_OBJECT

public:
    ClientApp(int &argc, char **argv);

    /**
     * Sets up all the subsystems of the application. Must be called before the
     * event loop is started. At the end of this method, the bootstrap script is
     * executed.
     */
    void initialize();

    void preFrame();
    void postFrame();

    void checkPackageCompatibility(
            de::StringList const &packageIds,
            de::String const &userMessageIfIncompatible,
            std::function<void ()> finalizeFunc) override;

public:
    /**
     * Reports a new alert to the user.
     *
     * @param msg    Message to show. May contain style escapes.
     * @param level  Importance of the message.
     */
    static void alert(de::String const &msg, de::LogEntry::Level level = de::LogEntry::Message);

    static ClientPlayer &player(int console);
    static de::LoopResult forLocalPlayers(std::function<de::LoopResult (ClientPlayer &)> func);

    static ClientApp &          app();
    static BusyRunner &         busyRunner();
    static Updater &            updater();
    static ConfigProfiles &     logSettings();
    static ConfigProfiles &     networkSettings();
    static ConfigProfiles &     audioSettings();    ///< @todo Belongs in AudioSystem.
    static ConfigProfiles &     uiSettings();
    static ServerLink &         serverLink();
    static InFineSystem &       infineSystem();
    static InputSystem &        inputSystem();
    static ClientWindowSystem & windowSystem();
    static ::audio::System &    audioSystem();
    static RenderSystem &       renderSystem();
    static ClientResources &    resources();
    static ClientServerWorld &  world();

    static bool hasInputSystem();
    static bool hasRenderSystem();
    static bool hasAudioSystem();

public slots:
    void openHomepageInBrowser();
    void openInBrowser(QUrl url);

protected:
    void unloadGame(GameProfile const &upcomingGame) override;
    void makeGameCurrent(GameProfile const &newGame) override;
    void reset() override;

private:
    DENG2_PRIVATE(d)
};

#endif  // CLIENTAPP_H
