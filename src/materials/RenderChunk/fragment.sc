$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_position

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);

#ifdef TRANSPARENT
  SAMPLER2D_AUTOREG(s_ScreenBuffer);
  SAMPLER2D_AUTOREG(s_SceneDepthBuffer);
  uniform vec4 ViewPositionAndTime;

  #define NL_SSR_STEPS 10
  #define NL_SSR_THICKNESS 1.5
  #define NL_SSR_STRENGTH 0.8

  float nlLinearDepth(float d) {
    float ndc = d*2.0 - 1.0;
    return (2.0*0.05*512.0)/(512.05 - ndc*511.95);
  }

  vec4 nlScreenReflect(vec3 wp, vec3 dir) {
    vec3 p = wp;
    float len = 0.4;
    for (int i = 0; i < NL_SSR_STEPS; i++) {
      p += dir*len;
      len *= 1.3;
      vec4 c = mul(u_viewProj, vec4(p, 1.0));
      if (c.w <= 0.0) { break; }
      vec3 n = c.xyz/c.w;
      vec2 uv = n.xy*0.5 + 0.5;
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { break; }
      float diff = nlLinearDepth(n.z*0.5 + 0.5) - nlLinearDepth(texture2D(s_SceneDepthBuffer, uv).r);
      if (diff > 0.0 && diff < NL_SSR_THICKNESS + len) {
        float edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
        return vec4(texture2D(s_ScreenBuffer, uv).rgb, smoothstep(0.0, 0.12, edge));
      }
    }
    return vec4(0.0, 0.0, 0.0, 0.0);
  }
#endif

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  diffuse.rgb += glow;

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);
      diffuse.rgb *= 1.0 - 0.6*mask;
      diffuse.rgb += v_refl.rgb*mask;
    }
  }

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  diffuse.rgb = colorCorrection(diffuse.rgb);

  #ifdef TRANSPARENT
    if (v_extra.b > 0.9) {
      vec3 fn = normalize(cross(dFdx(v_position), dFdy(v_position)));
      vec3 vdir = normalize(v_position - ViewPositionAndTime.xyz);
      if (abs(fn.y) > 0.9 && vdir.y < 0.0) {
        vec4 ssr = nlScreenReflect(v_position, vec3(vdir.x, -vdir.y, vdir.z));
        float fres = 0.04 + 0.96*pow(1.0 + vdir.y, 5.0);
        float k = ssr.a*(1.0 - v_fog.a)*NL_SSR_STRENGTH*clamp(fres*3.0, 0.0, 1.0);
        diffuse.rgb = mix(diffuse.rgb, ssr.rgb, k);
        diffuse.a = max(diffuse.a, k);
      }
    }
  #endif

  gl_FragColor = diffuse;
}
