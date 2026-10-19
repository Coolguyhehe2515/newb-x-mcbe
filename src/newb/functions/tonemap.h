#ifndef TONEMAP_H
#define TONEMAP_H

#include "utils.h"

#if NL_TONEMAP_TYPE ==                                                         \
    5 // https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
vec3 ACESInput(vec3 c) {
  return vec3(dot(vec3(0.59719, 0.35458, 0.04823), c),
              dot(vec3(0.07600, 0.90834, 0.01566), c),
              dot(vec3(0.02840, 0.13383, 0.83777), c));
}

// ODT_SAT => XYZ => D60_2_D65 => sRGB
vec3 ACESOutput(vec3 c) {
  return vec3(dot(vec3(1.60475, -0.53108, -0.07367), c),
              dot(vec3(-0.10208, 1.10813, -0.00605), c),
              dot(vec3(-0.00327, -0.07276, 1.07602), c));
}

vec3 RRTAndODTFit(vec3 v) {
  vec3 a = v * (v + 0.0245786) - 0.000090537;
  vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return a / b;
}

vec3 acesHill(vec3 color) {
  color = ACESInput(color);
  color = RRTAndODTFit(color);
  color = ACESOutput(color);
  return clamp(color, 0.0, 1.0);
}
#endif
#if NL_TONEMAP_TYPE == 6
// Filmic Tonemapping Operators
// http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 filmic(vec3 x) {
  vec3 X = max(vec3_splat(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3_splat(2.2));
}
#endif
#if NL_TONEMAP_TYPE == 7
vec3 hejl(vec3 hdr, float whitePt) {
  vec4 vh = vec4(hdr, whitePt);  // pack: [r,g,b,w]
  vec4 va = (1.425 * vh) + 0.05; // eval filmic curve
  vec4 vf = ((vh * va + 0.004) / ((vh * (va + 0.55) + 0.0491))) - 0.0821;
  // The curve is negative below ~0.0046 linear, so the documented whitePt > 0.0
  // is understated: at the root vf.w is 0 and the correction blows up, below it
  // the whole result flips sign. max() keeps black at black for the input side.
  return max(vec3_splat(0.0), vf.rgb / vf.www); // white point correction
}
#endif
#if NL_TONEMAP_TYPE == 8
// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
vec3 lottes(vec3 x) {
  vec3 a = vec3_splat(1.6);
  vec3 d = vec3_splat(0.977);
  vec3 hdrMax = vec3_splat(8.0);
  vec3 midIn = vec3_splat(0.18);
  vec3 midOut = vec3_splat(0.267);

  vec3 b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
           ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
  vec3 c = (pow(hdrMax, a * d) * pow(midIn, a) -
            pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
           ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

  return pow(x, a) / (pow(x, a * d) * b + c);
}
#endif
#if NL_TONEMAP_TYPE == 9
// Khronos PBR Neutral Tone Mapper
// https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral
vec3 neutral(vec3 color) {
  const float startCompression = 0.8 - 0.04;
  const float desaturation = 0.15;

  float x = min(color.r, min(color.g, color.b));
  float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
  color -= offset;

  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression)
    return color;

  const float d = 1.0 - startCompression;
  float newPeak = 1.0 - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
  return mix(color, vec3_splat(newPeak), g);
}
#endif
#if NL_TONEMAP_TYPE == 10
// Reinhard driven by luminance, then blended per channel by the per channel
// result, so saturated colors keep their hue instead of skewing towards
// whichever channel compresses first
// https://64.github.io/tonemapping/#reinhard-jodie
vec3 reinhardJodie(vec3 x) {
  const vec3 lw = vec3(0.2126, 0.7152, 0.0722);

  float l = dot(x, lw);
  vec3 tv = x / (1.0 + x);

  return mix(x / (1.0 + l), tv, tv);
}
#endif
#if NL_TONEMAP_TYPE == 11
// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
  vec3 w2 = vec3(step(m + l0, x));
  vec3 w1 = vec3(1.0 - w0 - w2);

  vec3 T = vec3(m * pow(x / m, vec3_splat(c)) + b);
  vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
  vec3 L = vec3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}
#endif
#if NL_TONEMAP_TYPE == 12
// https://github.com/dmnsgn/shaders-tone-map/blob/main/packages/glsl-tone-map/uncharted2.glsl
vec3 uncharted2Tonemap(vec3 x) {
  float A = 0.15;
  float B = 0.50;
  float C = 0.10;
  float D = 0.20;
  float E = 0.02;
  float F = 0.30;
  float W = 11.2;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}
#endif
vec3 colorCorrection(vec3 col) {
#ifdef NL_EXPOSURE
  col *= NL_EXPOSURE;
#endif

// ref - https://64.github.io/tonemapping/
#if NL_TONEMAP_TYPE == 3
  // extended reinhard tonemap
  const float whiteScale = 0.063;
  col = col * (1.0 + col * whiteScale) / (1.0 + col);
#elif NL_TONEMAP_TYPE == 4
  // aces tonemap
  const float a = 1.04;
  const float b = 0.03;
  const float c = 0.93;
  const float d = 0.56;
  const float e = 0.14;
  col *= 0.85;
  col = clamp((col * (a * col + b)) / (col * (c * col + d) + e), 0.0, 1.0);
#elif NL_TONEMAP_TYPE == 2
  // simple reinhard tonemap
  col = col / (1.0 + col);
#elif NL_TONEMAP_TYPE == 1
  // exponential tonemap
  col = 1.0 - exp(-col * 0.8);
#elif NL_TONEMAP_TYPE == 5  // aces Hill
  col = acesHill(col);
#elif NL_TONEMAP_TYPE == 6  // flimic
  col = filmic(col);
#elif NL_TONEMAP_TYPE == 7  // hejl
  const float W = 11.2; // scene white point, same as uncharted2
  col = hejl(col, W);
#elif NL_TONEMAP_TYPE == 8  // lottes
  col = lottes(col);
#elif NL_TONEMAP_TYPE == 9  // neutral
  col = neutral(col);
#elif NL_TONEMAP_TYPE == 10 // reinhard Jodie
  col = reinhardJodie(col);
#elif NL_TONEMAP_TYPE == 11 // uchimura
  const float P = 1.0;  // max display brightness
  const float a = 1.0;  // contrast
  const float m = 0.22; // linear section start
  const float l = 0.4;  // linear section length
  const float c = 1.33; // black
  const float b = 0.0;  // pedestal
  col = uchimura(col, P, a, m, l, c, b);
#elif NL_TONEMAP_TYPE == 12 // uncharted2
  const float W = 11.2;
  float exposureBias = 2.0;
  vec3 curr = uncharted2Tonemap(exposureBias * col);
  vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3_splat(W));
  col = curr * whiteScale;
#elif NL_TONEMAP_TYPE == 13 // unreal
  // Unreal 3, Documentation: "Color Grading"
  col = col / (col + 0.155) * 1.019;
#endif

  // gamma correction
  col = pow(col, vec3_splat(1.0 / NL_GAMMA));

#ifdef NL_SATURATION
  col = mix(vec3_splat(luminance(col)), col, NL_SATURATION);
#endif

#ifdef NL_TINT
  col *= mix(NL_TINT_LOW, NL_TINT_HIGH, col);
#endif

  return col;
}

// inv used in fogcolor for nether
vec3 colorCorrectionInv(vec3 col) {
#ifdef NL_TINT
  col /= mix(NL_TINT_LOW, NL_TINT_HIGH, col); // not accurate inverse
#endif

#ifdef NL_SATURATION
  col = mix(vec3_splat(dot(col, vec3(0.21, 0.71, 0.08))), col,
            1.0 / NL_SATURATION);
#endif

  // incomplete
  // extended reinhard only
  float ws = 0.7966;
  col = pow(col, vec3_splat(NL_GAMMA));
  col = col * (ws + col) / (ws + col * (1.0 - ws));

#ifdef NL_EXPOSURE
  col /= NL_EXPOSURE;
#endif

  return col;
}

#endif
