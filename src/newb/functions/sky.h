#ifndef SKY_H
#define SKY_H

#include "detection.h"
#include "noise.h"

struct nl_skycolor {
  vec3 zenith;
  vec3 horizon;
  vec3 horizonEdge;
};

// rainbow spectrum
vec3 spectrum(float x) {
  vec3 s = vec3(x-0.5, x, x+0.5);
  s = smoothstep(1.0,0.0,abs(s));
  return s*s;
}

vec3 getUnderwaterCol(vec3 FOG_COLOR) {
  return 2.0*NL_UNDERWATER_TINT*FOG_COLOR*FOG_COLOR;
}

vec3 getEndZenithCol() {
  return NL_END_ZENITH_COL;
}

vec3 getEndHorizonCol() {
  return NL_END_HORIZON_COL;
}

nl_skycolor nlEndSkyColors(nl_environment env) {
  nl_skycolor s;
  s.zenith = getEndZenithCol();
  s.horizon = getEndHorizonCol();
  s.horizonEdge = s.horizon;
  return s;
}

nl_skycolor nlOverworldSkyColors(nl_environment env) {
  nl_skycolor s;
  float f = 1.0 + 2.0*(1.0-max(-env.dayFactor, 0.0));
  float nightFactor = step(env.dayFactor, 0.0);
  s.zenith = mix(NL_DAY_ZENITH_COL, NL_NIGHT_ZENITH_COL*f, nightFactor);
  s.horizon = mix(NL_DAY_HORIZON_COL, NL_NIGHT_HORIZON_COL*f, nightFactor);
  s.horizonEdge = mix(NL_DAY_EDGE_COL, NL_NIGHT_EDGE_COL*f, nightFactor);

  float dawnFactor = 1.0-smoothstep(0.0, 0.7, abs(env.dayFactor));
  dawnFactor *= dawnFactor*dawnFactor;
  dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
  s.zenith = mix(s.zenith, NL_DAWN_ZENITH_COL, dawnFactor);
  s.horizon = mix(s.horizon, NL_DAWN_HORIZON_COL, dawnFactor);
  s.horizonEdge = mix(s.horizonEdge, NL_DAWN_EDGE_COL, dawnFactor);

  float zh = dot(s.zenith, vec3_splat(0.33));
  float hh = dot(s.horizon, vec3_splat(0.33));
  float rainMix = env.rainFactor*NL_SKY_RAIN_MIX_FACTOR;
  s.zenith = mix(s.zenith, NL_RAIN_ZENITH_COL*zh, rainMix);
  s.horizon = mix(s.horizon, NL_RAIN_HORIZON_COL*hh, rainMix);
  s.horizonEdge = mix(s.horizonEdge, s.horizon, env.rainFactor);

  if (env.underwater) {
    vec3 underwaterFog = env.fogCol*env.fogCol*NL_UNDERWATER_TINT;
    s.zenith = mix(2.0*underwaterFog, underwaterFog*zh, 0.8);
    s.horizon = mix(2.0*underwaterFog, underwaterFog*hh, 0.8);
    s.horizonEdge = s.horizon;
  }

  return s;
}

nl_skycolor nlSkyColors(nl_environment env) {
  if (env.end) {
    return nlEndSkyColors(env);
  }
  return nlOverworldSkyColors(env);
}


vec3 renderOverworldSky(nl_skycolor skyCol, nl_environment env, vec3 viewDir, bool isSkyPlane) {
  float avy = abs(viewDir.y);
  float mask = 0.5 + (0.5*viewDir.y/(0.4 + avy));

  vec2 g = clamp(0.5 - 0.5*vec2(dot(env.sunDir, viewDir), dot(env.moonDir, viewDir)), 0.0, 1.0);
  vec2 g1 = 1.0-mix(sqrt(g), g, env.rainFactor);
  vec2 g2 = g1*g1;
  vec2 g4 = g2*g2;
  vec2 g8 = g4;
  float mg8 = (g8.x*0.5+g8.y)*mask*max(0.0, 0.5-0.9*env.rainFactor);

  float vh = max(0.0, 1.0 - viewDir.y*viewDir.y);
  vh = max(vh, 0.0001);
  float vh2 = pow(vh, 1.2);
  vh2 = mix(vh2, mix(1.0, vh2*vh2, NL_SKY_VOID_FACTOR), step(viewDir.y, 0.0));
  vh2 = mix(vh2, 1.0, mg8);
  float vh4 = vh2*vh2;

  float gradient1 = vh4*vh4;
  float gradient2 = 0.6*gradient1 + 0.1*vh2;
  gradient1 *= gradient1;
  gradient1 = mix(gradient1*gradient1, 1.0, mg8);
  gradient2 = mix(gradient2, 1.0, mg8);

  float dawnFactor = max(0.0001, 0.98-env.dayFactor*env.dayFactor);
  float df = mix(1.0, g2.x, dawnFactor*dawnFactor);
  df = max(df, 0.1);
  vec3 sky = mix(skyCol.horizon, skyCol.horizonEdge, gradient1*df*df);
  sky = mix(skyCol.zenith, sky, gradient2*df);

  sky *= 1.7+1.7*gradient2;
  sky *= (1.0 + (2.0*mg8 + 7.0*mg8*mg8)*mask)*mix(1.0, mask, NL_SKY_VOID_DARKNESS);

  if (!isSkyPlane) {
    float source = max(0.0, (mg8-0.22)/0.78);
    source *= source;
    source *= source;
    sky *= 1.0 + 15.0*source*(1.0-env.rainFactor);
  }

  #ifdef NL_RAINBOW
    if (!env.underwater) {
      float rainbowFade = 0.5 + 0.5*viewDir.y;
      rainbowFade *= rainbowFade;
      rainbowFade *= mix(NL_RAINBOW_CLEAR, NL_RAINBOW_RAIN, env.rainFactor);
      rainbowFade *= 0.5+0.5*env.dayFactor;
      sky += spectrum(24.2*(0.85-g.x))*rainbowFade*skyCol.horizon;
    }
  #endif
  sky = max(sky, 0.001);

  return sky;
}

float EndHorizonRay(float a, float t, vec3 v, float ra, float tm){
  float s = sin(a*7.5 + t*tm);
  s = s*s;
  s *= 0.4 + 0.7*cos(a*ra - 4.0*t);
  return smoothstep(0.59-s, -0.55, v.y);
}

vec3 renderEndSky(vec3 horizonCol, vec3 zenithCol, vec3 viewDir, float t) {
  t *= 0.15;
  float a = atan2(viewDir.x, viewDir.z);
  vec3 v = viewDir;
  v.y = smoothstep(0.0, 1.9,abs(v.y));

  float g = EndHorizonRay(a, t, v, 4.0, 1.1);
  float g2 = EndHorizonRay(a, t, v, 2.0, 0.8);

  float n1 = 0.5 + 0.5*sin(3.0*a + t + 10.0*viewDir.x*viewDir.y);
  float n2 = 0.5 + 0.5*cos(5.0*a + 0.5*t + 5.0*n1 + 0.1*sin(40.0*a -4.0*t));

  float waves = 0.8*n2*n1 + 0.0*n1;
  float grad = 0.5 + 0.5*viewDir.y;
  float streaks = waves*(1.2 - grad*grad*grad);
  streaks += (1.0-streaks)*smoothstep(1.0-waves, -1.0, viewDir.y);

  float f = 1.5*streaks + 0.5*smoothstep(1.0, -0.5, viewDir.y);
  g *= g*g;

  float verticalFade = pow(1.0 - abs(viewDir.y), 5.0);

  vec3 sky = vec3_splat(0.0);

  sky += mix(zenithCol, horizonCol, f)*(1.0 * f)*verticalFade;

  vec3 rg = (g*g*3.0)*vec3(0.9,0.4,0.8)*2.95;
  vec3 rg2 = (g2*g2*g2*3.0)*vec3(0.82,0.28,1.0)*0.95;
  sky += rg + rg2;

  return sky;
}

vec3 nlRenderSky(nl_skycolor skycol, nl_environment env, vec3 viewDir, float t, bool isSkyPlane) {
  vec3 sky;
  viewDir.y = -viewDir.y;

  if (env.end) {
    sky = renderEndSky(skycol.horizon, skycol.zenith, viewDir, t);
  } else if (env.nether) {
    sky = env.fogCol*2.0;
  } else {
    sky = renderOverworldSky(skycol, env, viewDir, isSkyPlane);
    #ifdef NL_UNDERWATER_STREAKS
       if (env.underwater) {
        float a = atan2(viewDir.x, viewDir.z);
        float sunAz = atan2(env.sunDir.x, env.sunDir.z);
        float relAz = a - sunAz;
        float grad = 0.5 + 0.5*viewDir.y;
        grad *= grad;
        float spread = (0.3 + 0.7*sin(12.0*relAz + 0.4*t + 3.0*sin(20.0*relAz - 0.6*t)));
        spread *= (0.3 + 0.7*sin(11.0*relAz - sin(0.9*t)))*grad;
        spread += (1.0-spread)*grad;
        float streaks = spread*spread;
        streaks *= streaks;
        streaks = (spread + 4.0*grad*grad + 4.0*streaks*streaks);
        sky += 2.0*streaks*skycol.horizon;
      }
    #endif
  }

  return sky;
}

// shooting star
vec3 nlRenderShootingStar(vec3 viewDir, vec3 FOG_COLOR, float t) {
  // transition vars
  float h = t / (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD);
  float h0 = floor(h);
  t = (NL_SHOOTING_STAR_DELAY + NL_SHOOTING_STAR_PERIOD) * (h-h0);
  t = min(t/NL_SHOOTING_STAR_PERIOD, 1.0);
  float t0 = t*t;
  float t1 = 1.0-t0;
  t1 *= t1; t1 *= t1; t1 *= t1;

  // randomize size, rotation, add motion, add skew
  float r = fract(sin(h0) * 43758.545313);
  float a = 6.2831*r;
  float cosa = cos(a);
  float sina = sin(a);
  vec2 uv = viewDir.xz * (6.0 + 4.0*r);
  uv = vec2(cosa*uv.x + sina*uv.y, -sina*uv.x + cosa*uv.y);
  uv.x += t1 - t;
  uv.x -= 2.0*r + 3.5;
  uv.y += viewDir.y * 1.8;

  // draw star
  float g = 1.0-min(abs((uv.x-0.95))*20.0, 1.0); // source glow
  float s = 1.0-min(abs(8.0*uv.y), 1.0); // line
  s *= s*s*smoothstep(-1.0+1.96*t1, 0.98-t, uv.x); // decay tail
  s *= s*s*smoothstep(1.0, 0.98-t0, uv.x); // decay source
  s *= 1.0-t1; // fade in
  s *= 1.0-t0; // fade out
  s *= 0.7 + 16.0*g*g;
  s *= max(1.0-FOG_COLOR.r-FOG_COLOR.g-FOG_COLOR.b, 0.0); // fade out during day
  return s*vec3(0.8, 0.9, 1.0);
}

// Galaxy stars - needs further optimization
vec3 nlRenderGalaxy(vec3 vdir, vec3 fogColor, nl_environment env, float t) {
  if (env.underwater) {
    return vec3_splat(0.0);
  }

  t *= NL_GALAXY_SPEED;

  // rotate space
  float cosb = sin(0.2*t);
  float sinb = cos(0.2*t);
  vdir.xy = mul(mat2(cosb, sinb, -sinb, cosb), vdir.xy);

  // noise
  float n0 = 0.5 + 0.5*sin(5.0*vdir.x)*sin(5.0*vdir.y - 0.5*t)*sin(5.0*vdir.z + 0.5*t);
  float n1 = noise3D(15.0*vdir + sin(0.85*t + 1.3));
  float n2 = noise3D(50.0*vdir + 1.0*n1 + sin(0.7*t + 1.0));
  float n3 = noise3D(200.0*vdir - 10.0*sin(0.4*t + 0.500));

  // stars
  n3 = smoothstep(0.04,0.3,n3+0.02*n2);
  float gd = vdir.x + 0.1*vdir.y + 0.1*sin(10.0*vdir.z + 0.2*t);
  float st = n1*n2*n3*n3*(1.0+70.0*gd*gd);
  st = (1.0-st)/(1.0+400.0*st);
  vec3 stars = (0.8 + 0.2*sin(vec3(8.0,6.0,10.0)*(2.0*n1+0.8*n2) + vec3(0.0,0.4,0.82)))*st;

  // glow
  float gfmask = abs(vdir.x)-0.15*n1+0.04*n2+0.25*n0;
  float gf = 1.0 - (vdir.x*vdir.x + 0.03*n1 + 0.2*n0);
  gf *= gf;
  gf *= gf*gf;
  gf *= 1.0-0.3*smoothstep(0.2, 0.3, gfmask);
  gf *= 1.0-0.2*smoothstep(0.3, 0.4, gfmask);
  gf *= 1.0-0.1*smoothstep(0.2, 0.1, gfmask);
  vec3 gfcol = normalize(vec3(n0, cos(2.0*vdir.y), sin(vdir.x+n0)));
  stars += (0.4*gf + 0.012)*mix(vec3(0.5, 0.5, 0.5), gfcol*gfcol, NL_GALAXY_VIBRANCE);

  stars *= mix(1.0, NL_GALAXY_DAY_VISIBILITY, env.dayFactor);

  return stars*(1.0-env.rainFactor);
}

vec3 endGalaxy(vec3 vdir, float t) {
    t *= NL_GALAXY_SPEED;

    float cosb = sin(0.2*t);
    float sinb = cos(0.2*t);
    vdir.xy = mul(mat2(cosb, sinb, -sinb, cosb), vdir.xy);

    float n0 = 0.5 + 0.5*sin(5.0*vdir.x)*sin(5.0*vdir.y - 0.5*t)*sin(5.0*vdir.z + 0.5*t);
    float n1 = noise3D(15.0*vdir + sin(0.85*t + 1.3));
    float n2 = noise3D(50.0*vdir + 1.0*n1 + sin(0.7*t + 1.0));
    float n3 = noise3D(200.0*vdir - 10.0*sin(0.4*t + 0.500));

    n3 = smoothstep(0.04,0.3,n3+0.02*n2);
    float gd = vdir.x + 0.1*vdir.y + 0.1*sin(10.0*vdir.z + 0.2*t);
    float st = n1*n2*n3*n3*(1.0+70.0*gd*gd);
    st = (1.0-st)/(1.0+400.0*st);

    vec3 starColor = (0.8 + 0.2*sin(vec3(8.0,6.0,10.0)*(2.0*n1+0.8*n2) + vec3(0.0,0.4,0.82))) * st;
    float starLum = luminance(starColor);
    vec3 purpleTint = vec3(1.2, 0.6, 1.4);
    starColor = mix(vec3_splat(starLum), starLum * purpleTint, 0.6);

    starColor *= 5.9 + 0.1 * sin(n1 * 30.0 + vec3(0.0, 1.0, 2.0));

    float gf = 1.0 - (vdir.x*vdir.x + 0.03*n1 + 0.2*n0);
    gf *= gf;
    gf *= gf*gf;
    vec3 dustColor = vec3(0.8, 0.2, 0.7);
    vec3 glowColor = dustColor * (0.4*gf + 0.012);

    return starColor + glowColor;
}

vec4 renderVortex(vec3 vdir, float t) {
  t *= 0.75;

  float r = 4.0;
  vec3 vr = vdir;
  vr.xy = mul(mat2(cos(r), -sin(r), sin(r), cos(r)), vr.xy);

  vec3 vd = vr-vec3(0.0, -1.0, 0.0);
  float nl = sin(1.0*vd.x + t)*sin(15.0*vd.y - t)*sin(15.0*vd.z + t);
  float a = atan2(vd.x, vd.z);

  float d = 1.8*length(vd + 0.01*nl);
  float d0 = (0.6-d)/0.9;
  float dm0 = 1.0-max(d0, 0.0);

  float gl = 1.0-clamp(-0.3*d0, 0.0, 1.0);
  float gla = pow(1.0-min(abs(d0), 1.0), 6.0);
  float gl8 = pow(gl, 9.0);

  float hole = 0.5*pow(dm0, 15.0) + 0.3*pow(dm0, 5.0);
  float bh = (gla + 0.1*gl8 + 0.4*gl8*gl8) * hole;

  float df = sin(3.0*a - 4.0*d + 10.0*pow(1.4-d, 2.0) + t);
  df *= 0.4 + 1.0*sin(1.0*a + d + 2.0*t - 2.0*df);
  bh *= 0.7 + pow(df, 2.0)*hole*max(1.0-bh, 0.0);

  vec3 col = bh*3.5*mix(vec3(0.82, 0.28, 1.0), vec3(0.82, 0.28, 1.0) , min(bh, 1.0));
  return vec4(col, hole);
}

float pow2(float x) { return x * x; }
float clamp01(float x) { return clamp(x, 0.0, 1.0); }
float sqrt1(float x) { return sqrt(max(x, 0.0)); }

vec3 GetAurora(vec3 vDir, float time, float dither, sampler2D noiseTex) {
  float height = abs(vDir.y);
  float visibility = smoothstep(0.0, 0.5, height);
  visibility = clamp(visibility, 0.0, 1.0);
  if (visibility < 0.01) return vec3(0.0);

  vec3 aurora = vec3(0.0);
  vDir.xz /= max(vDir.y*0.1, 0.0);
  vec2 cameraPosM = vec2(0.0);
  cameraPosM.x += time * 0.65;

  const int sampleCount = 5;
  const int sampleCountP = sampleCount + 12;

  float ditherM = dither + 10.0;
  float auroraAnimate = time * 0.0035;

  for (int i = 0; i < sampleCount; i++) {
    float current = pow2((float(i) + ditherM) / float(sampleCountP));
    vec2 planePos = vDir.xz * (0.005 + current) * 8.0 + cameraPosM;
    planePos *= 0.0003;
    float noise = texture2D(noiseTex, planePos).r;
    noise = pow2(pow2(pow2(pow2(1.0 - 1.0 * abs(noise - 0.5)))));
    noise *= texture2D(noiseTex, planePos * 3.5 + auroraAnimate).b;
    noise *= texture2D(noiseTex, planePos * 1.0 - auroraAnimate).g;
    float currentM = 1.0 - current;
    aurora += noise * currentM * mix(vec3(1.0, 0.0, 1.0)*1.5, vec3(0.0, 1.0, 1.0)*24.5, pow2(pow2(currentM)))*3.0;
  }
  aurora *= 3.8;
  return aurora * visibility / float(sampleCount);
}


#endif
