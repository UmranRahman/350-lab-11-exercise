#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include "glad/glad.h" // must come before any other GL header

#include <cstddef>  // offsetof

#include "basic_transform.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

class SpriteManager {
public:
        SpriteManager(const sf::Vector2u& atlasSize, uint padding)
        : mVertices(sf::PrimitiveType::Triangles),
          mNumSprites{0},
          mAtlas(atlasSize, padding) {
        // PART 1: "main_sfml.vert".  PART 2: change this to "main.vert".
        if (!mShader.loadFromFile(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"})) {
            throw std::runtime_error("Failed to load shaders");
        }
        mShader.setUniform("texture", sf::Shader::CurrentTexture);  // used by part 1 only

        // Part 2: look up locations (they are -1 when using main_sfml.vert, which is harmless)
        const GLuint prog = mShader.getNativeHandle();
        mPositionAttribLocation = glGetAttribLocation(prog, "position");
        mTexCoordsAttribLocation = glGetAttribLocation(prog, "texCoords");
        mSpriteNumberAttribLocation = glGetAttribLocation(prog, "spriteNumber");
        mTextureUniformLocation = glGetUniformLocation(prog, "texture");
        mTransformsUniformLocation = glGetUniformLocation(prog, "transforms");
        mNumTransformsUniformLocation = glGetUniformLocation(prog, "numTransforms");

        glGenBuffers(1, &mVerticesVBO);
        glGenBuffers(1, &mSpriteNumberVBO);
        glGenTextures(1, &mTransformsTexture);
        
    }

    ~SpriteManager() {
        glDeleteBuffers(1, &mVerticesVBO);
        glDeleteBuffers(1, &mSpriteNumberVBO);
        glDeleteTextures(1, &mTransformsTexture);
    }

    // We own GL handles, so forbid copying.
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;

    // TODO
    TextureID registerTexture(const std::string& filepath) {
        return mAtlas.registerTexture(filepath);
    };

    // TODO
    SpriteID createSprite(TextureID textureID) {
        const SpriteID id = mNumSprites++;
        mTransforms.emplace_back();                          // default BasicTransform
        mVertices.resize(mVertices.getVertexCount() + 6);    // 6 new vertices
        for (int i = 0; i < 6; ++i) {
            mSpriteNumbers.push_back(static_cast<float>(id));  // part 2 attribute
        }
        writeQuad(id, textureID);
        return id;
    };

    // TODO
    void setTexture(SpriteID sprite, TextureID newTexture) {
        writeQuad(sprite, newTexture); 
    };

    // TODO
    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        sf::VertexArray tmp(sf::PrimitiveType::Triangles, mVertices.getVertexCount());
        for (size_t s = 0; s < mNumSprites; ++s) {
            for (size_t k = 0; k < 6; ++k) {
                const size_t i = 6 * s + k;
                tmp[i] = mVertices[i];
                tmp[i].position = mTransforms[s].applyPoint(mVertices[i].position);
            }
        }
        states.texture = &mAtlas.getTexture();
        states.shader = &mShader;
        target.draw(tmp, states);
    };

    // TODO
    void drawGL() {
        if (mNumSprites == 0) return;
        glUseProgram(mShader.getNativeHandle());

        // vertex positions + uvs (interleaved in sf::Vertex: 20 bytes, pos @0, uv @12)
        glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO);
        glBufferData(GL_ARRAY_BUFFER, mVertices.getVertexCount() * sizeof(sf::Vertex), &mVertices[0], GL_STREAM_DRAW);
        glVertexAttribPointer(mPositionAttribLocation, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (const void*)offsetof(sf::Vertex, position));
        glEnableVertexAttribArray(mPositionAttribLocation);
        glVertexAttribPointer(mTexCoordsAttribLocation, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (const void*)offsetof(sf::Vertex, texCoords));
        glEnableVertexAttribArray(mTexCoordsAttribLocation);

        // sprite numbers (separate buffer, tightly packed floats)
        glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO);
        glBufferData(GL_ARRAY_BUFFER, mSpriteNumbers.size() * sizeof(float), mSpriteNumbers.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(mSpriteNumberAttribLocation, 1, GL_FLOAT, GL_FALSE, sizeof(float), (const void*)0);
        glEnableVertexAttribArray(mSpriteNumberAttribLocation);

        // texture unit 0: atlas (2D)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle());

        // texture unit 1: transforms (1D, RGBA32F; one texel per sprite)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_1D, mTransformsTexture);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // no mipmaps!
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB, static_cast<GLsizei>(mTransforms.size()), 0, GL_RGBA, GL_FLOAT, mTransforms.data());

        // uniforms
        glUniform1i(mTextureUniformLocation, 0);
        glUniform1i(mTransformsUniformLocation, 1);
        glUniform1i(mNumTransformsUniformLocation, static_cast<GLint>(mTransforms.size()));

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mVertices.getVertexCount()));

        // tidy up so we don't leak state
        glDisableVertexAttribArray(mPositionAttribLocation);
        glDisableVertexAttribArray(mTexCoordsAttribLocation);
        glDisableVertexAttribArray(mSpriteNumberAttribLocation);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0);
    };

    // TODO
    BasicTransform& getTransform(SpriteID id) {
        return mTransforms[id];
    }

    // TODO
    const TextureAtlas& getAtlas() const {
        return mAtlas;
    };

private:
    // Writes the 6 vertices (two triangles) of a sprite: local position and UVs.
    // Bottom-left of the quad is (0,0), top-right is (texture width, texture height).
    void writeQuad(SpriteID sprite, TextureID tex) {
        const sf::Vector2f size(mAtlas.getSizePixels(tex));
        const sf::FloatRect uv = mAtlas.getBoundsUV(tex);  // position = bottom-left in GL UV
        const float u0 = uv.position.x, v0 = uv.position.y;
        const float u1 = u0 + uv.size.x, v1 = v0 + uv.size.y;
        const sf::Color c = sf::Color::White;

        const sf::Vertex bl{{0.f, 0.f}, c, {u0, v0}};
        const sf::Vertex br{{size.x, 0.f}, c, {u1, v0}};
        const sf::Vertex tr{{size.x, size.y}, c, {u1, v1}};
        const sf::Vertex tl{{0.f, size.y}, c, {u0, v1}};

        const size_t b = 6 * sprite;
        mVertices[b + 0] = bl;  // triangle 1
        mVertices[b + 1] = br;
        mVertices[b + 2] = tr;
        mVertices[b + 3] = bl;  // triangle 2
        mVertices[b + 4] = tr;
        mVertices[b + 5] = tl;
    }


    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;

    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation = -1;
    GLint mTransformsUniformLocation = -1;
    GLint mNumTransformsUniformLocation = -1;

    GLint mPositionAttribLocation = -1;
    GLint mTexCoordsAttribLocation = -1;
    GLint mSpriteNumberAttribLocation = -1;

    GLuint mVerticesVBO = 0;
    GLuint mSpriteNumberVBO = 0;
    GLuint mTransformsTexture = 0;
};

#endif  // SPRITE_MANAGER_H
