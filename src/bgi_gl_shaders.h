#pragma once
/**
 * @file bgi_gl_shaders.h
 * @brief Embedded GLSL source strings for the BGI OpenGL rendering pipeline.
 *
 * Three shader programs:
 *  1. Page-texture program  — draws the 2D BGI page buffer as a fullscreen RGBA quad.
 *  2. Flat Phong program    — per-face Phong shading using the `flat` GLSL qualifier.
 *  3. Smooth Phong program  — per-vertex (Gouraud) Phong shading with interpolated normals.
 *  4. Line program          — depth-tested coloured line segments (no lighting).
 *
 * All solid shaders implement a two-light Phong model:
 *   - Primary light: configurable direction, world-space or view-space.
 *   - Fill light:    configurable direction (always world-space), reduced intensity.
 *   - Ambient + diffuse + specular + shininess are uniform scalars.
 */

// =============================================================================
// 1. Page-texture program — fullscreen quad with RGBA texture
// =============================================================================

static const char *kPageTexVertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char *kPageTexFragSrc = R"GLSL(
#version 330 core
in  vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
void main() {
    fragColor = texture(uTex, vUV);
}
)GLSL";

// =============================================================================
// 2. Flat Phong solid program
//    Vertex shader passes colour and face normal flat to fragment shader.
//    Fragment shader applies full Phong lighting once per fragment (constant
//    per face because the normal is flat-interpolated).
// =============================================================================

static const char *kFlatVertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

flat out vec3 vNormal;
flat out vec3 vColor;
     out vec3 vFragPos;

uniform mat4 uMVP;
uniform mat4 uModel;   // model = identity (world coords submitted directly)
uniform mat3 uNormMat; // transpose(inverse(model)) for normal transform

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal  = normalize(uNormMat * aNormal);
    vColor   = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char *kFlatFragSrc = R"GLSL(
#version 330 core
flat in vec3 vNormal;
flat in vec3 vColor;
     in vec3 vFragPos;
out vec4 fragColor;

// Primary light
uniform vec3  uLightDir;       // normalised, in world-space (may be view-rotated outside)
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform float uShininess;
uniform vec3  uCamPos;         // world-space camera position (for specular)

// Fill light
uniform vec3  uFillLightDir;
uniform float uFillStrength;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCamPos - vFragPos);
    vec3 R = reflect(uLightDir, N);

    float diff    = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(R, V), 0.0), uShininess);
    float fillDif = max(dot(N, normalize(-uFillLightDir)), 0.0) * uFillStrength;

    vec3 lit = vColor * (uAmbient + uDiffuse * diff + uFillStrength * fillDif)
             + vec3(1.0) * uSpecular * spec;

    fragColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
)GLSL";

// =============================================================================
// 3. Smooth Phong solid program (Gouraud-style: normals interpolated per vertex)
// =============================================================================

static const char *kSmoothVertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

out vec3 vNormal;
out vec3 vColor;
out vec3 vFragPos;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormMat;

void main() {
    vFragPos    = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = normalize(uNormMat * aNormal);
    vColor      = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char *kSmoothFragSrc = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
in vec3 vFragPos;
out vec4 fragColor;

uniform vec3  uLightDir;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform float uShininess;
uniform vec3  uCamPos;

uniform vec3  uFillLightDir;
uniform float uFillStrength;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCamPos - vFragPos);
    vec3 R = reflect(uLightDir, N);

    float diff    = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(R, V), 0.0), uShininess);
    float fillDif = max(dot(N, normalize(-uFillLightDir)), 0.0) * uFillStrength;

    vec3 lit = vColor * (uAmbient + uDiffuse * diff + uFillStrength * fillDif)
             + vec3(1.0) * uSpecular * spec;

    fragColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
)GLSL";

// =============================================================================
// 4. Depth-tested line program — colour passthrough, no lighting
// =============================================================================

static const char *kLineVertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uMVP;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char *kLineFragSrc = R"GLSL(
#version 330 core
in  vec3 vColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(vColor, 1.0);
}
)GLSL";
