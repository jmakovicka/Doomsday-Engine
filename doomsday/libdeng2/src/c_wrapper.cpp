/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/c_wrapper.h"
#include "de/Error"
#include "de/App"
#include "de/LegacyCore"
#include "de/LegacyNetwork"
#include "de/Address"
#include "de/ByteRefArray"
#include "de/Block"
#include "de/LogBuffer"
#include "de/Info"
#include <QFile>
#include <cstring>
#include <stdarg.h>

#define DENG2_LEGACYCORE()      de::LegacyCore::instance()
#define DENG2_LEGACYNETWORK()   DENG2_LEGACYCORE().network()
#define DENG2_COMMANDLINE()     static_cast<de::App*>(qApp)->commandLine()

LegacyCore* LegacyCore_New(void* dengApp)
{
    return reinterpret_cast<LegacyCore*>(new de::LegacyCore(reinterpret_cast<de::App*>(dengApp)));
}

void LegacyCore_Delete(LegacyCore* lc)
{
    if(lc)
    {
        // It gets stopped automatically.
        DENG2_SELF(LegacyCore, lc);
        delete self;
    }
}

LegacyCore* LegacyCore_Instance()
{
    return reinterpret_cast<LegacyCore*>(&de::LegacyCore::instance());
}

int LegacyCore_RunEventLoop()
{
    return DENG2_LEGACYCORE().runEventLoop();
}

void LegacyCore_Stop(int exitCode)
{
    DENG2_LEGACYCORE().stop(exitCode);
}

void LegacyCore_SetLoopRate(int freqHz)
{
    DENG2_LEGACYCORE().setLoopRate(freqHz);
}

void LegacyCore_SetLoopFunc(void (*callback)(void))
{
    return DENG2_LEGACYCORE().setLoopFunc(callback);
}

void LegacyCore_PushLoop()
{
    DENG2_LEGACYCORE().pushLoop();
}

void LegacyCore_PopLoop()
{
    DENG2_LEGACYCORE().popLoop();
}

void LegacyCore_PauseLoop()
{
    DENG2_LEGACYCORE().pauseLoop();
}

void LegacyCore_ResumeLoop()
{
    DENG2_LEGACYCORE().resumeLoop();
}

void LegacyCore_Timer(unsigned int milliseconds, void (*callback)(void))
{
    DENG2_LEGACYCORE().timer(milliseconds, callback);
}

int LegacyCore_SetLogFile(const char* filePath)
{
    try
    {
        DENG2_LEGACYCORE().setLogFileName(filePath);
        return true;
    }
    catch(de::LogBuffer::FileError& er)
    {
        LOG_AS("LegacyCore_SetLogFile");
        LOG_WARNING(er.asText());
        return false;
    }
}

const char* LegacyCore_LogFile()
{
    return DENG2_LEGACYCORE().logFileName();
}

void LegacyCore_PrintLogFragment(const char* text)
{
    DENG2_LEGACYCORE().printLogFragment(text);
}

void LegacyCore_PrintfLogFragmentAtLevel(legacycore_loglevel_t level, const char* format, ...)
{
    // Validate the level.
    de::Log::LogLevel logLevel = de::Log::LogLevel(level);
    if(level < DE2_LOG_TRACE || level > DE2_LOG_CRITICAL)
    {
        logLevel = de::Log::MESSAGE;
    }

    // If this level is not enabled, just ignore.
    if(!de::LogBuffer::appBuffer().enabled(logLevel)) return;

    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args); /// @todo unsafe
    va_end(args);

    DENG2_LEGACYCORE().printLogFragment(buffer, logLevel);
}

void LegacyCore_SetTerminateFunc(void (*func)(const char*))
{
    DENG2_LEGACYCORE().setTerminateFunc(func);
}

void LegacyCore_FatalError(const char* msg)
{
    DENG2_LEGACYCORE().handleUncaughtException(msg);
}

void CommandLine_Alias(const char* longname, const char* shortname)
{
    DENG2_COMMANDLINE().alias(longname, shortname);
}

int CommandLine_Count(void)
{
    return DENG2_COMMANDLINE().count();
}

const char* CommandLine_At(int i)
{
    DENG2_ASSERT(i >= 0);
    DENG2_ASSERT(i < DENG2_COMMANDLINE().count());
    return *(DENG2_COMMANDLINE().argv() + i);
}

const char* CommandLine_PathAt(int i)
{
    DENG2_COMMANDLINE().makeAbsolutePath(i);
    return CommandLine_At(i);
}

static int argLastMatch = 0; // used only in ArgCheck/ArgNext (not thread-safe)

const char* CommandLine_Next(void)
{
    if(!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return 0;
    }
    return CommandLine_At(++argLastMatch);
}

const char* CommandLine_NextAsPath(void)
{
    if(!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return 0;
    }
    DENG2_COMMANDLINE().makeAbsolutePath(argLastMatch + 1);
    return CommandLine_Next();
}

int CommandLine_Check(const char* check)
{
    return argLastMatch = DENG2_COMMANDLINE().check(check);
}

int CommandLine_CheckWith(const char* check, int num)
{
    return argLastMatch = DENG2_COMMANDLINE().check(check, num);
}

int CommandLine_Exists(const char* check)
{
    return DENG2_COMMANDLINE().has(check);
}

int CommandLine_IsOption(int i)
{
    return DENG2_COMMANDLINE().isOption(i);
}

int CommandLine_IsMatchingAlias(const char* original, const char* originalOrAlias)
{
    return DENG2_COMMANDLINE().matches(original, originalOrAlias);
}

void LogBuffer_Flush(void)
{
    de::LogBuffer::appBuffer().flush();
}

void LogBuffer_EnableStandardOutput(int enable)
{
	de::LogBuffer::appBuffer().enableStandardOutput(enable != 0);
}

int LegacyNetwork_OpenServerSocket(unsigned short port)
{
    try
    {
        return DENG2_LEGACYNETWORK().openServerSocket(port);
    }
    catch(const de::Error& er)
    {
        LOG_AS("LegacyNetwork_OpenServerSocket");
        LOG_WARNING(er.asText());
        return 0;
    }
}

int LegacyNetwork_Accept(int serverSocket)
{
    return DENG2_LEGACYNETWORK().accept(serverSocket);
}

int LegacyNetwork_Open(const char* ipAddress, unsigned short port)
{
    return DENG2_LEGACYNETWORK().open(de::Address(ipAddress, port));
}

void LegacyNetwork_GetPeerAddress(int socket, char* host, int hostMaxSize, unsigned short* port)
{
    de::Address peer = DENG2_LEGACYNETWORK().peerAddress(socket);
    std::memset(host, 0, hostMaxSize);
    std::strncpy(host, peer.host().toString().toAscii().constData(), hostMaxSize - 1);
    if(port) *port = peer.port();
}

void LegacyNetwork_Close(int socket)
{
    DENG2_LEGACYNETWORK().close(socket);
}

int LegacyNetwork_Send(int socket, const void* data, int size)
{
    return DENG2_LEGACYNETWORK().sendBytes(
                socket, de::ByteRefArray(reinterpret_cast<const de::IByteArray::Byte*>(data), size));
}

unsigned char* LegacyNetwork_Receive(int socket, int *size)
{
    de::Block data;
    if(DENG2_LEGACYNETWORK().receiveBlock(socket, data))
    {
        // Successfully got a block of data. Return a copy of it.
        unsigned char* buffer = new unsigned char[data.size()];
        std::memcpy(buffer, data.constData(), data.size());
        *size = data.size();
        return buffer;
    }
    else
    {
        // We did not receive anything.
        *size = 0;
        return 0;
    }
}

void LegacyNetwork_FreeBuffer(unsigned char* buffer)
{
    delete [] buffer;
}

int LegacyNetwork_IsDisconnected(int socket)
{
    if(!socket) return 0;
    return !DENG2_LEGACYNETWORK().isOpen(socket);
}

int LegacyNetwork_BytesReady(int socket)
{
    if(!socket) return 0;
    return DENG2_LEGACYNETWORK().incomingForSocket(socket);
}

int LegacyNetwork_NewSocketSet()
{
    return DENG2_LEGACYNETWORK().newSocketSet();
}

void LegacyNetwork_DeleteSocketSet(int set)
{
    DENG2_LEGACYNETWORK().deleteSocketSet(set);
}

void LegacyNetwork_SocketSet_Add(int set, int socket)
{
    DENG2_LEGACYNETWORK().addToSet(set, socket);
}

void LegacyNetwork_SocketSet_Remove(int set, int socket)
{
    DENG2_LEGACYNETWORK().removeFromSet(set, socket);
}

int LegacyNetwork_SocketSet_Activity(int set)
{
    if(!set) return 0;
    return DENG2_LEGACYNETWORK().checkSetForActivity(set);
}

Info* Info_NewFromString(const char* utf8text)
{
    try
    {
        return reinterpret_cast<Info*>(new de::Info(QString::fromUtf8(utf8text)));
    }
    catch(const de::Error& er)
    {
        LOG_WARNING(er.asText());
        return 0;
    }
}

Info* Info_NewFromFile(const char* nativePath)
{
    try
    {
        QScopedPointer<de::Info> info(new de::Info);
        info->parseNativeFile(QString::fromUtf8(nativePath));
        return reinterpret_cast<Info*>(info.take());
    }
    catch(const de::Error& er)
    {
        LOG_WARNING(er.asText());
        return 0;
    }
}

void Info_Delete(Info* info)
{
    if(info)
    {
        DENG2_SELF(Info, info);
        delete self;
    }
}

int Info_FindValue(Info* info, const char* path, char* buffer, size_t bufSize)
{
    if(!info) return false;

    DENG2_SELF(Info, info);
    const de::Info::Element* element = self->findByPath(path);
    if(!element || !element->isKey()) return false;
    QString value = static_cast<const de::Info::KeyElement*>(element)->value();
    if(buffer)
    {
        qstrncpy(buffer, value.toUtf8().constData(), bufSize);
        return true;
    }
    else
    {
        // Just return the size of the value.
        return value.size();
    }
}

int UnixInfo_GetConfigValue(const char* configFile, const char* key, char* dest, size_t destLen)
{
    // "paths" is the only config file currently being used.
    if(qstrcmp(configFile, "paths")) return false;

    de::String foundValue;
    if(de::App::unixInfo().path(key, foundValue))
    {
        qstrncpy(dest, foundValue.toUtf8().constData(), destLen);
        return true;
    }
    return false;
}
