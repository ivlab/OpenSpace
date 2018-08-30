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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___SHADOWCOMPONENT___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___SHADOWCOMPONENT___H__

#include <openspace/properties/propertyowner.h>

#include <openspace/properties/scalar/boolproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <openspace/properties/scalar/intproperty.h>
#include <openspace/properties/vector/vec2property.h>
#include <openspace/properties/vector/vec4property.h>
#include <openspace/properties/stringproperty.h>

#include <ghoul/glm.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/uniformcache.h>

namespace ghoul {
    class Dictionary;
}
   
namespace ghoul::filesystem { class File; }

namespace ghoul::opengl {
    class ProgramObject;
} // namespace ghoul::opengl

namespace openspace {
    struct RenderData;
    struct UpdateData;

    namespace documentation { struct Documentation; }

    static const GLfloat shadowBorder[] = { 1.f, 0.f, 0.f, 0.f };

    class ShadowComponent : public properties::PropertyOwner {
    public:
        ShadowComponent(const ghoul::Dictionary& dictionary);

        void initialize();
        void initializeGL();
        void deinitializeGL();
        //bool deinitialize();

        bool isReady() const;

        void begin(const RenderData& data);
        void end(const RenderData& data);
        void update(const UpdateData& data);

        static documentation::Documentation Documentation();

    private:
        void createDepthTexture();
        void createShadowFBO();

        // Debug
        void saveDepthBuffer();

    private:
        std::unique_ptr<ghoul::opengl::ProgramObject> _shaderProgram;
        
        ghoul::Dictionary _shadowMapDictionary;
        
        UniformCache(Color, MVP, sunPosition, sunIntensity,
            shadowMap, shadowMatrix) _uniformCache;

        int _shadowDepthTextureHeight;
        int _shadowDepthTextureWidth;
        
        GLuint _shadowDepthTexture;
        GLuint _shadowFBO;
        GLuint _firstPassSubroutine;
        GLuint _secondPassSubroutine;
        GLint _defaultFBO;
        GLint _mViewport[4];

        GLboolean _faceCulling;
        
        GLenum _faceToCull;

        glm::vec3 _sunPosition;

        glm::dmat4 _shadowMatrix;

        glm::dvec3 _cameraPos;
        glm::dvec3 _cameraFocus;
        
    };

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___SHADOWCOMPONENT___H__
