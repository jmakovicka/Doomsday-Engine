/** @file widgets/logwidget.cpp  Widget for output message log.
 *
 * @todo Refactor: Separate the non-Log-related functionality into its own
 * class that handles long text documents with a background thread used for
 * preparing it for showing on the screen. Such a class can be used as the
 * foundation of DocumentWidget as well.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LogWidget"
//#include "de/FontLineWrapping"
//#include "de/GLTextComposer"
#include "de/TextDrawable"
#include "de/Style"

#include <de/KeyEvent>
#include <de/MouseEvent>
#include <de/MemoryLogSink>
#include <de/LogBuffer>
#include <de/AtlasTexture>
#include <de/Drawable>
#include <de/VertexBuilder>
#include <de/Task>
#include <de/TaskPool>
#include <de/App>

#include <QImage>
#include <QPainter>

namespace de {

using namespace ui;

DENG_GUI_PIMPL(LogWidget),
DENG2_OBSERVES(Atlas, OutOfSpace),
public Font::RichFormat::IStyle
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    /**
     * Cached log entry ready for drawing. The styled text of the entry is
     * wrapped onto multiple lines according to the available content width.
     *
     * CacheEntry is Lockable because it is accessed both by the main thread
     * when drawing and by RewrapTask when wrapping the cached entries to a
     * resized content width.
     */
    class CacheEntry // : public Lockable
    {
        int _height; ///< Current height of the entry, in pixels.
        //bool _unknownHeight; ///< Cannot be drawn yet, or new content is being prepared.

    public:
        //int sinkIndex; ///< Index of the corresponding entry in the Sink (for sorting).
        //Font::RichFormat format;
        //FontLineWrapping wraps;
        //GLTextComposer composer;
        TextDrawable drawable;

        CacheEntry(/*int index, */Font const &font, Font::RichFormat::IStyle &richStyle, Atlas &atlas)
            : _height(0)
            //, _dirty(true)
            //, sinkIndex(index), format(richStyle)
        {
            drawable.init(atlas, font, &richStyle);
            drawable.setRange(Rangei()); // Determined later.
            //wraps.setFont(font);
            //composer.setAtlas(atlas);
        }

        ~CacheEntry()
        {
            //DENG2_GUARD(this);
            // Free atlas allocations.
            drawable.deinit();
            //composer.release();
        }

        int height() const
        {
            return _height;
        }

        bool isReady() const
        {
            return drawable.isReady();
        }

        void wrap(String const &richText, int width)
        {
            //DENG2_GUARD(this);
            //_dirty = true;
            /*String plain = format.initFromStyledText(richText);
            wraps.wrapTextToWidth(plain, format, width);
            composer.setText(plain, format);
            composer.setWrapping(wraps);
            recalculateHeight();*/            
            drawable.setLineWrapWidth(width);
            drawable.setText(richText);
        }

        void rewrap(int width)
        {
            //DENG2_GUARD(this);
            //int oldHeight = _height;
            //wraps.wrapTextToWidth(wraps.text(), format, width);
            drawable.setLineWrapWidth(width);
            //recalculateHeight();
            //return _height - oldHeight;
        }

        void recalculateHeight()
        {
            //_height = wraps.height() * wraps.font().lineSpacing().valuei();
            _height = drawable.wraps().height() * drawable.font().lineSpacing().valuei();
        }

        /*bool needsUpdate() const
        {
            return _dirty;
        }*/

        /// Returns the possible delta in the height of the entry.
        /// Does not block even though a long wrapping task is in progress.
        int updateHeightOnly()
        {
            int const oldHeight = _height;
            if(drawable.update())
            {
                recalculateHeight();
                return _height - oldHeight;
            }
            return 0;
        }

        void update(int yBottom, Rangei const &visiblePixels)
        {
            if(!_height) return;

            // Determine which lines might be visible.
            int const lineSpacing = drawable.font().lineSpacing().value();
            int const yTop = yBottom - _height;
            Rangei range;

            if(yBottom < visiblePixels.start || yTop > visiblePixels.end)
            {
                // Completely outside.
            }
            else if(yTop >= visiblePixels.start && yBottom <= visiblePixels.end)
            {
                // Completely inside.
                range = Rangei(0, drawable.wraps().height());
            }
            else if(yTop < visiblePixels.start && yBottom > visiblePixels.end)
            {
                // Extends over the whole range and beyond.
                range = Rangei::fromSize((visiblePixels.start - yTop) / lineSpacing,
                                         (visiblePixels.end - visiblePixels.start) / lineSpacing + 1);
            }
            else if(yBottom > visiblePixels.end)
            {
                DENG2_ASSERT(yTop >= visiblePixels.start);

                // Partially inside.
                range = Rangei(0, (visiblePixels.end - yTop) / lineSpacing);
            }
            else
            {
                DENG2_ASSERT(yBottom <= visiblePixels.end);

                // Partially inside.
                int count = (yBottom - visiblePixels.start) / lineSpacing;
                range = Rangei(drawable.wraps().height() - count,
                               drawable.wraps().height());
            }

            //qDebug() << yBottom << visiblePixels.asText() << "=>" << range.asText();

            drawable.setRange(range);

            //if(!needsUpdate()) return 0;

            //int const oldHeight = _height;

            // Prepare the visible lines for drawing.
            drawable.update();

            /*{
                //_dirty = false;
                //recalculateHeight();
                return _height - oldHeight;
            }
            return 0;*/
        }

        void make(GLTextComposer::Vertices &verts, int y)
        {
            DENG2_ASSERT(isReady());
            //DENG2_GUARD(this);
            //composer.update();
            //if(isReady())
            {
                drawable.makeVertices(verts, Vector2i(0, y), AlignLeft);
            }
        }

        void clear()
        {
            //DENG2_GUARD(this);
            drawable.setRange(Rangei()); // Nothing visible.
            /*
            if(composer.isReady())
            {
                composer.release();
            }*/
        }
    };

    /**
     * Log sink that wraps log entries as rich text to multiple lines before
     * they are shown by LogWidget. The wrapping is done by a TaskPool in the
     * background.
     *
     * Whenever all the wrapping tasks are complete, LogWidget will be notified
     * and it will check if excess entries should be removed. Entries are only
     * removed from the sink (and cache) during a prune, in the main thread,
     * and during this no background tasks are running.
     */
    class WrappingMemoryLogSink : public MemoryLogSink
    {
    public:
        WrappingMemoryLogSink(LogWidget::Instance *wd)
            : d(wd)
            , _maxEntries(1000)
            , _next(0)
            , _width(0)
        {
            // Whenever the pool is idle, we'll check if pruning should be done.
            /*QObject::connect(&_pool, SIGNAL(allTasksDone()),
                             d->thisPublic, SLOT(pruneExcessEntries()));*/
        }

        ~WrappingMemoryLogSink()
        {
            //_pool.waitForDone();
            clear();
        }

        /*
        bool isBusy() const
        {
            return !_pool.isDone();
        }*/

        int maxEntries() const { return _maxEntries; }

        void clear()
        {
            DENG2_GUARD(_wrappedEntries);
            qDeleteAll(_wrappedEntries);
            _wrappedEntries.clear();
        }

        void remove(int pos, int n = 1)
        {
            DENG2_GUARD(this);
            MemoryLogSink::remove(pos, n);
            _next -= n;
        }

        void setWidth(int wrapWidth)
        {
            _width = wrapWidth;
            beginWorkOnNext();
        }

        void addedNewEntry(LogEntry &)
        {
            beginWorkOnNext();
        }

        CacheEntry *nextCachedEntry()
        {
            DENG2_GUARD(_wrappedEntries);
            if(_wrappedEntries.isEmpty()) return 0;
            return _wrappedEntries.takeFirst();
        }

#if 0
    protected:
        /**
         * Asynchronous task for wrapping an incoming entry as rich text in the
         * background. WrapTask is responsible for creating the CacheEntry
         * instances for the LogWidget's entry cache.
         */
        class WrapTask : public Task
        {
        public:
            WrapTask(WrappingMemoryLogSink &owner, int index, String const &styledText)
                : _sink(owner),
                  _index(index),
                  _styledText(styledText)
            {}

            void runTask()
            {
                CacheEntry *cached = new CacheEntry(_index, *_sink.d->font, *_sink.d,
                                                    *_sink.d->entryAtlas);
                cached->wrap(_styledText, _sink._width);

                //usleep(75000); // testing aid

                DENG2_GUARD_FOR(_sink._wrappedEntries, G);
                _sink._wrappedEntries << cached;
            }

        private:
            WrappingMemoryLogSink &_sink;
            int _index;
            String _styledText;
        };
#endif

        /**
         * Schedules wrapping tasks for all incoming entries.
         */
        void beginWorkOnNext()
        {
            if(!d->formatter) return; // Must have a formatter.

            DENG2_GUARD(this);

            while(_width > 0 && _next >= 0 && _next < entryCount())
            {
                LogEntry const &ent = entry(_next);
                String const styled = d->formatter->logEntryToTextLines(ent).at(0);

                //_pool.start(new WrapTask(*this, _next, styled));

                CacheEntry *cached = new CacheEntry(/*_next, */*d->font, *d, *d->entryAtlas);
                //cached->wrap(_styledText, _sink._width);
                cached->wrap(styled, _width);

                //usleep(75000); // testing aid

                DENG2_GUARD(_wrappedEntries);
                _wrappedEntries << cached;

                _next++;
            }
        }

    private:
        LogWidget::Instance *d;
        int _maxEntries;
        int _next;
        TaskPool _pool;
        int _width;

        struct WrappedEntries : public QList<CacheEntry *>, public Lockable {};
        WrappedEntries _wrappedEntries;
    };

    WrappingMemoryLogSink sink;

    QList<CacheEntry *> cache; ///< Indices match entry indices in sink.
    int cacheWidth;

#if 0
    /**
     * Asynchronous task that iterates through the cached entries in reverse
     * order and rewraps their existing content to a new maximum width. The
     * task is cancellable because an existing wrap should be abandoned if the
     * widget content width changes again during a rewrap.
     *
     * The total height of the entries is updated as the entries are rewrapped.
     */
    class RewrapTask : public Task
    {
        LogWidget::Instance *d;
        duint32 _cancelLevel;
        int _next;
        int _width;

    public:
        RewrapTask(LogWidget::Instance *wd, int startFrom, int width)
            : d(wd), _cancelLevel(wd->cancelRewrap), _next(startFrom), _width(width)
        {}

        void runTask()
        {
            while(_next >= 0 && _cancelLevel == d->cancelRewrap)
            {
                CacheEntry *e = d->cache[_next--];

                // Rewrap and update total height.
                int delta = e->rewrap(_width);
                d->self.modifyContentHeight(delta);

                /// @todo Adjust the scroll position if this entry is below it
                /// (would cause a visible scroll to occur).

                if(_next < d->visibleRange.end)
                {
                    // Above the visible range, no need to rush.
                    TimeDelta(.001).sleep();
                }
            }
        }
    };

    TaskPool rewrapPool; ///< Used when rewrapping existing cached entries.
    volatile duint32 cancelRewrap;

    enum { CancelAllRewraps = 0xffffffff };
#endif

    // State.
    Rangei visibleRange;
    Animation contentOffset; ///< Additional vertical offset to apply when drawing content.
    int contentOffsetForDrawing; ///< Set when geometry updated.

    // Style.
    LogSink::IFormatter *formatter;
    Font const *font;
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;
    ColorBank::Color altAccentColor;

    // GL objects.
    VertexBuf *buf;
    VertexBuf *bgBuf;
    AtlasTexture *entryAtlas;
    bool entryAtlasLayoutChanged;
    bool entryAtlasFull;
    Drawable contents;
    Drawable background;
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uShadowColor;
    GLUniform uColor;
    GLUniform uBgMvpMatrix;
    Matrix4f projMatrix;
    Matrix4f viewMatrix;
    Id scrollTex;

    Instance(Public *i)
        : Base(i)
        , sink(this)
        , cacheWidth(0)
        //, cancelRewrap(0)
        , visibleRange(Rangei(-1, -1))
        , formatter(0)
        , font(0)
        , buf(0)
        , entryAtlas(0)
        , entryAtlasLayoutChanged(false)
        , entryAtlasFull(false)
        , uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4)
        , uTex        ("uTex",       GLUniform::Sampler2D)
        , uShadowColor("uColor",     GLUniform::Vec4)
        , uColor      ("uColor",     GLUniform::Vec4)
        , uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        self.setFont("log.normal");
        updateStyle();
    }

    ~Instance()
    {
        LogBuffer::appBuffer().removeSink(sink);
    }

    void clear()
    {
        sink.clear();
        clearCache();
    }

    void cancelRewraps()
    {
        /*
        cancelRewrap = CancelAllRewraps;
        rewrapPool.waitForDone();
        cancelRewrap = 0;*/

        // Cancel all wraps.

    }

    void clearCache()
    {
        cancelRewraps();

        entryAtlas->clear();
        cache.clear();
    }

    void updateStyle()
    {        
        // TODO -- stop wrapping tasks in the sink

        Style const &st = style();

        font           = &self.font();

        normalColor    = st.colors().color("log.normal");
        highlightColor = st.colors().color("log.highlight");
        dimmedColor    = st.colors().color("log.dimmed");
        accentColor    = st.colors().color("log.accent");
        dimAccentColor = st.colors().color("log.dimaccent");
        altAccentColor = st.colors().color("log.altaccent");

        self.set(Background(st.colors().colorf("background")));
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch(index)
        {
        default:
        case Font::RichFormat::NormalColor:
            return normalColor;

        case Font::RichFormat::HighlightColor:
            return highlightColor;

        case Font::RichFormat::DimmedColor:
            return dimmedColor;

        case Font::RichFormat::AccentColor:
            return accentColor;

        case Font::RichFormat::DimAccentColor:
            return dimAccentColor;

        case Font::RichFormat::AltAccentColor:
            return altAccentColor;
        }
    }

    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const
    {
        return style().richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
    }

    Font const *richStyleFont(Font::RichFormat::Style fontStyle) const
    {
        return style().richStyleFont(fontStyle);
    }

    void glInit()
    {
        // Private atlas for the composed entry text lines.
        entryAtlas = AtlasTexture::newWithRowAllocator(
                Atlas::BackingStore | Atlas::AllowDefragment,
                GLTexture::maximumSize().min(Atlas::Size(4096, 2048)));

        entryAtlas->audienceForReposition() += this;
        entryAtlas->audienceForOutOfSpace() += this;

        // Simple texture for the scroll indicator.
        Image solidWhitePixel = Image::solidColor(Image::Color(255, 255, 255, 255),
                                                  Image::Size(1, 1));
        scrollTex = entryAtlas->alloc(solidWhitePixel);
        self.setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());

        uTex = entryAtlas;
        uColor = Vector4f(1, 1, 1, 1);

        background.addBuffer(bgBuf = new VertexBuf);
        shaders().build(background.program(), "generic.textured.color")
                << uBgMvpMatrix
                << uAtlas();

        // Vertex buffer for the log entries.
        contents.addBuffer(buf = new VertexBuf);
        shaders().build(contents.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uShadowColor
                << uTex;
    }

    void glDeinit()
    {
        clearCache();

        delete entryAtlas;
        entryAtlas = 0;

        contents.clear();
        background.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        if(entryAtlas == &atlas)
        {
            entryAtlasLayoutChanged = true;
            self.setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());
        }
    }

    void atlasOutOfSpace(Atlas &atlas)
    {
        if(entryAtlas == &atlas)
        {
            entryAtlasFull = true;
        }
    }

    duint contentWidth() const
    {
        return self.viewportSize().x;
    }

    int maxVisibleOffset()
    {
        return self.maximumScrollY().valuei();
    }

    void modifyContentHeight(float delta)
    {
        self.modifyContentHeight(delta); //cached->height());

        // Adjust visible offset so it remains fixed in relation to
        // existing entries.
        if(self.scrollPositionY().animation().target() > 0)
        {
            self.scrollPositionY().shift(delta);

            //emit self.scrollPositionChanged(visibleOffset.target());
        }
    }

    void fetchNewCachedEntries()
    {
        while(CacheEntry *cached = sink.nextCachedEntry())
        {
            // Find a suitable place according to the original index in the sink;
            // the task pool may output the entries slightly out of order as
            // multiple threads may be in use.
            /*int pos = cache.size();
            while(pos > 0 && cache.at(pos - 1)->sinkIndex > cached->sinkIndex)
            {
                --pos;
            }
            cache.insert(pos, cached);*/

            cache << cached;

#if 0
            self.modifyContentHeight(cached->height());

            // Adjust visible offset so it remains fixed in relation to
            // existing entries.
            if(self.scrollPositionY().animation().target() > 0)
            {
                self.scrollPositionY().shift(cached->height());

                //emit self.scrollPositionChanged(visibleOffset.target());
            }
#endif
        }
    }

    void rewrapCache()
    {
        /*if(cache.isEmpty()) return;

        if(isRewrapping())
        {
            // Cancel an existing rewrap.
            cancelRewrap++;
        }

        // Start a rewrapping task that goes through all the existing entries,
        // starting from the latest entry.
        rewrapPool.start(new RewrapTask(this, cache.size() - 1, contentWidth()));*/

        for(int idx = cache.size() - 1; idx >= 0; --idx)
        {
            cache[idx]->rewrap(contentWidth());
        }
    }

    /*
    bool isRewrapping() const
    {
        //return !rewrapPool.isDone();
        return numberOfUnwrappedEntries > 0;
    }
    */

    void releaseExcessComposedEntries()
    {
        if(visibleRange < 0) return;

        int len = de::max(10, visibleRange.size());

        // Excess entries before the visible range.
        int excess = visibleRange.start - len;
        for(int i = 0; i <= excess && i < cache.size(); ++i)
        {
            cache[i]->clear();
        }

        // Excess entries after the visible range.
        excess = visibleRange.end + len;
        for(int i = excess; i < cache.size(); ++i)
        {
            cache[i]->clear();
        }
    }

    /**
     * Releases all entries currently stored in the entry atlas.
     */
    void releaseAllNonVisibleEntries()
    {
        for(int i = 0; i < cache.size(); ++i)
        {
            if(!visibleRange.contains(i))
            {
                cache[i]->clear();
            }
        }
    }

    /**
     * Removes entries from the sink and the cache.
     */
    void prune()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();
#if 0
        if(isRewrapping())
        {
            // Rewrapper is busy, let's not do this right now.
            return;
        }

        // We must lock the sink so no new entries are added.
        DENG2_GUARD(sink);

        /*if(sink.isBusy())
        {
            // New entries are being added, prune later.
            return;
        }*/

        fetchNewCachedEntries();

        DENG2_ASSERT(sink.entryCount() == cache.size());
#endif
#if 0
        // We must lock the sink so no new entries are added.
        DENG2_GUARD(sink);

        fetchNewCachedEntries();

        // There has to be a cache entry for each sink entry.
        DENG2_ASSERT(sink.entryCount() == cache.size());

        int num = sink.entryCount() - sink.maxEntries();
        if(num > 0)
        {
            sink.remove(0, num);
            for(int i = 0; i < num; ++i)
            {
                self.modifyContentHeight(-cache[0]->height());
                delete cache.takeFirst();
            }
            /*
            // Adjust existing indices to match.
            for(int i = 0; i < cache.size(); ++i)
            {
                cache[i]->sinkIndex -= num;
            }*/
        }
#endif
    }

    void updateProjection()
    {
        projMatrix = root().projMatrix2D();

        uBgMvpMatrix = projMatrix;
    }

    void updateEntries()
    {
        int oldHeight = self.contentHeight();

        for(int idx = cache.size() - 1; idx >= 0; --idx)
        {
            CacheEntry *entry = cache[idx];

            int delta = entry->updateHeightOnly();
            if(delta)
            {
                // The new height will be effective on the next frame.
                modifyContentHeight(delta);
            }
        }

        if(self.contentHeight() > oldHeight)
        {
            emit self.contentHeightIncreased(self.contentHeight() - oldHeight);
        }
    }

    Rangei extendPixelRangeWithPadding(Rangei const &range)
    {
        int const padding = range.size() / 2;
        return Rangei(range.start - padding, range.end + padding);
    }

    void updateGeometry()
    {
        Vector2i const contentSize = self.viewportSize();

        // If the width of the widget changes, text needs to be reflowed with the
        // new width.
        if(cacheWidth != contentSize.x)
        {
            rewrapCache();
            cacheWidth = contentSize.x;
        }

        updateEntries();

        // If the atlas becomes full, we'll retry once.
        entryAtlasFull = false;

        VertexBuf::Builder verts;

        // Draw in reverse, as much as we need.
        int initialYBottom = contentSize.y + self.scrollPositionY().valuei();
        contentOffsetForDrawing = std::ceil(contentOffset.value());

        Rangei const visiblePixelRange = extendPixelRangeWithPadding(
                    Rangei(-contentOffsetForDrawing, contentSize.y - contentOffsetForDrawing));

        for(int attempt = 0; attempt < 2; ++attempt)
        {
            if(entryAtlasFull)
            {
                releaseAllNonVisibleEntries();
                entryAtlasFull = false;
            }

            int yBottom = initialYBottom;
            visibleRange = Rangei(-1, -1);
            entryAtlasLayoutChanged = false;

            bool gotReady = false;

            // Find the visible range and update all visible entries.
            for(int idx = cache.size() - 1; yBottom >= -contentOffsetForDrawing && idx >= 0; --idx)
            {
                CacheEntry *entry = cache[idx];

                entry->update(yBottom, visiblePixelRange);

                if(gotReady && !entry->isReady())
                {
                    // Anything above this has an undefined position, so we must stop here.
                    break;
                }

                yBottom -= entry->height();

                if(entry->isReady() && yBottom + contentOffsetForDrawing <= contentSize.y)
                {
                    gotReady = true;

                    // Rasterize and allocate if needed.
                    entry->make(verts, yBottom);

                    // Update the visible range.
                    if(visibleRange.end == -1)
                    {
                        visibleRange.end = idx;
                    }
                    visibleRange.start = idx;
                }

                if(entryAtlasLayoutChanged || entryAtlasFull)
                {
                    goto nextAttempt;
                }
            }

            // Successfully completed.
            break;

nextAttempt:
            // Oops, the atlas was optimized during the loop and some items'
            // positions are obsolete.
            verts.clear();
        }

        // Draw the scroll indicator, too.
        self.glMakeScrollIndicatorGeometry(verts);

        buf->setVertices(gl::TriangleStrip, verts, gl::Dynamic);
    }

    void draw()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || !bgBuf->isReady())
        {
            // Update the background quad.
            VertexBuf::Builder bgVerts;
            self.glMakeGeometry(bgVerts);
            bgBuf->setVertices(gl::TriangleStrip, bgVerts, gl::Static);
        }

        background.draw();

        Rectanglei vp = self.viewport();
        if(vp.height() > 0)
        {
            GLState &st = GLState::push();

            // Leave room for the indicator in the scissor.
            st.setNormalizedScissor(
                    self.normalizedRect(
                            vp.adjusted(Vector2i(), Vector2i(self.margins().right().valuei(), 0))));

            // First draw the shadow of the text.
            uMvpMatrix = projMatrix * Matrix4f::translate(
                         Vector2f(vp.topLeft + Vector2i(0, contentOffsetForDrawing)));
            uShadowColor = Vector4f(0, 0, 0, 1);
            contents.draw();

            // Draw the text itself.
            uMvpMatrix = projMatrix * Matrix4f::translate(
                         Vector2f(vp.topLeft + Vector2i(0, contentOffsetForDrawing - 1)));
            uShadowColor = Vector4f(1, 1, 1, 1);
            contents.draw();

            GLState::pop();
        }

        // We don't need to keep all entries ready for drawing immediately.
        releaseExcessComposedEntries();
    }
};

LogWidget::LogWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{
    setOrigin(Bottom);

    //connect(&d->rewrapPool, SIGNAL(allTasksDone()), this, SLOT(pruneExcessEntries()));

    LogBuffer::appBuffer().addSink(d->sink);
}

void LogWidget::setLogFormatter(LogSink::IFormatter &formatter)
{
    d->formatter = &formatter;
}

LogSink &LogWidget::logSink()
{
    return d->sink;
}

void LogWidget::clear()
{
    d->clear();
}

void LogWidget::setContentYOffset(Animation const &anim)
{
    if(isAtBottom())
    {
        d->contentOffset = anim;
    }
    else
    {
        // When not at the bottom, the content is expected to stay fixed in place.
        d->contentOffset = 0;
    }
}

Animation const &LogWidget::contentYOffset() const
{
    return d->contentOffset;
}

void LogWidget::viewResized()
{
    GuiWidget::viewResized();

    d->updateProjection();
}

void LogWidget::update()
{
    ScrollAreaWidget::update();

    d->sink.setWidth(d->contentWidth());
    d->fetchNewCachedEntries();
    d->prune();

    // The log widget's geometry is fully dynamic -- regenerated on every frame.
    d->updateGeometry();
}

void LogWidget::drawContent()
{
    d->draw();
}

bool LogWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void LogWidget::pruneExcessEntries()
{
    d->prune();
}

void LogWidget::glInit()
{
    d->glInit();
}

void LogWidget::glDeinit()
{
    d->glDeinit();
}

} // namespace de
