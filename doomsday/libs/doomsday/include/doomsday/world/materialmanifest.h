/** @file materialmanifest.h  Description of a logical material resource.
 *
 * @authors Copyright © 2011-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MATERIALMANIFEST_H
#define LIBDOOMSDAY_WORLD_MATERIALMANIFEST_H

#include <de/Error>
#include <de/Observers>
#include <de/PathTree>
#include <de/Vector>

#include "../uri.h"
#include "Material"

namespace world {

class MaterialScheme;

/**
 * Description for a would-be logical Material resource.
 *
 * Models a reference to and the associated metadata for a logical material
 * in the material resource collection.
 *
 * @see MaterialScheme, Material
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC MaterialManifest : public de::PathTree::Node
{
public:
    /// Required material instance is missing. @ingroup errors
    DE_ERROR(MissingMaterialError);

    DE_DEFINE_AUDIENCE(Deletion,        void materialManifestBeingDeleted   (MaterialManifest const &manifest))
    DE_DEFINE_AUDIENCE(MaterialDerived, void materialManifestMaterialDerived(MaterialManifest &manifest, Material &material))

    enum Flag
    {
        /// The manifest was automatically produced for a game/add-on resource.
        AutoGenerated,

        /// The manifest was not produced for an original game resource.
        Custom
    };

    typedef std::function<Material * (MaterialManifest &)> MaterialConstructor;

public:
    MaterialManifest(de::PathTree::NodeArgs const &args);

    ~MaterialManifest();

    /**
     * Derive a new logical Material instance by interpreting the manifest.
     * The first time a material is derived from the manifest, said material
     * is assigned to the manifest (ownership is assumed).
     */
    Material *derive();

    void setScheme(MaterialScheme &scheme);

    /**
     * Returns the owning scheme of the manifest.
     */
    MaterialScheme &scheme() const;

    /// Convenience method for returning the name of the owning scheme.
    de::String const &schemeName() const;

    /**
     * Compose a URI of the form "scheme:path" for the material manifest.
     *
     * The scheme component of the URI will contain the symbolic name of
     * the scheme for the material manifest.
     *
     * The path component of the URI will contain the percent-encoded path
     * of the material manifest.
     */
    inline res::Uri composeUri(Char sep = '/') const {
        return res::Uri(schemeName(), path(sep));
    }

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    de::String description(res::Uri::ComposeAsTextFlags uriCompositionFlags = res::Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns a textual description of the source of the manifest.
     *
     * @return Human-friendly description of the source of the manifest.
     */
    de::String sourceDescription() const;

    /**
     * Returns the unique identifier associated with the manifest.
     */
    materialid_t id() const;

    void setId(materialid_t newId);

    /// @c true if the manifest was automatically produced for a game/add-on resource.
    inline bool isAutoGenerated() const { return isFlagged(AutoGenerated); }

    /// @c true if the manifest was not produced for an original game resource.
    inline bool isCustom() const { return isFlagged(Custom); }

    /**
     * Returns @c true if the manifest is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

    /**
     * Returns the flags for the manifest.
     */
    Flags flags() const;

    /**
     * Change the manifest's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true if a Material is presently associated with the manifest.
     *
     * @see material(), materialPtr()
     */
    bool hasMaterial() const;

    /**
     * Returns the logical Material associated with the manifest.
     *
     * @see hasMaterial()
     */
    Material &material() const;
    Material *materialPtr() const;

    /**
     * Change the material associated with the manifest.
     *
     * @param newMaterial  New material to associate with.
     */
    void setMaterial(Material *newMaterial);

    static void setMaterialConstructor(MaterialConstructor func);

private:
    DE_PRIVATE(d)
};

} // namespace world

#endif  // LIBDOOMSDAY_WORLD_MATERIALMANIFEST_H
