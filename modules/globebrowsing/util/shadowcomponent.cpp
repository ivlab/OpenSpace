/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014-2018                                                               *
*                                                                                       *
* Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
* software and associated documentation files (the "Software"), to deal in the Software *
* without restriction, including without limitation the rights to use, copy, modify,    *
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
* permit persons to whom the Software is furnished to do so, subject to the following   *
* conditions:                                                                           *
*                                                                                       *
* The above copyright notice and this permission notice shall be included in all copies *
* or substantial portions of the Software.                                              *
*                                                                                       *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
****************************************************************************************/

#include <modules/globebrowsing/util/shadowcomponent.h>
#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/globes/renderableglobe.h>

#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>
#include <openspace/util/updatestructures.h>

#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/engine/wrapper/windowwrapper.h>

#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>

#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>

#include <ghoul/font/fontmanager.h>
#include <ghoul/font/fontrenderer.h>

#include <fstream>
#include <cstdlib>
#include <locale>

namespace {
    constexpr const std::array<const char*, 6> UniformNames = {
        "modelViewProjectionTransform", "textureOffset", "transparency", "_nightFactor",
        "sunPosition", "texture1"
    };

    constexpr openspace::properties::Property::PropertyInfo TextureInfo = {
        "Texture",
        "Texture",
        "This value is the path to a texture on disk that contains a one-dimensional "
        "texture which is used for these rings."
    };

    constexpr openspace::properties::Property::PropertyInfo SizeInfo = {
        "Size",
        "Size",
        "This value specifies the radius of the rings in meter."
    };

    constexpr openspace::properties::Property::PropertyInfo OffsetInfo = {
        "Offset",
        "Offset",
        "This value is used to limit the width of the rings.Each of the two values is a "
        "value between 0 and 1, where 0 is the center of the ring and 1 is the maximum "
        "extent at the radius. If this value is, for example {0.5, 1.0}, the ring is "
        "only shown between radius/2 and radius. It defaults to {0.0, 1.0}."
    };

    constexpr openspace::properties::Property::PropertyInfo NightFactorInfo = {
        "NightFactor",
        "Night Factor",
        "This value is a multiplicative factor that is applied to the side of the rings "
        "that is facing away from the Sun. If this value is equal to '1', no darkening "
        "of the night side occurs."
    };

    constexpr openspace::properties::Property::PropertyInfo TransparencyInfo = {
        "Transparency",
        "Transparency",
        "This value determines the transparency of part of the rings depending on the "
        "color values. For this value v, the transparency is equal to length(color) / v."
    };
} // namespace

namespace openspace {

    documentation::Documentation ShadowComponent::Documentation() {
        using namespace documentation;
        return {
            "Rings Component",
            "globebrowsing_rings_component",
            {
                {
                    TextureInfo.identifier,
                    new StringVerifier,
                    Optional::Yes,
                    TextureInfo.description
                },
                {
                    SizeInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    SizeInfo.description
                },
                {
                    OffsetInfo.identifier,
                    new DoubleVector2Verifier,
                    Optional::Yes,
                    OffsetInfo.description
                },
                {
                    NightFactorInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    NightFactorInfo.description
                },
                {
                    TransparencyInfo.identifier,
                    new DoubleVerifier,
                    Optional::Yes,
                    TransparencyInfo.description
                }
            }
        };
    }

    ShadowComponent::ShadowComponent(const ghoul::Dictionary& dictionary)
        : properties::PropertyOwner({ "Shadows" })		
        , _shadowMapDictionary(dictionary)
    {
        using ghoul::filesystem::File;

        if (dictionary.hasKey("Shadow")) {
            dictionary.getValue("Shadow", _shadowMapDictionary);
        }

        documentation::testSpecificationAndThrow(
            Documentation(),
            _shadowMapDictionary,
            "ShadowComponent"
        );
    }

    void ShadowComponent::initialize()
    {
        using ghoul::filesystem::File;

        
    }

    bool ShadowComponent::isReady() const {
        return true;
    }

    void ShadowComponent::initializeGL() {
        
    }

    void ShadowComponent::deinitializeGL() {
        
    }

    void ShadowComponent::draw(const RenderData& data) {
        
    }

    void ShadowComponent::update(const UpdateData& data) {
        
    }

} // namespace openspace
