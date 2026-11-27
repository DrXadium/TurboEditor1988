#version 440
// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate dynamic CRT pass.
// Ported from cool-retro-term's terminal_dynamic.frag (GPL-3.0). The terminal
// frame sampler and the custom vertex shader are removed: this shader feeds a
// general UI texture, so the per-frame vertex values (brightness, distortion
// scale/frequency) are computed here from the same noise field. All compile-time
// CRT variants are replaced by uniform branches, keeping one shader program.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform ubuf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec4 fontColor;
    vec4 backgroundColor;
    vec2 virtualResolution;
    float rasterizationIntensity;
    int rasterizationMode;
    float burnInLastUpdate;
    float burnInTime;
    float burnIn;
    float staticNoise;
    float screenCurvature;
    float glowingLine;
    float chromaColor;
    vec2 jitterDisplacement;
    float jitter;
    float horizontalSync;
    float horizontalSyncStrength;
    float flickering;
    float frameSize;
    float bloom;
    float phosphorEnabled;
    float contrast;
    float sweep;
    float sweepPhase;
    // Physical pixels per raster cell. CRT's grid is a character cell; a GUI
    // source needs an explicit pitch, so the mask is quantised to physical
    // pixels before CRT's math runs.
    float scanlinePitch;
};

layout(binding = 0) uniform sampler2D screenBuffer;
layout(binding = 1) uniform sampler2D burnInSource;

float min2(vec2 v) { return min(v.x, v.y); }
float rgb2grey(vec3 v) { return dot(v, vec3(0.21, 0.72, 0.04)); }

// A procedural replacement for cool-retro-term's allNoise512.png. The noise
// bitmap is a random field; these hashes give the same four independent
// channels the shader reads (r = horizontal-sync gate, g = brightness /
// frequency, b/a = jitter), without shipping a texture that the test harness
// and the installed app would both have to locate.
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec4 noiseAt(vec2 p) {
    return vec4(hash21(p),
                hash21(p + vec2(19.19, 7.77)),
                hash21(p + vec2(37.37, 11.11)),
                hash21(p + vec2(71.71, 3.33)));
}

vec2 distortCoordinates(vec2 coords) {
    vec2 paddedCoords = coords * (vec2(1.0) + frameSize * 2.0) - frameSize;
    vec2 cc = (paddedCoords - vec2(0.5));
    float dist = dot(cc, cc) * screenCurvature;
    return (paddedCoords + cc * (1.0 + dist) * dist);
}

vec3 applyRasterization(vec2 screenCoords, vec3 texel, vec2 virtualRes,
                        float intensity, int mode) {
    if (mode == 0 || mode == 4 || intensity <= 0.0)
        return texel;

    const float INTENSITY = 0.30;
    const float BRIGHTBOOST = 0.30;

    vec3 pixelHigh = ((1.0 + BRIGHTBOOST) - (0.2 * texel)) * texel;
    vec3 pixelLow  = ((1.0 - INTENSITY) + (0.1 * texel)) * texel;

    // Sample the cell at its physical pixel, not at a fractional position:
    // with pitch 2 the pixel centres sit at 0.25/0.75 of a cell, where CRT's
    // symmetric mask returns the same value on both rows.
    vec2 cell = floor(screenCoords * virtualRes * scanlinePitch)
                / max(scanlinePitch, 1.0);

    if (mode == 1) {
        vec2 coords = fract(cell) * 2.0 - vec2(1.0);
        float mask = 1.0 - abs(coords.y);
        vec3 rasterizationColor = mix(pixelLow, pixelHigh, mask);
        return mix(texel, rasterizationColor, intensity);
    }

    if (mode == 2) {
        vec2 coords = fract(cell) * 2.0 - vec2(1.0);
        coords = coords * coords;
        float mask = 1.0 - coords.x - coords.y;
        vec3 rasterizationColor = mix(pixelLow, pixelHigh, mask);
        return mix(texel, rasterizationColor, intensity);
    }

    if (mode == 3) {
        const float SUBPIXELS = 3.0;
        vec3 offsets = vec3(3.141592654) * vec3(0.5, 0.5 - 2.0 / 3.0, 0.5 - 4.0 / 3.0);

        vec2 omega = vec2(3.141592654) * vec2(2.0) * virtualRes;
        vec2 angle = screenCoords * omega;
        vec3 xfactors = (SUBPIXELS + sin(angle.x + offsets)) / (SUBPIXELS + 1.0);

        vec3 result = texel * xfactors;
        vec3 subHigh = ((1.0 + BRIGHTBOOST) - (0.2 * result)) * result;
        vec3 subLow  = ((1.0 - INTENSITY) + (0.1 * result)) * result;

        vec2 coords = fract(cell) * 2.0 - vec2(1.0);
        float mask = 1.0 - abs(coords.y);
        vec3 rasterizationColor = mix(subLow, subHigh, mask);
        return mix(texel, rasterizationColor, intensity);
    }

    return texel;
}

float randomPass(vec2 coords) {
    return fract(smoothstep(-120.0, 0.0,
                            coords.y - (virtualResolution.y + 120.0) * fract(time * 0.15)));
}

vec3 convertWithChroma(vec3 inColor) {
    float grey = rgb2grey(inColor);
    float denom = max(grey, 0.0001);
    vec3 foregroundColor = mix(fontColor.rgb, inColor * fontColor.rgb / denom, chromaColor);
    return mix(backgroundColor.rgb, foregroundColor, grey);
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec2 cc = vec2(0.5) - uv;
    float dist = length(cc);

    vec2 staticCoords = distortCoordinates(uv);
    vec2 coords = uv;

    // Per-frame vertex values from terminal_dynamic.vert, now in the fragment
    // stage. The frame noise is sampled at the same two scroll rates.
    vec4 initialNoise = noiseAt(vec2(fract(time / 2.048), fract(time / 1048.576)));
    float vBrightness = 1.0 + (initialNoise.g - 0.5) * flickering;

    float randval = horizontalSyncStrength - initialNoise.r;
    float vDistortionScale = step(0.0, randval) * randval * horizontalSyncStrength * horizontalSync;
    float vDistortionFreq = mix(4.0, 40.0, initialNoise.g) * step(0.0, horizontalSync);

    float dst = sin((coords.y + time) * vDistortionFreq);
    coords.x += dst * vDistortionScale;

    // 512 is allNoise512.png's edge; 0.75 reproduces scaleNoiseSize.
    vec2 noiseScale = vec2(0.75) * virtualResolution / 512.0;
    vec4 noiseTexel = noiseAt(noiseScale * coords
                              + vec2(fract(time / 0.051), fract(time / 0.237)));

    vec2 txt_coords = coords + (noiseTexel.ba - vec2(0.5)) * jitterDisplacement * jitter;

    float color = 0.0;
    color += noiseTexel.a * staticNoise * (1.0 - dist * 1.3);
    color += randomPass(coords * virtualResolution) * glowingLine * 0.2;

    vec3 txt_color = texture(screenBuffer, txt_coords).rgb;
    txt_color *= 1.0 + max(bloom, 0.0);

    if (burnIn > 0.0) {
        vec4 txt_blur = texture(burnInSource, staticCoords);
        float blurDecay = clamp((time - burnInLastUpdate) * burnInTime, 0.0, 1.0);
        vec3 burnInColor = 0.65 * (txt_blur.rgb - vec3(blurDecay)) * (1.0 - txt_blur.a);
        txt_color = max(txt_color, burnInColor);
    }

    txt_color += vec3(color);
    txt_color = applyRasterization(staticCoords, txt_color, virtualResolution,
                                   rasterizationIntensity, rasterizationMode);

    // Display contrast, neutral at 0.8 (CRT folds contrast into derived
    // terminal colours; a GUI source needs a display contrast instead).
    float contrastFactor = clamp(1.0 + (contrast - 0.8), 0.2, 2.2);
    txt_color = (txt_color - 0.5) * contrastFactor + 0.5;

    if (phosphorEnabled > 0.5)
        txt_color = convertWithChroma(txt_color);

    float brightness = mix(1.0, vBrightness, step(0.0, flickering));
    txt_color *= brightness;

    // TV-revision2 sweep: a bright band travelling top to bottom.
    if (sweep != 0.0) {
        float phase = fract(sweepPhase);
        float coord = (uv.y - phase) * 6.28318530718;
        float band = pow(max(sin(coord), 0.0), 4.0);
        txt_color += txt_color * band * 1.4;
        txt_color += vec3(band) * 0.30;
    }

    fragColor = vec4(clamp(txt_color, 0.0, 1.0), qt_Opacity);
}
