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

#include <openspace/util/camera.h>

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

#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <cstdlib>
#include <locale>

namespace {
    constexpr const std::array<const char*, 6> UniformNames = {
        "Color", "MVP", "sunPosition", "sunIntensity", "shadowMap", "shadowMatrix"
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
        , _shadowDepthTextureHeight(2048)
        , _shadowDepthTextureWidth(2048)
        , _shadowDepthTexture(-1)
        , _shadowFBO(-1)
        , _firstPassSubroutine(-1)
        , _secondPassSubroutine(1)
        , _defaultFBO(-1)
        , _sunPosition(0.0)
        , _shadowMatrix(1.0)

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
        _shaderProgram = ghoul::opengl::ProgramObject::Build(
            "ShadowProgram",
            absPath("${MODULE_GLOBEBROWSING}/shaders/shadow_vs.glsl"),
            absPath("${MODULE_GLOBEBROWSING}/shaders/shadow_fs.glsl")
        );

        ghoul::opengl::updateUniformLocations(*_shaderProgram, _uniformCache, UniformNames);
        
        _firstPassSubroutine = glGetSubroutineIndex(
            *_shaderProgram, 
            GL_FRAGMENT_SHADER, 
            "recordDepth"
        );
        _secondPassSubroutine = glGetSubroutineIndex(
            *_shaderProgram, 
            GL_FRAGMENT_SHADER, 
            "shadeWithShadow"
        );

        createDepthTexture();
        createShadowFBO();
    }

    void ShadowComponent::deinitializeGL() {
        glDeleteTextures(1, &_shadowDepthTexture);
        glDeleteFramebuffers(1, &_shadowFBO);
    }

    void ShadowComponent::begin(const RenderData& data) {

        //_shaderProgram->activate();

        // Texture coords in [0, 1], while clip coords in [-1, 1]
        glm::dmat4 toTextureCoordsMatrix = glm::dmat4(
            glm::vec4(0.5, 0.0, 0.0, 0.0),
            glm::vec4(0.0, 0.5, 0.0, 0.0),
            glm::vec4(0.0, 0.0, 0.5, 0.0),
            glm::vec4(0.5, 0.5, 0.5, 1.0)
        );

        // Builds light's ModelViewProjectionMatrix:
        /*glm::dvec3 lightPosition = glm::dvec3(_sunPosition);
        glm::dvec3 lightDirection = glm::normalize(glm::dvec3(data.modelTransform.translation - lightPosition));
        glm::dvec3 lightUpVector = glm::cross(data.camera.lookUpVectorWorldSpace(), lightDirection);
        glm::dvec3 lightRightVector = glm::cross(lightDirection, lightUpVector);

        glm::dmat4 lightViewMatrix = glm::dmat4(
            glm::dvec4(lightUpVector, 0.0),
            glm::dvec4(lightRightVector, 0.0),
            glm::dvec4(lightDirection, 0.0),
            glm::dvec4(0.0, 0.0, 0.0, 1.0)
        );
        lightViewMatrix = lightViewMatrix * glm::translate(glm::dmat4(1.0), -lightPosition);*/

        // Camera matrix
        glm::dmat4 lightViewMatrix = glm::lookAt(
            glm::dvec3(_sunPosition), // position
            glm::dvec3(data.modelTransform.translation), // looks at 
            data.camera.lookUpVectorWorldSpace()  // up
        );

        glm::dmat4 lightProjectionMatrix = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(_shadowDepthTextureWidth) / static_cast<float>(_shadowDepthTextureHeight),
            0.1f,
            100.0f
        );

        glm::dmat4 lightModelViewProjectionMatrix = lightProjectionMatrix * lightViewMatrix *
            glm::translate(glm::dmat4(1.0), -glm::dvec3(_sunPosition));

        /*_shadowMatrix = toTextureCoordsMatrix * lightModelViewProjectionMatrix;

        _shaderProgram->setUniform(_uniformCache.shadowMatrix, _shadowMatrix);
        _shaderProgram->setUniform(_uniformCache.sunIntensity, glm::vec3(0.85f));
        _shaderProgram->setUniform(_uniformCache.shadowMap, _shadowDepthTexture);*/


        /*_cameraPos = data.camera.eyePositionVec3();
        _cameraFocus = data.camera.focusPositionVec3();

        Camera *camera = OsEng.renderEngine().camera();
        camera->setPositionVec3(glm::dvec3(_sunPosition));
        camera->setFocusPositionVec3(data.modelTransform.translation);*/
        
        // Saves current state
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);
        glGetIntegerv(GL_VIEWPORT, _mViewport);
        _faceCulling = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &_faceToCull);

        glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, _shadowDepthTextureWidth, _shadowDepthTextureHeight);
        glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &_firstPassSubroutine);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.5f, 10.0f);
        
    }

    void ShadowComponent::end(const RenderData& data) {
        //_shaderProgram->deactivate();
           
        glFlush();
        saveDepthBuffer();

        /*Camera *camera = OsEng.renderEngine().camera();
        camera->setPositionVec3(_cameraPos);
        camera->setFocusPositionVec3(_cameraFocus);
*/
        // Restores system state
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
        glViewport(
            _mViewport[0],
            _mViewport[1],
            _mViewport[2],
            _mViewport[3]
        );
        if (_faceCulling) {
            glEnable(GL_CULL_FACE);
            glCullFace(_faceToCull);
        }
        else {
            glDisable(GL_CULL_FACE);
        }
    }

    void ShadowComponent::update(const UpdateData& data) {
        _sunPosition = glm::normalize(
            OsEng.renderEngine().scene()->sceneGraphNode("Sun")->worldPosition() -
            data.modelTransform.translation
        );
    }

    void ShadowComponent::createDepthTexture() {
        glGenTextures(1, &_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, _shadowDepthTexture);
        glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            GL_DEPTH_COMPONENT32F,
            _shadowDepthTextureWidth, 
            _shadowDepthTextureHeight
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, shadowBorder);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
    }

    void ShadowComponent::createShadowFBO() {
        // Saves current FBO first
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);

        /*GLint _mViewport[4];
        glGetIntegerv(GL_VIEWPORT, _mViewport);*/

        glGenFramebuffers(1, &_shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, 
            GL_DEPTH_ATTACHMENT, 
            GL_TEXTURE_2D, 
            _shadowDepthTexture, 
            0
        );
        GLenum drawBuffers[] = { GL_NONE };
        glDrawBuffers(1, drawBuffers);

        // Restores system state
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
        /*glViewport(
            _mViewport[0],
            _mViewport[1],
            _mViewport[2],
            _mViewport[3]
        );*/
    }

    void ShadowComponent::saveDepthBuffer() {
        int size = _shadowDepthTextureWidth * _shadowDepthTextureHeight;
        float * buffer = new float[size];

        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, buffer);
        
        std::fstream ppmFile;

        ppmFile.open("depthBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight << std::endl;
            ppmFile << "255" << std::endl;

            std::cout << "\n\nFILE\n\n";

            for (int i = 0; i < _shadowDepthTextureHeight; i++) {
                for (int j = 0; j < _shadowDepthTextureWidth; j++) {
                    int imgIdx = 3 * ((i * _shadowDepthTextureWidth) + j);
                    int bufIdx = ((_shadowDepthTextureHeight - i - 1) * _shadowDepthTextureWidth) + j;

                    float minVal = 0.88f;
                    float scale = (buffer[bufIdx] - minVal) / (1.0f - minVal);
                    unsigned char val = (unsigned char)(scale * 255);
                    ppmFile << val << " " << val << " " << val;
                }
                ppmFile << std::endl;
            }
            ppmFile.close();
        }

        delete[] buffer;
    }
} // namespace openspace
