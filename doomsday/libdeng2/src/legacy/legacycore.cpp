/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/LegacyCore"

#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QDebug>

using namespace de;

/**
 * @internal Private instance data for LegacyCore.
 */
struct LegacyCore::Instance
{
    QCoreApplication* app;
    void (*func)(void);

    Instance() : app(0), func(0) {}
    ~Instance() {
        delete app;
    }
};

LegacyCore::LegacyCore(int& argc, char** argv)
{
    d = new Instance;

    // Construct a new core application (must have one for the event loop).
    d->app = new QCoreApplication(argc, argv);
}

LegacyCore::~LegacyCore()
{
    stop();

    delete d;
}

int LegacyCore::runEventLoop(void (*func)(void))
{
    qDebug() << "LegacyCore: Starting event loop...";

    // Set up a timer to periodically call the provided callback function.
    d->func = func;
    QTimer::singleShot(1, this, SLOT(callback()));

    // Run the Qt event loop. In the future this will be replaced by the
    // application's main Qt event loop, where deng2 will hook into.
    int code = d->app->exec();

    qDebug() << "LegacyCore: Event loop exited with code" << code;
    return code;
}

void LegacyCore::stop(int exitCode)
{
    d->app->exit(exitCode);
}

void LegacyCore::callback()
{
    if(d->func)
    {
        d->func();
    }
    QTimer::singleShot(1, this, SLOT(callback()));
}
