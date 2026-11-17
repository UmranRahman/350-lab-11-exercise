#version 120

varying vec2 texCoordsOut;

vec2 cameraPos(vec2 worldPos) {
    // (0,0)..(1280,720) -> (-1,-1)..(1,1)
    return vec2(worldPos.x / 1280.0 * 2.0 - 1.0,
                worldPos.y / 720.0 * 2.0 - 1.0);
}

void main() {
    gl_Position = vec4(cameraPos(gl_Vertex.xy), 0.0, 1.0);
    // getBoundsUV gives OpenGL-convention UVs (origin bottom-left), but SFML stores
    // the image top row first, so flip v to get an upright sprite.
    texCoordsOut = vec2(gl_MultiTexCoord0.x, 1.0 - gl_MultiTexCoord0.y);
}