#version 440
// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate generalized CRT fragment shader.
// Derived from cool-retro-term's terminal_static.frag / terminal_dynamic.frag
// (GPL-3.0), generalized to a single `source` sampler with NO bezel/frame/
// burn-in (user: no bezel; source is a plain UI texture, not a terminal).
//
// All effects are gated by uniforms so that disabling them yields identity:
//   curvature (screenCurvature), scanlines (rasterizationIntensity), glow/bloom
//   (bloom), chromatic aberration (rgbShift), jitter/motion (jitter, horizontalSync),
//   flicker (flickering), brightness/contrast (brightness, contrast).

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ubuf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2  resolution;
    float screenCurvature;
    float rasterizationIntensity;
    float glow;            // bloom/glow
    float rgbShift;        // chromatic aberration
    float jitter;          // horizontal jitter amount
    float horizontalSync;  // vertical roll amount
    float flickering;      // brightness flicker amount
    float brightness;
    float contrast;
    float staticNoise;
    float scanlineDensity; // scanlines per pixel row scale
};

layout(binding = 0) uniform sampler2D source;

float rand2(vec2 v) { return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453); }
float min2(vec2 v) { return min(v.x, v.y); }
float max2(vec2 v) { return max(v.x, v.y); }

// Barrel distortion — from cool-retro-term distortCoordinates (frameSize=0: no bezel).
vec2 distortCoordinates(vec2 coords) {
    vec2 cc = coords - vec2(0.5);
    float dist = dot(cc, cc) * screenCurvature;
    return coords + cc * (1.0 + dist) * dist;
}

void main() {
    vec2 uv = qt_TexCoord0;
    float shownDraw = 1.0;

    // --- Curvature ---
    if (screenCurvature != 0.0) {
        vec2 c = distortCoordinates(uv);
        shownDraw = max2(step(vec2(0.0), c) - step(vec2(1.0), c));
        uv = clamp(c, 0.0, 1.0);
    }

    // --- Motion: horizontal jitter + vertical sync roll ---
    if (jitter != 0.0) {
        float n = rand2(vec2(floor(uv.y * resolution.y), floor(time * 12.0)));
        uv.x += (n - 0.5) * jitter * 0.01;
    }
    if (horizontalSync != 0.0) {
        uv.y += sin(time * 2.0 + uv.x * 6.2831) * horizontalSync * 0.02;
    }

    // --- Chromatic aberration (RGB shift) ---
    vec3 col;
    if (rgbShift != 0.0) {
        vec2 d = vec2(rgbShift * 0.01, 0.0);
        col.r = texture(source, uv - d).r;
        col.g = texture(source, uv).g;
        col.b = texture(source, uv + d).b;
    } else {
        col = texture(source, uv).rgb;
    }

    // --- Scanlines (rasterization) ---
    if (rasterizationIntensity != 0.0) {
        float line = sin(uv.y * resolution.y * scanlineDensity * 3.14159);
        float scan = 1.0 - rasterizationIntensity * 0.5 * (1.0 - line);
        col *= scan;
    }

    // --- Glow / bloom (cheap: lift bright areas) ---
    if (glow != 0.0) {
        float lum = dot(col, vec3(0.299, 0.587, 0.114));
        col += col * glow * smoothstep(0.5, 1.0, lum);
    }

    // --- Contrast + brightness ---
    col = (col - 0.5) * (1.0 + contrast) + 0.5;
    col *= (0.5 + brightness);

    // --- Static noise ---
    if (staticNoise != 0.0) {
        float n = rand2(uv * resolution + vec2(fract(time))) - 0.5;
        col += n * staticNoise * 0.15;
    }

    // --- Flicker (time-varying brightness) ---
    if (flickering != 0.0) {
        float f = 1.0 + (rand2(vec2(floor(time * 24.0), 1.0)) - 0.5) * flickering;
        col *= f;
    }

    col = clamp(col, 0.0, 1.0) * shownDraw;
    fragColor = vec4(col, qt_Opacity);
}
