$input v_color0, v_fog, v_light, v_glintuv, v_wpos

#include <bgfx_shader.sh>
#include <MinecraftRenderer.Materials/ActorUtil.dragonh>
#include <newb/main.sh>

uniform vec4 ChangeColor;
uniform vec4 OverlayColor;
uniform vec4 GlintColor;
uniform vec4 MatColor;
uniform vec4 MultiplicativeTintColor;
uniform vec4 TileLightColor;
uniform vec4 ColorBased;
uniform vec4 TimeOfDay;

SAMPLER2D_AUTOREG(s_GlintTexture);

void main() {
  #if defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    return;
  #endif

  vec4 albedo = vec4(mix(vec3(1.0, 1.0, 1.0), v_color0.rgb, ColorBased.x), 1.0);

  #ifdef MULTI_COLOR_TINT
    albedo = applyMultiColorChange(albedo, ChangeColor.rgb, MultiplicativeTintColor.rgb);
  #else
    albedo = applyColorChange(albedo, ChangeColor, albedo.a);
    albedo.a *= ChangeColor.a;
  #endif

  albedo = applyOverlayColor(albedo, OverlayColor);

  #ifdef ALPHA_TEST
    if (albedo.a < 0.5) {
      discard;
    }
  #endif

  vec4 light = nlGlint(v_light, v_glintuv, s_GlintTexture, GlintColor, TileLightColor, albedo);
  
  vec3 unlitColor = albedo.rgb;
  albedo.rgb *= albedo.rgb * light.rgb;
  
  float t = 2.0 * PI * TimeOfDay.x;
  vec3 sunDir = vec3(-sin(t), cos(t), 0.0);

  vec3 wmap = normalize(cross(dFdx(v_wpos), dFdy(v_wpos)));
  float skylight = luminance(v_light.rgb);
  float ds = dirShadow(wmap, sunDir, skylight);
  albedo.rgb *= ds;
  
  bool glowing = v_color0.a <= 0.99;
  if (glowing) {
    float glowBoost = 0.65;
    albedo.rgb += unlitColor * glowBoost;
  }

  albedo.rgb = mix(albedo.rgb, v_fog.rgb, v_fog.a);

  albedo.rgb = colorCorrection(albedo.rgb);

  gl_FragColor = albedo;
}
