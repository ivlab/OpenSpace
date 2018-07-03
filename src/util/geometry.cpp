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

#include <openspace/util/geometry.h>

#include <ghoul/glm.h>
#include <vector>

namespace openspace::geometry {
    
void deleteBuffers(Buffers& buffers) {
    if (buffers.vao != 0) {
        glDeleteVertexArrays(1, &buffers.vao);
        buffers.vao = 0;
    }
    if (buffers.vbo != 0) {
        glDeleteBuffers(1, &buffers.vbo);
        buffers.vbo = 0;
    }
    if (buffers.ibo != 0) {
        glDeleteBuffers(1, &buffers.ibo);
        buffers.ibo = 0;
    }
}

void createSphere(Buffers& buffers, float radius, int nSegments) {
    if (buffers.vao == 0) {
        glGenVertexArrays(1, &buffers.vao);
    }
    if (buffers.vbo == 0) {
        glGenBuffers(1, &buffers.vbo);
    }
    if (buffers.ibo == 0) {
        glGenBuffers(1, &buffers.ibo);
    }

    glBindVertexArray(buffers.vao);

    struct Vertex {
        GLfloat location[4];
        GLfloat tex[2];
        GLfloat normal[3];
    };
    std::vector<Vertex> vertices((nSegments + 1) * (nSegments + 1));

    int index = 0;
    for (int i = 0; i <= nSegments; i++) {
        // define an extra vertex around the y-axis due to texture mapping
        for (int j = 0; j <= nSegments; j++) {
            const float fi = static_cast<float>(i);
            const float fj = static_cast<float>(j);
            // inclination angle (north to south)
            const float theta = fi * glm::pi<float>() / nSegments;  // 0 -> PI

                                                                    // azimuth angle (east to west)
            const float phi = fj * glm::pi<float>() * 2.f / nSegments;  // 0 -> 2*PI

            const float x = radius * sin(phi) * sin(theta);  //
            const float y = radius * cos(theta);             // up
            const float z = radius * cos(phi) * sin(theta);  //

            glm::vec3 normal = glm::vec3(x, y, z);
            if (!(x == 0.f && y == 0.f && z == 0.f)) {
                normal = glm::normalize(normal);
            }

            const float t1 = (fj / nSegments);
            const float t2 = 1.f - (fi / nSegments);

            //double tp = 1.0 / pow(10, static_cast<GLfloat>(radius[1]));

            vertices[index].location[0] = x;
            vertices[index].location[1] = y;
            vertices[index].location[2] = z;
            vertices[index].location[3] = 0.f;
            vertices[index].normal[0] = normal[0];
            vertices[index].normal[1] = normal[1];
            vertices[index].normal[2] = normal[2];

            vertices[index].tex[0] = t1;
            vertices[index].tex[1] = t2;
            ++index;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, tex)) // NOLINT
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, normal)) // NOLINT
    );

    vertices.clear();

    buffers.nVertices = 6 * nSegments * nSegments;
    std::vector<int> indices(buffers.nVertices);
    index = 0;
    // define indices for all triangles
    for (int i = 1; i <= nSegments; ++i) {
        for (int j = 0; j < nSegments; ++j) {
            const int t = nSegments + 1;
            indices[index++] = t * (i - 1) + j + 0; //1
            indices[index++] = t * (i + 0) + j + 0; //2
            indices[index++] = t * (i + 0) + j + 1; //3

            indices[index++] = t * (i - 1) + j + 0; //4
            indices[index++] = t * (i + 0) + j + 1; //5
            indices[index++] = t * (i - 1) + j + 1; //6
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(int),
        indices.data(),
        GL_STATIC_DRAW
    );

    glBindVertexArray(0);
}

void createSphere(Buffers& buffers, const glm::vec3& radii, int nSegments) {
    if (buffers.vao == 0) {
        glGenVertexArrays(1, &buffers.vao);
    }
    if (buffers.vbo == 0) {
        glGenBuffers(1, &buffers.vbo);
    }
    if (buffers.ibo == 0) {
        glGenBuffers(1, &buffers.ibo);
    }

    glBindVertexArray(buffers.vao);

    struct Vertex {
        GLfloat location[4];
        GLfloat tex[2];
        GLfloat normal[3];
    };
    std::vector<Vertex> vertices((nSegments + 1) * (nSegments + 1));

    int index = 0;
    for (int i = 0; i <= nSegments; i++) {
        // define an extra vertex around the y-axis due to texture mapping
        for (int j = 0; j <= nSegments; j++) {
            const float fi = static_cast<float>(i);
            const float fj = static_cast<float>(j);
            // inclination angle (north to south)
            const float theta = fi * glm::pi<float>() / nSegments;  // 0 -> PI

                                                                    // azimuth angle (east to west)
            const float phi = fj * glm::pi<float>() * 2.f / nSegments;  // 0 -> 2*PI

            const float x = radii[0] * sin(phi) * sin(theta);  //
            const float y = radii[1] * cos(theta);             // up
            const float z = radii[2] * cos(phi) * sin(theta);  //

            glm::vec3 normal = glm::vec3(x, y, z);
            if (!(x == 0.f && y == 0.f && z == 0.f)) {
                normal = glm::normalize(normal);
            }

            const float t1 = (fj / nSegments);
            const float t2 = 1.f - (fi / nSegments);

            //double tp = 1.0 / pow(10, static_cast<GLfloat>(radius[1]));

            vertices[index].location[0] = x;
            vertices[index].location[1] = y;
            vertices[index].location[2] = z;
            vertices[index].location[3] = 0.0;

            vertices[index].normal[0] = normal[0];
            vertices[index].normal[1] = normal[1];
            vertices[index].normal[2] = normal[2];

            vertices[index].tex[0] = t1;
            vertices[index].tex[1] = t2;
            ++index;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffers.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, tex)) // NOLINT
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const GLvoid*>(offsetof(Vertex, normal)) // NOLINT
    );

    vertices.clear();

    buffers.nVertices = 6 * nSegments * nSegments;
    std::vector<int> indices(buffers.nVertices);
    index = 0;
    // define indices for all triangles
    for (int i = 1; i <= nSegments; ++i) {
        for (int j = 0; j < nSegments; ++j) {
            const int t = nSegments + 1;
            indices[index++] = t * (i - 1) + j + 0; //1
            indices[index++] = t * (i + 0) + j + 0; //2
            indices[index++] = t * (i + 0) + j + 1; //3

            indices[index++] = t * (i - 1) + j + 0; //4
            indices[index++] = t * (i + 0) + j + 1; //5
            indices[index++] = t * (i - 1) + j + 1; //6
        }
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(int),
        indices.data(),
        GL_STATIC_DRAW
    );

    glBindVertexArray(0);
}


} // namespace openspace::geometry
