#ifndef INSTANCING
  $input v_worldPos, v_underwaterRainTimeDay
#endif

#include <bgfx_shader.sh>

#ifndef INSTANCING
  #include <newb/main.sh>
  uniform vec4 TimeOfDay;
  uniform vec4 FogColor;
  uniform vec4 FogAndDistanceControl;
#endif

SAMPLER2D_AUTOREG(s_NoiseVoxel);
SAMPLER2D_AUTOREG(s_BlueNoise);

void main() {
  #ifndef INSTANCING
    vec3 viewDir = normalize(v_worldPos);

    nl_environment env;
    env.end = false;
    env.nether = false;
    env.underwater = v_underwaterRainTimeDay.x > 0.5;
    env.rainFactor = v_underwaterRainTimeDay.y;
    env.dayFactor = v_underwaterRainTimeDay.w;
    env.fogCol = FogColor.rgb;
    env = calculateSunParams(env, TimeOfDay.x);

    nl_skycolor skycol = nlOverworldSkyColors(env);

    vec3 skyColor = nlRenderSky(skycol, env, -viewDir, v_underwaterRainTimeDay.z, true);
    
    float daymask = smoothstep(0.001, -0.2, env.dayFactor);
    daymask = clamp(daymask, 0.0, 1.0);
    
    float rainmask = 1.0 - env.rainFactor;
    float underwatermask = env.underwater ? 0.0 : 1.0;
    float mask = daymask*rainmask*underwatermask;
    
    float dither = texture2D(s_BlueNoise, mod(gl_FragCoord.xy,256.0)/256.0).r;
    vec3 aurora = GetAurora(viewDir, v_underwaterRainTimeDay.z, dither, s_NoiseVoxel);
    skyColor += aurora*mask;
    
    #ifdef NL_SHOOTING_STAR
      skyColor += NL_SHOOTING_STAR*nlRenderShootingStar(viewDir, env.fogCol, v_underwaterRainTimeDay.z);
    #endif
    #ifdef NL_GALAXY_STARS
      skyColor += NL_GALAXY_STARS*nlRenderGalaxy(viewDir, env.fogCol, env, v_underwaterRainTimeDay.z);
    #endif

    skyColor = colorCorrection(skyColor);

    gl_FragColor = vec4(skyColor, 1.0);
  #else
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  #endif
}
