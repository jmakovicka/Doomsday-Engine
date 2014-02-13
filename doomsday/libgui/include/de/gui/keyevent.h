/** @file keyevent.h  Input event from a keyboard.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_KEYEVENT_H
#define LIBGUI_KEYEVENT_H

#include "libgui.h"
#include "ddkey.h"
#include <de/Event>
#include <de/String>

#include <QFlags>

namespace de {

/**
 * Input event generated by a keyboard device. @ingroup ui
 */
class LIBGUI_PUBLIC KeyEvent : public de::Event
{
public:
    enum State
    {
        Released,   ///< Released key.
        Pressed,    ///< Pressed key.
        Repeat      ///< Repeat while held pressed.
    };

    enum Modifier
    {
        Shift   = 1,
        Control = 2,
        Alt     = 4,
        Meta    = 8,

        NoModifiers = 0
    };
    Q_DECLARE_FLAGS(Modifiers, Modifier)

public:
    KeyEvent();

    KeyEvent(State keyState, int qtKeyCode, int ddKeyCode, int nativeKeyCode, de::String const &keyText,
             Modifiers const &mods = NoModifiers);

    State state() const;
    inline int qtKey() const { return _qtKey; }
    inline int ddKey() const { return _ddKey; }
    inline int nativeCode() const { return _nativeCode; }
    inline de::String const &text() const { return _text; }
    inline Modifiers modifiers() const { return _mods; }

    /**
     * Translates a Qt key code to a Doomsday key code (see ddkey.h).
     *
     * @param qtKey             Qt key code.
     * @param nativeVirtualKey  Native virtual key code.
     * @param nativeScanCode    Native scan code.
     *
     * @return DDKEY code.
     */
    static int ddKeyFromQt(int qtKey, int nativeVirtualKey, int nativeScanCode);

private:
    int _qtKey;
    Modifiers _mods;
    int _ddKey;
    int _nativeCode;
    de::String _text;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KeyEvent::Modifiers)

} // namespace de

#endif // LIBGUI_KEYEVENT_H
