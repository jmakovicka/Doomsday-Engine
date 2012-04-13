/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LogBuffer"
#include "de/Writer"
#include "de/FixedByteArray"
#include "de/Guard"

#include <stdio.h>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

using namespace de;

const Time::Delta FLUSH_INTERVAL = .2;

LogBuffer* LogBuffer::_appBuffer = 0;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : _enabledOverLevel(Log::MESSAGE), 
      _maxEntryCount(maxEntryCount),
      _standardOutput(true),
      _outputFile(0),
      _autoFlushTimer(0)
{
    _autoFlushTimer = new QTimer(this);
    connect(_autoFlushTimer, SIGNAL(timeout()), this, SLOT(flush()));

#ifdef WIN32
    _standardOutput = false;
#endif
}

LogBuffer::~LogBuffer()
{
    setOutputFile("");
    clear();
}

void LogBuffer::clear()
{
    DENG2_GUARD(this);

    flush();
    DENG2_FOR_EACH(i, _entries, EntryList::iterator)
    {
        delete *i;
    }
    _entries.clear();
}

dsize LogBuffer::size() const
{
    DENG2_GUARD(this);
    return _entries.size();
}

void LogBuffer::latestEntries(Entries& entries, int count) const
{
    DENG2_GUARD(this);
    entries.clear();
    for(int i = _entries.size() - 1; i >= 0; --i)
    {
        entries.append(_entries[i]);
        if(count && entries.size() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    _maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry* entry)
{       
    DENG2_GUARD(this);

    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(_lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    _entries.push_back(entry);
    _toBeFlushed.push_back(entry);

    // Should we start autoflush?
    if(!_autoFlushTimer->isActive() && qApp)
    {
        // Every now and then the buffer will be flushed.
        _autoFlushTimer->start(FLUSH_INTERVAL * 1000);
    }
}

void LogBuffer::enable(Log::LogLevel overLevel)
{
    _enabledOverLevel = overLevel;
}

void LogBuffer::setOutputFile(const String& path)
{
    flush();

    if(_outputFile)
    {
        delete _outputFile;
        _outputFile = 0;
    }
    if(path.isEmpty()) return;

    _outputFile = new QFile(path);
    if(!_outputFile->open(QFile::Text | QFile::WriteOnly))
    {
        delete _outputFile;
        _outputFile = 0;
        throw FileError("LogBuffer::setOutputFile", "Could not open " + path);
    }
}

class IOutputStream
{
public:
    virtual ~IOutputStream() {}
    virtual void flush() {}
    virtual IOutputStream& operator << (const QString& text) = 0;
};

class TextOutputStream : public IOutputStream
{
public:
    TextOutputStream(QTextStream* ts) : _ts(ts) {
        if(_ts) _ts->setCodec("UTF-8");
    }
    ~TextOutputStream() {
        delete _ts;
    }
    void flush() {
        if(_ts) _ts->flush();
    }
    IOutputStream& operator << (const QString& text) {
        if(_ts) (*_ts) << text;
        return *this;
    }
private:
    QTextStream* _ts;
};

class DebugOutputStream : public IOutputStream
{
public:
    DebugOutputStream(QtMsgType msgType) {
        _qs = new QDebug(msgType);
    }
    ~DebugOutputStream() {
        delete _qs;
    }
    IOutputStream& operator << (const QString& text) {
        _qs->nospace();
        (*_qs) << text.toUtf8().constData();
        _qs->nospace();
        return *this;
    }
private:
    QDebug* _qs;
};

void LogBuffer::flush()
{
    DENG2_GUARD(this);

    if(!_toBeFlushed.isEmpty())
    {
        TextOutputStream fs  (_outputFile?     new QTextStream(_outputFile) : 0);
#ifndef WIN32
        TextOutputStream outs(_standardOutput? new QTextStream(stdout) : 0);
        TextOutputStream errs(_standardOutput? new QTextStream(stderr) : 0);
#else
        DebugOutputStream outs(QtDebugMsg);
        DebugOutputStream errs(QtWarningMsg);
#endif
        try
        {
#ifdef _DEBUG
            // Debug builds include a timestamp and msg type indicator.
            const duint MAX_LENGTH = 110;
            const duint SIMPLE_INDENT = 25;
#else
            const duint MAX_LENGTH = 89;
            const duint SIMPLE_INDENT = 4;
#endif
            const duint RULER_LENGTH = MAX_LENGTH - SIMPLE_INDENT - 1;

            DENG2_FOR_EACH(i, _toBeFlushed, EntryList::iterator)
            {
                // Error messages will go to stderr instead of stdout.
                QList<IOutputStream*> os;
                os << ((*i)->level() >= Log::WARNING? &errs : &outs) << &fs;

#ifdef _DEBUG
                String message = (*i)->asText();
#else
                // In a release build we can dispense with the metadata.
                String message = (*i)->asText(LogEntry::Simple);
#endif

                // Print line by line.
                String::size_type pos = 0;
                while(pos != String::npos)
                {
                    String::size_type next = message.indexOf('\n', pos);
                    duint lineLen = (next == String::npos? message.size() - pos : next - pos);
                    const duint maxLen = (pos > 0? MAX_LENGTH - SIMPLE_INDENT : MAX_LENGTH);
                    if(lineLen > maxLen)
                    {
                        // Wrap overly long lines.
                        next = pos + maxLen;
                        lineLen = maxLen;

                        // Maybe there's whitespace we can wrap at.
                        int checkPos = pos + maxLen;
                        while(checkPos > pos)
                        {
                            if(message[checkPos].isSpace() ||
                                    (message[checkPos].isPunct() && message[checkPos] != '.' &&
                                     message[checkPos] != ',' && message[checkPos] != '-'))
                            {
                                if(!message[checkPos].isSpace())
                                {
                                    // Include the punctuation on this line.
                                    checkPos++;
                                }

                                // Break here.
                                next = checkPos;
                                lineLen = checkPos - pos;
                                break;
                            }
                            checkPos--;
                        }
                    }

                    // For lines other than the first one, print an indentation.
                    if(pos > 0)
                    {
                        foreach(IOutputStream* stream, os)
                        {
                            if(stream) *stream << QString(SIMPLE_INDENT, QChar(' '));
                        }
                    }

                    String lineText = message.substr(pos, lineLen);

                    // Check for formatting symbols.
                    lineText.replace("$R", String(RULER_LENGTH, '-'));

                    foreach(IOutputStream* stream, os)
                    {
                        if(stream) *stream << lineText;
                    }

                    pos = next;
                    if(pos != String::npos && message[pos].isSpace()) pos++; // Skip whitespace.

                    foreach(IOutputStream* stream, os)
                    {
                        if(stream) *stream << "\n";
                    }
                }
            }
        }
        catch(const de::Error& error)
        {
            QList<IOutputStream*> os;
            os << &errs << &fs;
            foreach(IOutputStream* stream, os)
            {
                if(stream) *stream << "Exception during log flush:\n" << error.what() << "\n";
            }
        }

        _toBeFlushed.clear();

        // Make sure it really gets written now.
        fs.flush();
    }

    _lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(_entries.size() > _maxEntryCount)
    {
        LogEntry* old = _entries.front();
        _entries.pop_front();
        delete old;
    }
}

#ifdef DENG2_FS_AVAILABLE
void LogBuffer::fileBeingDeleted(const File& file)
{
    DENG2_ASSERT(_outputFile == &file);
    flush();
    _outputFile = 0;
}
#endif

void LogBuffer::setAppBuffer(LogBuffer &appBuffer)
{
    _appBuffer = &appBuffer;
}

LogBuffer& LogBuffer::appBuffer()
{
    DENG2_ASSERT(_appBuffer != 0);
    return *_appBuffer;
}
