#version 440
// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate burn-in accumulation pass.
// Ported from cool-retro-term's burn_in.frag (GPL-3.0) without modification to
// the math; only the CRT-Kate file header and the absence of the vertex stage
// (Qt's default vertex shader supplies qt_TexCoord0) differ.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ubuf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float burnInLastUpdate;
    float burnInTime;
    float prevLastUpdate;
};

layout(binding = 0) uniform sampler2D txt_source;
layout(binding = 1) uniform sampler2D burnInSource;

float rgb2grey(vec3 v) {
    return dot(v, vec3(0.21, 0.72, 0.04));
}

void main() {
    vec2 coords = qt_TexCoord0;

    vec3 txtColor = texture(txt_source, coords).rgb;
    vec4 accColor = texture(burnInSource, coords);

    float prevMask = accColor.a;

    float blurDecay = clamp((burnInLastUpdate - prevLastUpdate) * burnInTime, 0.0, 1.0);
    blurDecay = max(0.0, blurDecay - prevMask);
    vec3 color = max(accColor.rgb - vec3(blurDecay), txtColor);

    float currMask = step(rgb2grey(color), rgb2grey(txtColor));

    fragColor = vec4(color, currMask) * qt_Opacity;
}
