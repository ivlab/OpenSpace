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

#include <openspace/engine/globals.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/moduleengine.h>

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

    constexpr openspace::properties::Property::PropertyInfo SaveDepthTextureInfo = {
        "SaveDepthTextureInfo",
        "Save Depth Texture",
        "Debug"
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
        , _saveDepthTexture(SaveDepthTextureInfo)
        , _distanceFraction(SizeInfo, 10, 1, 100000)
        , _enabled({ "Enabled", "Enabled", "Enable/Disable Shadows" }, true)
        , _shadowMapDictionary(dictionary)
        , _shadowDepthTextureHeight(1024)
        , _shadowDepthTextureWidth(1024)
        , _shadowDepthTexture(-1)
        , _positionInLightSpaceTexture(-1)
        , _shadowFBO(-1)
        , _firstPassSubroutine(-1)
        , _secondPassSubroutine(1)
        , _defaultFBO(-1)
        , _sunPosition(0.0)
        , _shadowMatrix(1.0)
        , _executeDepthTextureSave(false)
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

        _saveDepthTexture.onChange([&]() {
            _executeDepthTextureSave = true;
        });

        addProperty(_enabled);
        addProperty(_saveDepthTexture);
        addProperty(_distanceFraction);
    }

    void ShadowComponent::initialize()
    {
        using ghoul::filesystem::File;
    }

    bool ShadowComponent::isReady() const {
        return true;
    }

    void ShadowComponent::initializeGL() {
        createDepthTexture();
        createShadowFBO();
    }

    void ShadowComponent::deinitializeGL() {
        glDeleteTextures(1, &_shadowDepthTexture);
        glDeleteTextures(1, &_positionInLightSpaceTexture);
        glDeleteFramebuffers(1, &_shadowFBO);
    }

    void ShadowComponent::begin(const RenderData& data) {

        
        // Builds light's ModelViewProjectionMatrix:
        
        // Camera matrix
        glm::dvec3 diffVector = glm::dvec3(_sunPosition) - data.modelTransform.translation;
        double originalLightDistance = glm::length(diffVector);
        glm::dvec3 lightDirection = glm::normalize(diffVector);
        
        // Percentage of the original light source distance
        double multiplier = originalLightDistance * (static_cast<double>(_distanceFraction)/100000.0);
        
        // New light source position
        glm::dvec3 lightPosition = data.modelTransform.translation + (lightDirection * multiplier);
       
        // Light view matrix
        glm::dmat4 lightViewMatrix = glm::lookAt(
            //lightPosition,
            glm::dvec3(0.0),
            //glm::dvec3(_sunPosition), // position
            glm::dvec3(data.modelTransform.translation), // focus 
            //data.camera.lookUpVectorWorldSpace()  // up
            glm::dvec3(0.0, 1.0, 0.0)
        );

        _cameraPos = data.camera.positionVec3();
        _cameraFocus = data.camera.focusPositionVec3();
        _cameraRotation = data.camera.rotationQuaternion();
        
        Camera *camera = global::renderEngine.camera();
        camera->setPositionVec3(lightPosition);
        //camera->setPositionVec3(glm::dvec3(_sunPosition));
        camera->setFocusPositionVec3(data.modelTransform.translation);
        camera->setRotation(glm::dquat(glm::inverse(lightViewMatrix)));
        
        glm::dmat4 lightProjectionMatrix = glm::dmat4(camera->projectionMatrix());
        //glm::dmat4 lightProjectionMatrix = glm::ortho(-1000.0, 1000.0, -1000.0, 1000.0, 0.0010, 1000.0);

        /*
        //=============== Manually Created Light Matrix ===================
        //=================================================================
        // compute the forward vector from target to eye
        glm::dvec3 forward = lightDirection;
        lightDirection = glm::normalize(lightDirection); // make unit length

        // compute the left vector
        glm::dvec3 left = glm::cross(glm::dvec3(0.0, 1.0, 0.0), forward); // cross product
        left = glm::normalize(left);

        // recompute the orthonormal up vector
        glm::dvec3 up = glm::cross(forward, left);    // cross product

        // init 4x4 matrix
        glm::dmat4 shadowMatrix(1.0);
        
        double* matrix = glm::value_ptr(shadowMatrix);
        // set rotation part, inverse rotation matrix: M^-1 = M^T for Euclidean transform
        matrix[0] = left.x;
        matrix[4] = left.y;
        matrix[8] = left.z;
        matrix[1] = up.x;
        matrix[5] = up.y;
        matrix[9] = up.z;
        matrix[2] = forward.x;
        matrix[6] = forward.y;
        matrix[10] = forward.z;

        // set translation part
        matrix[12] = -left.x * lightPosition.x - left.y * lightPosition.y - left.z * lightPosition.z;
        matrix[13] = -up.x * lightPosition.x - up.y * lightPosition.y - up.z * lightPosition.z;
        matrix[14] = -forward.x * lightPosition.x - forward.y * lightPosition.y - forward.z * lightPosition.z;

        _shadowData.shadowMatrix =
            _toTextureCoordsMatrix *
            lightProjectionMatrix *
            shadowMatrix;
        //=======================================================================
        //=======================================================================
        */

        //============= Light Matrix by Camera Matrices Composition =============
        //=======================================================================
        _shadowData.shadowMatrix = 
            _toTextureCoordsMatrix * 
            lightProjectionMatrix * 
            camera->combinedViewMatrix();
        
        // Saves current state
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);
        glGetIntegerv(GL_VIEWPORT, _mViewport);
        _faceCulling = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &_faceToCull);
        _polygonOffSet = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &_polygonOffSetFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &_polygonOffSetUnits);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, _colorClearValue);
        glGetFloatv(GL_DEPTH_CLEAR_VALUE, &_depthClearValue);

        checkGLError("begin() -- before binding FBO");
        glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
        checkGLError("begin() -- after binding FBO");
        glClearDepth(1.0f);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        checkGLError("begin() -- after cleanning Depth buffer");
        glViewport(0, 0, _shadowDepthTextureWidth, _shadowDepthTextureHeight);
        checkGLError("begin() -- set new viewport");
        //glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &_firstPassSubroutine);
        //checkGLError("begin() -- got firstPassSubroutine");
        glEnable(GL_CULL_FACE);
        checkGLError("begin() -- enabled cull face");
        glCullFace(GL_FRONT);
        checkGLError("begin() -- set cullface to front");
        glEnable(GL_POLYGON_OFFSET_FILL);
        checkGLError("begin() -- enabled polygon offset fill");
        //glPolygonOffset(2.5f, 10.0f);
        checkGLError("begin() -- set values for polygon offset");

        checkGLError("begin() finished");
        
    }

    void ShadowComponent::end(const RenderData& /*data*/) {   
        glFlush();
        if (_executeDepthTextureSave) {
            saveDepthBuffer();
            _executeDepthTextureSave = false;
        }

        Camera *camera = global::renderEngine.camera();
        camera->setPositionVec3(_cameraPos);
        camera->setFocusPositionVec3(_cameraFocus);
        camera->setRotation(_cameraRotation);
        
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

        if (_polygonOffSet) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(_polygonOffSetFactor, _polygonOffSetUnits);
        }
        else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        glClearColor(
            _colorClearValue[0], 
            _colorClearValue[1], 
            _colorClearValue[2], 
            _colorClearValue[3]
        );
        glClearDepth(_depthClearValue);

        checkGLError("end() finished");
    }

    void ShadowComponent::update(const UpdateData& /*data*/) {
        _sunPosition = global::renderEngine.scene()->sceneGraphNode("Sun")->worldPosition();
    }

    void ShadowComponent::createDepthTexture() {
        glGenTextures(1, &_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, _shadowDepthTexture);
        /*glTexStorage2D(
            GL_TEXTURE_2D, 
            1, 
            GL_DEPTH_COMPONENT32,
            _shadowDepthTextureWidth, 
            _shadowDepthTextureHeight
        );
*/
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, shadowBorder);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
        checkGLError("createdDepthTexture");

        glGenTextures(1, &_positionInLightSpaceTexture);
        glBindTexture(GL_TEXTURE_2D, _positionInLightSpaceTexture);        
        //glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(
            GL_TEXTURE_2D, 
            0, 
            GL_RGB32F, 
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight, 
            0, 
            GL_RGB, 
            GL_FLOAT, 
            nullptr
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        checkGLError("createdPositionTexture");

        _shadowData.shadowDepthTexture = _shadowDepthTexture;
        _shadowData.positionInLightSpaceTexture = _positionInLightSpaceTexture;
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
        glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT3,
            _positionInLightSpaceTexture,
            0
        );

        //GLenum drawBuffers[] = { GL_NONE };
        GLenum drawBuffers[] = { GL_NONE, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3 };
        glDrawBuffers(4, drawBuffers);

        // Restores system state
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
        /*glViewport(
            _mViewport[0],
            _mViewport[1],
            _mViewport[2],
            _mViewport[3]
        );*/
        checkGLError("createdShadowFBO");
    }

    void ShadowComponent::saveDepthBuffer() {
        int size = _shadowDepthTextureWidth * _shadowDepthTextureHeight;
        /*GLuint * buffer = new GLuint[size];
        
        glReadPixels(
            0,
            0,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT,
            buffer
        );

        checkGLError("readDepthBuffer To buffer");
        std::fstream ppmFile;

        ppmFile.open("depthBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight << std::endl;
            ppmFile << "255" << std::endl;
            
            std::cout << "\n\nTexture saved to file depthBufferShadowMapping.ppm\n\n";
            int k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++, k++) {
                    unsigned int val = (buffer[k] / std::numeric_limits<GLuint>::max()) * 255;
                    ppmFile << val << " " << val << " " << val << " ";
                }
                ppmFile << std::endl;
            }

            ppmFile.close();
        }        

        delete[] buffer;*/

        GLubyte * buffer = new GLubyte[size];

        glReadPixels(
            0,
            0,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_BYTE,
            buffer
        );

        checkGLError("readDepthBuffer To buffer");
        std::fstream ppmFile;

        ppmFile.open("depthBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight << std::endl;
            ppmFile << "255" << std::endl;

            std::cout << "\n\nTexture saved to file depthBufferShadowMapping.ppm\n\n";
            int k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++, k++) {
                    unsigned int val = static_cast<unsigned int>(buffer[k]);
                    ppmFile << val << " " << val << " " << val << " ";
                }
                ppmFile << std::endl;
            }

            ppmFile.close();
        }

        delete[] buffer;

        GLubyte * bBuffer = new GLubyte[size * 3];

        glReadBuffer(GL_COLOR_ATTACHMENT3);
        glReadPixels(
            0,
            0,
            _shadowDepthTextureWidth,
            _shadowDepthTextureHeight,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            bBuffer
        );

        checkGLError("readPositionBuffer To buffer");
        ppmFile.clear();

        ppmFile.open("positionBufferShadowMapping.ppm", std::fstream::out);
        if (ppmFile.is_open()) {

            ppmFile << "P3" << std::endl;
            ppmFile << _shadowDepthTextureWidth << " " << _shadowDepthTextureHeight << std::endl;
            ppmFile << "255" << std::endl;

            std::cout << "\n\nTexture saved to file positionBufferShadowMapping.ppm\n\n";
            int k = 0;
            for (int i = 0; i < _shadowDepthTextureWidth; i++) {
                for (int j = 0; j < _shadowDepthTextureHeight; j++) {
                    /*ppmFile << (buffer[k] / std::numeric_limits<GLuint>::max()) * 255 << " " 
                        << (buffer[k+1] / std::numeric_limits<GLuint>::max()) * 255 << " " 
                        << (buffer[k+2] / std::numeric_limits<GLuint>::max()) * 255 << " ";*/
                    ppmFile << static_cast<unsigned int>(bBuffer[k]) << " "
                        << static_cast<unsigned int>(bBuffer[k + 1]) << " "
                        << static_cast<unsigned int>(bBuffer[k + 2]) << " ";
                    k += 3;
                }
                ppmFile << std::endl;
            }

            ppmFile.close();
        }

        delete[] bBuffer;
    }

    void ShadowComponent::checkGLError(const std::string & where) const {
        const GLenum error = glGetError();
        switch (error) {
        case GL_NO_ERROR:
            break;
        case GL_INVALID_ENUM:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_INVALID_ENUM", where)
            );
            break;
        case GL_INVALID_VALUE:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_INVALID_VALUE", where)
            );
            break;
        case GL_INVALID_OPERATION:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format(
                    "Function {}: GL_INVALID_OPERATION", where
                ));
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format(
                    "Function {}: GL_INVALID_FRAMEBUFFER_OPERATION",
                    where
                )
            );
            break;
        case GL_OUT_OF_MEMORY:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Function {}: GL_OUT_OF_MEMORY", where)
            );
            break;
        default:
            LERRORC(
                "OpenGL Invalid State",
                fmt::format("Unknown error code: {0:x}", static_cast<int>(error))
            );
        }
    }

    bool ShadowComponent::isEnabled() const {
        return _enabled;
    }

    ShadowComponent::ShadowMapData ShadowComponent::shadowMapData() const {
        return _shadowData;
    }
} // namespace openspace
