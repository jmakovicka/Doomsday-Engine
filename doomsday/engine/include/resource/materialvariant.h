/** @file materialvariant.h Logical Material Variant.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_RESOURCE_MATERIALVARIANT_H
#define LIBDENG_RESOURCE_MATERIALVARIANT_H

#include "render/rendpoly.h"
#include "materials.h"

struct texturevariant_s;
struct materialvariantspecification_s;

#define MATERIALVARIANT_MAXLAYERS       DDMAX_MATERIAL_LAYERS

typedef struct materialvariant_layer_s {
    int stage; // -1 => layer not in use.
    struct texture_s *texture;
    float texOrigin[2]; /// Origin of the texture in material-space.
    float glow;
    short tics;
} materialvariant_layer_t;

#ifdef __cplusplus

namespace de {

class MaterialSnapshot;

/**
 * @ingroup resource
 */
class MaterialVariant
{
public:
    MaterialVariant(material_t &generalCase, materialvariantspecification_s const &spec,
                    ded_material_t const &def);
    ~MaterialVariant();

    /**
     * Process a system tick event.
     * @param time  Length of the tick in seconds.
     */
    void ticker(timespan_t time);

    /**
     * Reset the staged animation point for this Material.
     */
    void resetAnim();

    /// @return  Material from which this variant is derived.
    material_t &generalCase() const { return *material; }

    /// @return  MaterialVariantSpecification from which this variant is derived.
    materialvariantspecification_s const &spec() const { return *varSpec; }

    /**
     * Retrieve a handle for a staged animation layer form this variant.
     * @param layer  Index of the layer to retrieve.
     * @return  MaterialVariantLayer for the specified layer index.
     */
    materialvariant_layer_t const *layer(int layer);

    /**
     * Attach MaterialSnapshot data to this. MaterialVariant is given ownership of @a materialSnapshot.
     * @return  Same as @a materialSnapshot for caller convenience.
     */
    MaterialSnapshot &attachSnapshot(MaterialSnapshot &snapshot);

    /**
     * Detach MaterialSnapshot data from this. Ownership of the data is relinquished to the caller.
     */
    MaterialSnapshot *detachSnapshot();

    /// @return  MaterialSnapshot data associated with this.
    MaterialSnapshot *snapshot() const { return snapshot_; }

    /// @return  Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame() const { return snapshotPrepareFrame_; }

    /**
     * Change the frame when the snapshot was last prepared/updated.
     * @param frame  Frame to mark the snapshot with.
     */
    void setSnapshotPrepareFrame(int frame);

    /// @return  Translated 'next' (or target) MaterialVariant if set, else this.
    MaterialVariant *translationNext();

    /// @return  Translated 'current' MaterialVariant if set, else this.
    MaterialVariant *translationCurrent();

    /// @return  Translation position [0..1]
    float translationPoint();

    /**
     * Change the translation target for this variant.
     *
     * @param current  Translated 'current' MaterialVariant.
     * @param next  Translated 'next' (or target) MaterialVariant.
     */
    void setTranslation(MaterialVariant *current, MaterialVariant *next);

    /**
     * Change the translation point for this variant.
     * @param inter  Translation point.
     */
    void setTranslationPoint(float inter);

private:
    /// Superior Material of which this is a derivative.
    material_t *material;

    /// For "smoothed" Material animation:
    bool hasTranslation;
    MaterialVariant *current;
    MaterialVariant *next;
    float inter;

    /// Specification used to derive this variant.
    materialvariantspecification_s const *varSpec;

    /// Cached copy of current state if any.
    MaterialSnapshot *snapshot_;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame_;

    materialvariant_layer_t layers[MATERIALVARIANT_MAXLAYERS];
};

} // namespace de

#endif // __cplusplus

struct materialvariant_s; // The materialvariant instance (opaque).

#endif /* LIBDENG_RESOURCE_MATERIALVARIANT_H */
