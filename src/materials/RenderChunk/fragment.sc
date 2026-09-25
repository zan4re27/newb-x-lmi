$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_wPos, v_isTree, v_time

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);

uniform vec4 TimeOfDay;
uniform vec4 CameraPosition;

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
  
  vec2 offset = 1.0 / vec2(textureSize(s_MatTexture, 0));
  vec2 sampleUV = v_texcoord0 + offset * vec2(-0.02, -0.02);
  vec4 neighborTex = texture2D(s_MatTexture, sampleUV);
  if (neighborTex.a > 0.6) {
    vec3 neighbor = neighborTex.rgb;
    vec3 contrast = diffuse.rgb - neighbor;
    float dist = length(v_wPos);
    float fade = clamp(1.0 - dist / 10.0, 0.0, 1.0);
    diffuse.rgb += contrast * 0.38 * fade;
  }

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
  
  float ao = mix(1.0, 0.8, smoothstep(0.74, 0.52, v_color1.g));
  diffuse.rgb *= ao;
  
  float t = 2.0 * PI * TimeOfDay.x;
  vec3 sunDir = vec3(-sin(t), cos(t), 0.0);
  vec3 wmap = normalize(cross(dFdx(v_wPos), dFdy(v_wPos)));

  if (v_extra.b <= 0.9) {
    float ds = dirShadow(wmap, sunDir, v_lightmapUV.y);
    diffuse.rgb *= ds;
  }
  
  #if !defined(TRANSPARENT)
    if (v_isTree <= 0.3 && !(v_extra.b > 0.9)) {
      diffuse.rgb += glow;
    }
  #endif

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

  gl_FragColor = diffuse;
}
