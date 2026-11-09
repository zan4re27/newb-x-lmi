$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_wPos, v_isTree, v_time

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);
SAMPLER2D_AUTOREG(s_ScreenBuffer);
SAMPLER2D_AUTOREG(s_SceneDepthBuffer);

uniform vec4 TimeOfDay;
uniform vec4 CameraPosition;

#define FOAM_NEAR  0.05
#define FOAM_FAR   512.0
#define FOAM_WIDTH 0.6
#define FOAM_COLOR vec3(0.55, 0.75, 0.75)

float sceneDepthLinear(float d) {
    float ndc = d * 2.0 - 1.0;
    return (2.0 * FOAM_NEAR * FOAM_FAR) /
           (FOAM_FAR + FOAM_NEAR - ndc * (FOAM_FAR - FOAM_NEAR));
}

vec3 ssr(vec3 relWorldPos, vec3 viewDir, vec3 normal, float NdotV, vec3 skyFallback) {
    const float REFLECT_DISTANCE = 64.0;
    const float MIN_REFLECT_Y    = 0.02;
    const float SSR_REGION_END   = 0.05;
    const float FADE_SOFTNESS    = 0.05;
    const float SSR_STRENGTH     = 1.0;
    
    vec3 waterBase = skyFallback;
    
    vec3 n    = normalize(vec3(normal.x, 1.0, normal.z));
    vec3 refl = reflect(viewDir, n);
    refl.y    = max(refl.y, MIN_REFLECT_Y);
    
    vec4 clip = mul(u_viewProj, vec4(relWorldPos + refl * REFLECT_DISTANCE, 1.0));
    if (clip.w <= 0.0) return waterBase;
    
    vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
    
    vec2 edgeDist = min(uv, 1.0 - uv);
    float minEdge = min(edgeDist.x, edgeDist.y);
    float ssrBlend = smoothstep(SSR_REGION_END - FADE_SOFTNESS, SSR_REGION_END, minEdge);
    
    vec2 clampedUV = clamp(uv, 0.001, 0.999);
    float sampleDepth = texture2D(s_SceneDepthBuffer, clampedUV).r;
    float isSky       = step(0.9999, sampleDepth);
    
    vec3 sceneColor = texture2DLod(s_ScreenBuffer, clampedUV, 0.0).rgb;
    vec3 reflectedColor = mix(sceneColor, skyFallback, isSky);
    
    vec3 ssrResult = mix(waterBase, reflectedColor, SSR_STRENGTH);
    
    return mix(waterBase, ssrResult, ssrBlend);
}

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    return;
  #endif

  vec4 diffuse = texture2D(s_MatTexture, v_texcoord0);
  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
    
    if (v_isTree > 0.5 && gl_FrontFacing) discard;
  #endif

  vec2 offset = 1.0 / vec2(textureSize(s_MatTexture, 0));
  vec2 sampleUV = v_texcoord0 + offset * vec2(-0.1, -0.1);
  sampleUV = clamp(sampleUV, vec2_splat(0.001), vec2_splat(0.999));
  vec4 neighborTex = texture2D(s_MatTexture, sampleUV);
  if (diffuse.a > 0.6 && neighborTex.a > 0.6) {
    vec3 neighbor = neighborTex.rgb;
    vec3 contrast = diffuse.rgb - neighbor;
    float dist = length(v_wPos);
    float fade = clamp(1.0 - dist / 12.0, 0.0, 1.0);
    diffuse.rgb += contrast * 0.6 * fade;
  }

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0, 1.0, 1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY * (1.0 - diffuse.b * 1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  vec3 wmap    = normalize(cross(dFdx(v_wPos), dFdy(v_wPos)));
  
  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb * v_refl.a;
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a * (clamp(v_extra.r * 10.0, 8.2, 8.8) - 7.8);
      diffuse.rgb *= 1.0 - 0.6 * mask;
      diffuse.rgb += v_refl.rgb * mask;
    }
  }

  #if NL_SHADE_MODE == 2
    if (v_extra.b <= 0.9) {
      float ao = mix(1.0, 0.75, smoothstep(0.64, 0.62, v_color1.g));
      diffuse.rgb *= ao;
    }
  #elif NL_SHADE_MODE == 1
    if (v_extra.b <= 0.9) {
      float ao = mix(1.0, 0.75, smoothstep(0.74, 0.52, v_color1.g));
      diffuse.rgb *= ao;
    }

    float t = 2.0 * PI * TimeOfDay.x;
    vec3 sunDir = vec3(-sin(t), cos(t), 0.0);

    if (v_extra.b <= 0.9 && v_isTree <= 0.5) {
        float ds = dirShadow(wmap, sunDir, v_lightmapUV.y);
        diffuse.rgb *= ds;
    }
  #endif

  #if !defined(TRANSPARENT)
    if (v_isTree <= 0.3 && !(v_extra.b > 0.9)) {
      diffuse.rgb += glow;
    }
  #endif

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);
  diffuse.rgb = colorCorrection(diffuse.rgb);
  
  #ifdef BGFX_SHADER_LANGUAGE_GLSL
  if (v_extra.b > 0.9 && v_refl.a > 0.0) {
    vec3 wmap    = normalize(cross(dFdx(v_wPos), dFdy(v_wPos)));
    vec3 viewDir = normalize(v_wPos);
    float NdotV  = clamp(abs(dot(viewDir, wmap)), 0.0, 1.0);
    vec3 correctedSky = colorCorrection(v_refl.rgb);

    diffuse.rgb = ssr(v_wPos, viewDir, wmap, NdotV, correctedSky);
  }
  #endif

  gl_FragColor = diffuse;
}