#version 120

uniform sampler1D transforms;
uniform int numTransforms;

attribute vec2 position;
attribute vec2 texCoords;
attribute float spriteNumber;

varying vec2 texCoordsOut;

vec2 cameraPos(vec2 worldPos) {
    return vec2(worldPos.x / 1280.0 * 2.0 - 1.0,
                worldPos.y / 720.0 * 2.0 - 1.0);
}

void main() {
    // one texel per transform; +0.5 samples the texel centre
    vec4 t = texture1D(transforms, (spriteNumber + 0.5) / float(numTransforms));
    vec2 scale = t.xy;        // BasicTransform memory layout: scale, then translation
    vec2 translation = t.zw;

    vec2 world = position * scale + translation;  // scale first, then translate
    gl_Position = vec4(cameraPos(world), 0.0, 1.0);
    texCoordsOut = vec2(texCoords.x, 1.0 - texCoords.y);
}