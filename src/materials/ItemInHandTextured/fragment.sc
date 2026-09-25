$input v_color0, v_fog, v_light, v_texcoord0, v_edgemap,v_wpos

#include <bgfx_shader.sh>
#include <MinecraftRenderer.Materials/ActorUtil.dragonh>
#include <newb/main.sh>

uniform vec4 ChangeColor;
uniform vec4 OverlayColor;
uniform vec4 ColorBased;
uniform vec4 MatColor;
uniform vec4 MultiplicativeTintColor;
uniform vec4 TimeOfDay;

SAMPLER2D_AUTOREG(s_MatTexture);

void main() {
  #if defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4_splat(0.0);
    return;
  #endif

  vec4 albedo = MatColor * texture2D(s_MatTexture, v_texcoord0);

  #ifdef ALPHA_TEST
    if (albedo.a < 0.5) {
      discard;
    }
  #endif

  #ifdef MULTI_COLOR_TINT
    albedo = applyMultiColorChange(albedo, ChangeColor.rgb, MultiplicativeTintColor.rgb);
  #else
    albedo = applyColorChange(albedo, ChangeColor, albedo.a);
  #endif

  albedo.rgb *= mix(vec3_splat(1.0), v_color0.rgb, ColorBased.x);

  albedo = applyOverlayColor(albedo, OverlayColor);

  albedo.rgb *= albedo.rgb * v_light.rgb;

  albedo.rgb *= nlEntityEdgeHighlight(v_edgemap);
  
  float t = 2.0 * PI * TimeOfDay.x;
  vec3 sunDir = vec3(-sin(t), cos(t), 0.0);

  vec3 wmap = normalize(cross(dFdx(v_wpos), dFdy(v_wpos)));
  float skylight = luminance(v_light.rgb);
  float ds = dirShadow(wmap, sunDir, skylight);
  albedo.rgb *= ds;
  
  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, 1.0);
  albedo.rgb += glow;

  albedo.rgb = mix(albedo.rgb, v_fog.rgb, v_fog.a);

  albedo.rgb = colorCorrection(albedo.rgb);

  gl_FragColor = albedo;
}
