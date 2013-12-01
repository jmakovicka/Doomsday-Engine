/** @file gl_model.h  3D model resource
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RESOURCE_MODEL_H
#define DENG_RESOURCE_MODEL_H

#include "Texture"
#include <de/Error>
#include <de/String>
#include <de/Vector>
#include <QBitArray>
#include <QList>
#include <QVector>

/// Unique identifier associated with each model in the collection.
typedef uint modelid_t;

/// Special value used to signify an invalid model id.
#define NOMODELID 0

class Model
{
public:
    /// Referenced frame is missing. @ingroup errors
    DENG2_ERROR(MissingFrameError);

    /// Referenced skin is missing. @ingroup errors
    DENG2_ERROR(MissingSkinError);

    /// Maximum levels of detail.
    static int const MAX_LODS = 4;

    /**
     * Classification/processing flags.
     */
    enum Flag {
        NoTextureCompression = 0x1 ///< Do not compress skin textures.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Animation key-frame.
     */
    struct Frame
    {
        struct Vertex {
            de::Vector3f pos;
            de::Vector3f norm;
        };
        typedef QVector<Vertex> VertexBuf;
        VertexBuf vertices;
        de::Vector3f min;
        de::Vector3f max;
        de::String name;

        Frame(de::String const &name = "") : name(name)
        {}

        void bounds(de::Vector3f &min, de::Vector3f &max) const;

        float horizontalRange(float *top, float *bottom) const;
    };
    typedef QList<Frame *> Frames;

    /**
     * Texture => Skin assignment.
     */
    struct Skin
    {
        de::String name;
        de::Texture *texture; // Not owned.

        Skin(de::String const &name = "", de::Texture *texture = 0)
            : name(name), texture(texture)
        {}
    };
    typedef QList<Skin> Skins;

    /**
     * Level of detail information.
     */
    struct DetailLevel
    {
        struct Primitive
        {
            struct Element
            {
                de::Vector2f texCoord;
                int index; ///< Index into the model's vertex mesh.
            };
            typedef QVector<Element> Elements;
            bool triFan; ///< @c true= triangle fan; otherwise triangle strip.
            Elements elements;
        };
        typedef QList<Primitive> Primitives;
        Primitives primitives;
    };
    typedef DetailLevel DetailLevels[MAX_LODS];

public:
    int _numVertices;       ///< Total number of vertices in the model.
    int _numLODs;           ///< Number of detail levels in use.
    DetailLevels _lods;     ///< Level of detail information.
    QBitArray _vertexUsage; ///< Denotes used vertices for each level of detail.

public:
    /**
     * Construct a new 3D model.
     */
    Model(uint modelId, Flags flags = 0);

    uint modelId() const;

    void setModelId(uint newId);

    /**
     * Returns a copy of the current model flags.
     */
    Flags flags() const;

    /**
     * Change the model's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Lookup a model animation frame by @a name.
     *
     * @return  Unique number of the found frame; otherwise @c -1 (not found).
     */
    int toFrameNumber(de::String name) const;

    /**
     * Convenient method of determining whether the specified model animation
     * frame @a number is valid (i.e., a frame is defined for it).
     */
    inline bool hasFrame(int number) const {
        return (number >= 0 && number < frameCount());
    }

    /**
     * Retrieve a model animation frame by it's unique frame @a number.
     */
    Frame &frame(int number) const;

    /**
     * Append a new animation frame to the model.
     *
     * @param newFrame  Ownership is given to the model.
     */
    void addFrame(Frame *newFrame);

    /**
     * Returns the total number of model animation frames.
     */
    inline int frameCount() const { return frames().count(); }

    /**
     * Provides access to the model animation frames, for efficient traversal.
     */
    Frames const &frames() const;

    /**
     * Clear all model animation frames.
     */
    void clearAllFrames();

    /**
     * Lookup a model skin by @a name.
     *
     * @return  Unique number of the found skin; otherwise @c -1 (not found).
     */
    int toSkinNumber(de::String name) const;

    /**
     * Convenient method of determining whether the specified model skin @a number
     * is valid (i.e., a skin is defined for it).
     */
    inline bool hasSkin(int number) const {
        return (number >= 0 && number < skinCount());
    }

    /**
     * Retrieve a model skin by it's unique @a number.
     */
    Skin &skin(int number) const;

    /**
     * Append a new skin with the given @a name to the model. If a skin already
     * exists with this name it will be returned instead.
     *
     * @return  Reference to the (possibly new) skin.
     */
    Skin &newSkin(de::String name);

    /**
     * Returns the total number of model skins.
     */
    inline int skinCount() const { return skins().count(); }

    /**
     * Provides access to the model skins, for efficient traversal.
     */
    Skins const &skins() const;

    /**
     * Clear all model skin assignments.
     */
    void clearAllSkins();

    /**
     * Returns the total number of detail levels for the model.
     */
    int lodCount() const;

    /**
     * Provides readonly access to the level of detail information.
     */
    DetailLevels const &lods() const;

    /// @todo Remove me.
    int numVertices() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Model::Flags)

typedef Model::DetailLevel ModelDetailLevel;
typedef Model::Frame ModelFrame;
typedef Model::Skin ModelSkin;

#endif // DENG_RESOURCE_MODEL_H
