/** @file materialsnapshot.cpp Material Snapshot.
 *
 * @authors Copyright � 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>
#include <de/Log>
#include "de_base.h"
#ifdef __CLIENT__
#  include "de_defs.h"
#endif

#ifdef __CLIENT__
#  include "render/rend_main.h" // detailFactor, detailScale, smoothTexAnim, etc...
#  include "gl/gl_texmanager.h"
#  include "gl/sys_opengl.h"
#endif

#include "resource/material.h"
#include "resource/texture.h"

#include "resource/materialsnapshot.h"

namespace de {

struct Store {
    /// @c true= this material is completely opaque.
    bool opaque;

    /// Glow strength factor.
    float glowStrength;

    /// Dimensions in the world coordinate space.
    QSize dimensions;

    /// Minimum ambient light color for shine texture.
    Vector3f shineMinColor;

    /// Textures used on each logical material texture unit.
    Texture::Variant *textures[NUM_MATERIAL_TEXTURE_UNITS];

#ifdef __CLIENT__
    /// Decoration configuration.
    MaterialSnapshot::Decoration decorations[Material::max_decorations];

    /// Prepared render texture unit configuration. These map directly
    /// to the texture units supplied to the render lists module.
    rtexmapunit_t units[NUM_TEXMAP_UNITS];
#endif

    Store() { initialize(); }

    void initialize()
    {
        dimensions    = QSize(0, 0);
        shineMinColor = Vector3f(0, 0, 0);
        opaque        = true;
        glowStrength  = 0;

        std::memset(textures, 0, sizeof(textures));

#ifdef __CLIENT__
        std::memset(decorations, 0, sizeof(decorations));

        for(int i = 0; i < NUM_TEXMAP_UNITS; ++i)
        {
            Rtu_Init(&units[i]);
        }
#endif
    }

#ifdef __CLIENT__
    void writeTexUnit(rtexmapunitid_t unit, Texture::Variant *texture,
                      blendmode_t blendMode, QSizeF scale, QPointF offset,
                      float opacity)
    {
        DENG2_ASSERT(unit >= 0 && unit < NUM_TEXMAP_UNITS);
        rtexmapunit_t &tu = units[unit];

        tu.texture.variant = reinterpret_cast<texturevariant_s *>(texture);
        tu.texture.flags   = TUF_TEXTURE_IS_MANAGED;
        tu.opacity   = MINMAX_OF(0, opacity, 1);
        tu.blendMode = blendMode;
        V2f_Set(tu.scale, scale.width(), scale.height());
        V2f_Set(tu.offset, offset.x(), offset.y());
    }
#endif // __CLIENT__
};

struct MaterialSnapshot::Instance
{
    /// Variant material used to derive this snapshot.
    Material::Variant *variant;

    Store stored;

    Instance(Material::Variant &_variant)
        : variant(&_variant), stored()
    {}

    void takeSnapshot();
};

MaterialSnapshot::MaterialSnapshot(Material::Variant &materialVariant)
{
    d = new Instance(materialVariant);
}

MaterialSnapshot::~MaterialSnapshot()
{
    delete d;
}

Material::Variant &MaterialSnapshot::materialVariant() const
{
    return *d->variant;
}

QSize const &MaterialSnapshot::dimensions() const
{
    return d->stored.dimensions;
}

bool MaterialSnapshot::isOpaque() const
{
    return d->stored.opaque;
}

float MaterialSnapshot::glowStrength() const
{
    return d->stored.glowStrength;
}

Vector3f const &MaterialSnapshot::shineMinColor() const
{
    return d->stored.shineMinColor;
}

bool MaterialSnapshot::hasTexture(int index) const
{
    if(index < 0 || index >= NUM_MATERIAL_TEXTURE_UNITS) return false;
    return d->stored.textures[index] != 0;
}

Texture::Variant &MaterialSnapshot::texture(int index) const
{
    if(!hasTexture(index))
    {
        /// @throw UnknownUnitError Attempt to dereference with an invalid index.
        throw UnknownUnitError("MaterialSnapshot::texture", QString("Invalid texture index %1").arg(index));
    }
    return *d->stored.textures[index];
}

#ifdef __CLIENT__
rtexmapunit_t const &MaterialSnapshot::unit(rtexmapunitid_t id) const
{
    if(id < 0 || id >= NUM_TEXMAP_UNITS)
    {
        /// @throw UnknownUnitError Attempt to obtain a reference to a unit with an invalid id.
        throw UnknownUnitError("MaterialSnapshot::unit", QString("Invalid unit id %1").arg(id));
    }
    return d->stored.units[id];
}

MaterialSnapshot::Decoration &MaterialSnapshot::decoration(int index) const
{
    if(index < 0 || index >= Material::max_decorations)
    {
        /// @throw UnknownDecorationError Attempt to obtain a reference to a decoration with an invalid index.
        throw UnknownDecorationError("MaterialSnapshot::decoration", QString("Invalid decoration index %1").arg(index));
    }
    return d->stored.decorations[index];
}

/// @todo Optimize: Cache this result at material level.
static Texture *findTextureForLayerStage(ded_material_layer_stage_t const &def)
{
    try
    {
        return App_Textures()->find(*reinterpret_cast<de::Uri *>(def.texture)).texture();
    }
    catch(Textures::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

/// @todo Optimize: Cache this result at material level.
static Texture *findTextureForDetailLayerStage(ded_detail_stage_t const &def)
{
    try
    {
        return App_Textures()->scheme("Details").findByResourceUri(*reinterpret_cast<de::Uri *>(def.texture)).texture();
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}

/// @todo Optimize: Cache this result at material level.
static Texture *findTextureByResourceUri(String nameOfScheme, de::Uri const &resourceUri)
{
    if(resourceUri.isEmpty()) return 0;
    try
    {
        return App_Textures()->scheme(nameOfScheme).findByResourceUri(resourceUri).texture();
    }
    catch(Textures::Scheme::NotFoundError const &)
    {} // Ignore this error.
    return 0;
}
#endif // __CLIENT__

/// @todo Implement more useful methods of interpolation. (What do we want/need here?)
void MaterialSnapshot::Instance::takeSnapshot()
{
#define LERP(start, end, pos) (end * pos + start * (1 - pos))

    Material *material = &variant->generalCase();
    MaterialVariantSpec const &spec = variant->spec();
    Material::Layers const &layers = material->layers();
#ifdef __CLIENT__
    Material::DetailLayer const *detailLayer = material->isDetailed()? &material->detailLayer() : 0;
#endif

    Texture::Variant *prepTextures[NUM_MATERIAL_TEXTURE_UNITS][2];
    std::memset(prepTextures, 0, sizeof prepTextures);

    // Reinitialize the stored values.
    stored.initialize();

#ifdef __CLIENT__
    /*
     * Ensure all resources needed to visualize this have been prepared.
     *
     * If skymasked, we only need to update the primary tex unit (due to
     * it being visible when skymask debug drawing is enabled).
     */
    for(int i = 0; i < layers.count(); ++i)
    {
        Material::Variant::LayerState const &l = variant->layer(i);

        ded_material_layer_stage_t const *lsCur = layers[i]->stages()[l.stage];
        if(Texture *tex = findTextureForLayerStage(*lsCur))
        {
            // Pick the instance matching the specified context.
            preparetextureresult_t result;
            prepTextures[i][0] = GL_PrepareTexture(*tex, *spec.primarySpec, &result);

            // Primary texture was (re)prepared?
            if(i == 0 && l.stage == 0 &&
               (PTR_UPLOADED_ORIGINAL == result || PTR_UPLOADED_EXTERNAL == result))
            {
                // Are we inheriting the logical dimensions from the texture?
                if(material->dimensions().isEmpty())
                {
                    material->setDimensions(tex->dimensions());
                }
            }
        }

        // Smooth Texture Animation?
        if(!smoothTexAnim || layers[i]->stageCount() < 2) continue;

        ded_material_layer_stage_t const *lsNext = layers[i]->stages()[(l.stage + 1) % layers[i]->stageCount()];
        if(Texture *tex = findTextureForLayerStage(*lsNext))
        {
            // Pick the instance matching the specified context.
            preparetextureresult_t result;
            prepTextures[i][1] = GL_PrepareTexture(*tex, *spec.primarySpec, &result);
        }
    }

    // Do we need to prepare detail texture(s)?
    if(!material->isSkyMasked() && material->isDetailed())
    {
        Material::Variant::LayerState const &l = variant->detailLayer();
        ded_detail_stage_t const *lsCur = detailLayer->stages()[l.stage];

        float const contrast = MINMAX_OF(0, lsCur->strength, 1) * detailFactor /*Global strength multiplier*/;
        texturevariantspecification_t &texSpec = *GL_DetailTextureVariantSpecificationForContext(contrast);
        if(Texture *tex = findTextureForDetailLayerStage(*lsCur))
        {
            // Pick the instance matching the specified context.
            prepTextures[MTU_DETAIL][0] = GL_PrepareTexture(*tex, texSpec);
        }

        // Smooth Texture Animation?
        if(smoothTexAnim && detailLayer->stageCount() > 1)
        {
            ded_detail_stage_t const *lsNext = detailLayer->stages()[(l.stage + 1) % detailLayer->stageCount()];
            if(Texture *tex = findTextureForDetailLayerStage(*lsNext))
            {
                // Pick the instance matching the specified context.
                prepTextures[MTU_DETAIL][1] = GL_PrepareTexture(*tex, texSpec);
            }
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(!material->isSkyMasked() && material->isShiny())
    {
        Material::Variant::ShineLayerState &shiny = variant->shineLayer();

        Uri uri(material->manifest().composeUri());
        ded_reflection_t const *refDef =
            Def_GetReflection(reinterpret_cast<uri_s *>(&uri)/*,
                              result == PTR_UPLOADED_EXTERNAL, manifest.isCustom()*/);
        DENG_ASSERT(refDef);

        shiny.texture      = refDef->stage.texture? findTextureByResourceUri("Reflections", reinterpret_cast<de::Uri const&>(*refDef->stage.texture)) : 0;
        shiny.maskTexture  = refDef->stage.maskTexture? findTextureByResourceUri("Masks", reinterpret_cast<de::Uri const&>(*refDef->stage.maskTexture)) : 0;
        shiny.strength     = MINMAX_OF(0, refDef->stage.shininess, 1);
        shiny.blendmode    = refDef->stage.blendMode;
        DENG2_ASSERT(VALID_BLENDMODE(shiny.blendmode));
        shiny.minColor[CR] = MINMAX_OF(0, refDef->stage.minColor[CR], 1);
        shiny.minColor[CG] = MINMAX_OF(0, refDef->stage.minColor[CG], 1);
        shiny.minColor[CB] = MINMAX_OF(0, refDef->stage.minColor[CB], 1);

        if(Texture *tex = shiny.texture)
        {
            texturevariantspecification_t &texSpec =
                *GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_REFLECTION,
                     TSF_NO_COMPRESSION, 0, 0, 0, GL_REPEAT, GL_REPEAT, 1, 1, -1,
                     false, false, false, false);

            prepTextures[MTU_REFLECTION][0] = GL_PrepareTexture(*tex, texSpec);
        }

        // We are only interested in a mask if we have a shiny texture.
        if(prepTextures[MTU_REFLECTION][0])
        if(Texture *tex = shiny.maskTexture)
        {
            texturevariantspecification_t &texSpec =
                *GL_TextureVariantSpecificationForContext(
                     TC_MAPSURFACE_REFLECTIONMASK, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                     -1, -1, -1, true, false, false, false);

            prepTextures[MTU_REFLECTION_MASK][0] = GL_PrepareTexture(*tex, texSpec);
        }
    }
#endif // __CLIENT__

    stored.dimensions.setWidth(material->width());
    stored.dimensions.setHeight(material->height());

#ifdef __CLIENT__
    stored.opaque = (prepTextures[MTU_PRIMARY][0] && !prepTextures[MTU_PRIMARY][0]->isMasked());
#endif

    if(stored.dimensions.isEmpty()) return;

    Material::Variant::LayerState const &l   = variant->layer(0);
    ded_material_layer_stage_t const *lsCur  = layers[0]->stages()[l.stage];
    ded_material_layer_stage_t const *lsNext = layers[0]->stages()[(l.stage + 1) % layers[0]->stageCount()];

    // Glow strength is presently taken from layer #0.
    if(l.inter == 0)
    {
        stored.glowStrength = lsCur->glowStrength;
    }
    else // Interpolate.
    {
        stored.glowStrength = LERP(lsCur->glowStrength, lsNext->glowStrength, l.inter);
    }

    if(MC_MAPSURFACE == spec.context && prepTextures[MTU_REFLECTION][0])
    {
        stored.shineMinColor = Vector3f(variant->shineLayer().minColor);
    }

    // Setup the primary texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_PRIMARY][0])
    {
        stored.textures[MTU_PRIMARY] = tex;
#ifdef __CLIENT__
        QPointF offset;
        if(l.inter == 0)
        {
            offset = QPointF(lsCur->texOrigin[0], lsCur->texOrigin[1]);
        }
        else // Interpolate.
        {
            offset.setX(LERP(lsCur->texOrigin[0], lsNext->texOrigin[0], l.inter));
            offset.setY(LERP(lsCur->texOrigin[1], lsNext->texOrigin[1], l.inter));
        }

        stored.writeTexUnit(RTU_PRIMARY, tex, BM_NORMAL,
                            QSizeF(1.f / stored.dimensions.width(),
                                   1.f / stored.dimensions.height()),
                            offset, 1);
#endif
    }

#ifdef __CLIENT__
    // Setup the inter primary texture unit.
    if(Texture::Variant *tex = prepTextures[MTU_PRIMARY][1])
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(!(!usingFog && l.inter == 0))
        {
            stored.writeTexUnit(RTU_INTER, tex, BM_NORMAL,
                                QSizeF(stored.units[RTU_PRIMARY].scale[0],
                                       stored.units[RTU_PRIMARY].scale[1]),
                                QPointF(stored.units[RTU_PRIMARY].offset[0],
                                        stored.units[RTU_PRIMARY].offset[1]),
                                l.inter);
        }
    }
#endif

    if(!material->isSkyMasked() && material->isDetailed())
    {
#ifdef __CLIENT__
        Material::Variant::LayerState const &l = variant->detailLayer();
        ded_detail_stage_t const *lsCur  = detailLayer->stages()[l.stage];
        ded_detail_stage_t const *lsNext = detailLayer->stages()[(l.stage + 1) % detailLayer->stageCount()];
#endif

        // Setup the detail texture unit.
        if(Texture::Variant *tex = prepTextures[MTU_DETAIL][0])
        {
            stored.textures[MTU_DETAIL] = tex;
#ifdef __CLIENT__
            float scale;
            if(l.inter == 0)
            {
                scale = lsCur->scale;
            }
            else // Interpolate.
            {
                scale = LERP(lsCur->scale, lsNext->scale, l.inter);
            }

            // Apply the global scale factor.
            if(detailScale > .0001f)
                scale *= detailScale;

            stored.writeTexUnit(RTU_PRIMARY_DETAIL, tex, BM_NORMAL,
                                QSizeF(1.f / tex->generalCase().width()  * scale,
                                       1.f / tex->generalCase().height() * scale),
                                QPointF(0, 0), 1);
#endif
        }

#ifdef __CLIENT__
        // Setup the inter detail texture unit.
        if(Texture::Variant *tex = prepTextures[MTU_DETAIL][1])
        {
            // If fog is active, inter=0 is accepted as well. Otherwise
            // flickering may occur if the rendering passes don't match for
            // blended and unblended surfaces.
            if(!(!usingFog && l.inter == 0))
            {
                stored.writeTexUnit(RTU_INTER_DETAIL, tex, BM_NORMAL,
                                    QSizeF(stored.units[RTU_PRIMARY_DETAIL].scale[0],
                                           stored.units[RTU_PRIMARY_DETAIL].scale[1]),
                                    QPointF(stored.units[RTU_PRIMARY_DETAIL].offset[0],
                                            stored.units[RTU_PRIMARY_DETAIL].offset[1]),
                                    l.inter);
            }
        }
#endif
    }

    // Setup the shiny texture units.
    if(Texture::Variant *tex = prepTextures[MTU_REFLECTION][0])
    {
        stored.textures[MTU_REFLECTION] = tex;
#ifdef __CLIENT__
        Material::Variant::ShineLayerState const &shiny = variant->shineLayer();
        stored.writeTexUnit(RTU_REFLECTION, tex, shiny.blendmode,
                            QSizeF(1, 1), QPointF(0, 0), shiny.strength);
#endif
    }

    if(prepTextures[MTU_REFLECTION][0])
    if(Texture::Variant *tex = prepTextures[MTU_REFLECTION_MASK][0])
    {
        stored.textures[MTU_REFLECTION_MASK] = tex;
#ifdef __CLIENT__
        stored.writeTexUnit(RTU_REFLECTION_MASK, tex, BM_NORMAL,
                            QSizeF(1.f / (stored.dimensions.width()  * tex->generalCase().width()),
                                   1.f / (stored.dimensions.height() * tex->generalCase().height())),
                            QPointF(stored.units[RTU_PRIMARY].offset[0],
                                    stored.units[RTU_PRIMARY].offset[1]), 1);
#endif
    }

#ifdef __CLIENT__
    uint idx = 0;
    Material::Decorations const &decorations = material->decorations();
    for(Material::Decorations::const_iterator it = decorations.begin();
        it != decorations.end(); ++it, ++idx)
    {
        Material::Variant::DecorationState const &l = variant->decoration(idx);
        Material::Decoration const *lDef = *it;
        ded_decorlight_stage_t const *lsCur  = lDef->stages()[l.stage];
        ded_decorlight_stage_t const *lsNext = lDef->stages()[(l.stage + 1) % lDef->stageCount()];

        MaterialSnapshot::Decoration &decor = stored.decorations[idx];

        if(l.inter == 0)
        {
            decor.pos[0]         = lsCur->pos[0];
            decor.pos[1]         = lsCur->pos[1];
            decor.elevation      = lsCur->elevation;
            decor.radius         = lsCur->radius;
            decor.haloRadius     = lsCur->haloRadius;
            decor.lightLevels[0] = lsCur->lightLevels[0];
            decor.lightLevels[1] = lsCur->lightLevels[1];

            std::memcpy(decor.color, lsCur->color, sizeof(decor.color));
        }
        else // Interpolate.
        {
            decor.pos[0]         = LERP(lsCur->pos[0], lsNext->pos[0], l.inter);
            decor.pos[1]         = LERP(lsCur->pos[1], lsNext->pos[1], l.inter);
            decor.elevation      = LERP(lsCur->elevation, lsNext->elevation, l.inter);
            decor.radius         = LERP(lsCur->radius, lsNext->radius, l.inter);
            decor.haloRadius     = LERP(lsCur->haloRadius, lsNext->haloRadius, l.inter);
            decor.lightLevels[0] = LERP(lsCur->lightLevels[0], lsNext->lightLevels[0], l.inter);
            decor.lightLevels[1] = LERP(lsCur->lightLevels[1], lsNext->lightLevels[1], l.inter);

            for(int c = 0; c < 3; ++c)
            {
                decor.color[c] = LERP(lsCur->color[c], lsNext->color[c], l.inter);
            }
        }

        decor.tex      = GL_PrepareLightmap(lsCur->sides);
        decor.ceilTex  = GL_PrepareLightmap(lsCur->up);
        decor.floorTex = GL_PrepareLightmap(lsCur->down);
        decor.flareTex = GL_PrepareFlareTexture(lsCur->flare, lsCur->flareTexture);
    }
#endif // __CLIENT__

#undef LERP
}

void MaterialSnapshot::update()
{
    d->takeSnapshot();
}

} // namespace de
