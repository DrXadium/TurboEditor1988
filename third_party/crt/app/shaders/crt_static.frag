#version 440
// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate static CRT pass.
// Ported from cool-retro-term's terminal_static.frag (GPL-3.0), adapted for an
// edge-to-edge GUI: the bezel illumination becomes a screen-edge glow, frame
// shininess becomes a glass sheen, and the frame size knob becomes screen inset.
// Curvature, RGB shift, bloom and the film grain keep CRT's math.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ubuf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 resolution;
    float screenCurvature;
    float rgbShift;
    float frameShininess;
    float frameSize;
    float brightness;
    float bloom;
    float staticNoise;
    float screenRadius;
    float ambientLight;
    float glassSheen;
};

layout(binding = 0) uniform sampler2D source;
layout(binding = 1) uniform sampler2D bloomSource;

float min2(vec2 v) { return min(v.x, v.y); }
float max2(vec2 v) { return max(v.x, v.y); }
float rand2(vec2 v) { return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453); }

vec2 distortCoordinates(vec2 coords) {
    vec2 paddedCoords = coords * (vec2(1.0) + frameSize * 2.0) - frameSize;
    vec2 cc = (paddedCoords - vec2(0.5));
    float dist = dot(cc, cc) * screenCurvature;
    return (paddedCoords + cc * (1.0 + dist) * dist);
}

void main() {
    vec2 uv = qt_TexCoord0;

    vec2 curvatureCoords = distortCoordinates(uv);
    // Both axes must be inside the (curved, inset) screen rect. Upstream's
    // max2 exists because its frame/bezel reflection fills the outside; this
    // app has no bezel, so anything outside the screen is black.
    float shownDraw = min2(step(vec2(0.0), curvatureCoords) - step(vec2(1.0), curvatureCoords));
    float isScreen = shownDraw;
    vec2 txt_coords = curvatureCoords
        * (-1.0 + 2.0 * step(vec2(0.0), curvatureCoords) - 2.0 * step(vec2(1.0), curvatureCoords));

    vec3 txt_color = texture(source, txt_coords).rgb;

    // True RGB shift: the three channels sample at different offsets. Weights
    // are CRT's exactly (red 0.10/0.30/0.60, green 0.20/0.20/0.60,
    // blue 0.30/0.10/0.60).
    if (rgbShift != 0.0) {
        vec2 displacement = vec2(rgbShift * 4.0 / max(resolution.x, 1.0), 0.0);
        vec3 rightColor = texture(source, txt_coords + displacement).rgb;
        vec3 leftColor = texture(source, txt_coords - displacement).rgb;
        txt_color.r = leftColor.r * 0.10 + rightColor.r * 0.30 + txt_color.r * 0.60;
        txt_color.g = leftColor.g * 0.20 + rightColor.g * 0.20 + txt_color.g * 0.60;
        txt_color.b = leftColor.b * 0.30 + rightColor.b * 0.10 + txt_color.b * 0.60;
    }

    vec3 finalColor = txt_color * shownDraw;

    if (bloom > 0.0) {
        vec4 bloomFullColor = texture(bloomSource, txt_coords);
        vec3 bloomColor = bloomFullColor.rgb;
        float bloomAlpha = bloomFullColor.a;
        vec3 bloomOnScreen = bloomColor * isScreen;
        finalColor += clamp(bloomOnScreen * bloom * bloomAlpha, 0.0, 0.5);
        finalColor /= 1.0 + max(bloom, 0.0);
    }

    finalColor *= mix(0.5, 1.5, clamp(brightness, 0.0, 1.0));

    // Film grain. Gated so an all-effects-off chain stays identity (CRT's
    // grain is unconditional).
    if (staticNoise > 0.0) {
        float noise = rand2(uv) - 0.5;
        finalColor = clamp(finalColor + vec3(noise * 0.025), 0.0, 1.0);
    }

    // Ambient light: the screen's own light spills at the glass edge. Without a
    // casing this is an inward edge glow tinted by the local screen colour, the
    // truthful adaptation of CRT's bezel illumination.
    float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float edgeGlow = ambientLight * (1.0 - smoothstep(0.0, 0.10, edge));
    finalColor += finalColor * edgeGlow * 0.6;

    // Glass sheen: a subtle diagonal specular gradient, gated at 0 so it cannot
    // wash out text unless asked for.
    float sheen = clamp((uv.x + uv.y) * 0.5, 0.0, 1.0);
    finalColor += vec3(sheen * glassSheen * 0.12);

    // Rounded glass corners. 40 px at screenRadius = 1, expressed in UV so the
    // mask is resolution independent.
    if (screenRadius > 0.0) {
        float radiusUv = screenRadius * 40.0 / max(min(resolution.x, resolution.y), 1.0);
        vec2 q = abs(uv - vec2(0.5)) - (vec2(0.5) - radiusUv);
        float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radiusUv;
        float cornerMask = 1.0 - smoothstep(0.0, 0.0015, sd);
        finalColor *= cornerMask;
    }

    finalColor *= shownDraw;
    fragColor = vec4(clamp(finalColor, 0.0, 1.0), qt_Opacity);
}
