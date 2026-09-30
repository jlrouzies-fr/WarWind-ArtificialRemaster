// War Wind "Modern Graphics" passes (compiled at runtime by cnc-ddraw, vs_3_0 / ps_3_0).
//
// Inputs, all at frame resolution (1280x720) in the top-left corner of power-of-two textures:
//   Index   palette indices of the finished frame      Palette  RGB in row 0 (cnc-ddraw: 256x256 texture)
//   Mask    WWFX_CLASS_* per pixel                      Shadow   projected shadow coverage
//   EmisLut per palette index: rgb tint, a = strength of the light the colour emits
// Intermediate targets (normalised 0..1 UVs): half-resolution shadow/occlusion, quarter-resolution
// light. Colours are lit in approximately linear space (square / square root).

#define CLASS_NONE 0
#define CLASS_TERRAIN 1
#define CLASS_UNIT 3
#define CLASS_BUILDING 4
#define CLASS_DOODAD 5
#define CLASS_EFFECT 6

float4 TexelOffset : register(c0);   // vertex shader: (-1/targetW, 1/targetH)

struct VsOut
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

VsOut VsQuad(float4 pos : POSITION, float2 uv : TEXCOORD0, float4 color : TEXCOORD1)
{
    VsOut o;
    o.pos = float4(pos.xy + TexelOffset.xy * pos.w, pos.zw);
    o.uv = uv;
    o.color = color;
    return o;
}

// ---- shared pixel shader constants

float4 Frame : register(c0);    // (uv scale x, uv scale y, 1/frameW, 1/frameH): screen uv -> input tex uv
float4 Blur : register(c1);     // blur passes: (step x, step y) in source uv, radius in source texels

sampler2D Index : register(s0);
sampler2D Palette : register(s1);
sampler2D Mask : register(s2);
sampler2D Shadow : register(s3);
sampler2D EmisLut : register(s4);
sampler2D Fog : register(s5);

float ClassAt(float2 texUv)
{
    return floor(tex2D(Mask, texUv).r * 255.0 + 0.5);
}

float PaletteU(float index)
{
    return index * (255.0 / 256.0) + (0.5 / 256.0);
}

float3 ColourAt(float2 texUv, out float index)
{
    index = tex2D(Index, texUv).r;
    return tex2D(Palette, float2(PaletteU(index), 0)).rgb;
}

// Water is ground drawn in the teal ramp of the in-game palette; land tiles and their borders
// use it for no more than a few pixels.
bool IsWaterIndex(float index)
{
    float i = index * 255.0;
    return i > 176.5 && i < 183.5;
}

float OccluderWeight(float cls)
{
    if (cls == CLASS_BUILDING) return 1.0;
    if (cls == CLASS_DOODAD) return 0.8;
    if (cls == CLASS_UNIT) return 0.45;
    return 0.0;
}

// ---- half resolution: r = projected shadow, g = occluders, b = fog of war, a = water

float4 PsPrep(float2 uv : TEXCOORD0) : COLOR0
{
    const float2 corners[4] = { float2(-0.5, -0.5), float2(0.5, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5) };
    float4 acc = 0;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 t = (uv + corners[i] * Frame.zw) * Frame.xy;
        float cls = ClassAt(t);
        acc.r += tex2D(Shadow, t).r;
        acc.g += OccluderWeight(cls);
        acc.b += tex2D(Fog, t).r;
        acc.a += cls == CLASS_TERRAIN && IsWaterIndex(tex2D(Index, t).r) ? 1 : 0;
    }
    return acc * 0.25;
}

// ---- quarter resolution: light emitted by bright palette colours

float4 PsEmissive(float2 uv : TEXCOORD0) : COLOR0
{
    float3 acc = 0;
    [unroll] for (int y = 0; y < 4; ++y)
    [unroll] for (int x = 0; x < 4; ++x)
    {
        float2 t = (uv + (float2(x, y) - 1.5) * Frame.zw) * Frame.xy;
        float index;
        float3 c = ColourAt(t, index);
        float4 e = tex2D(EmisLut, float2(PaletteU(index), 0.5));
        if (ClassAt(t) != CLASS_NONE)
            acc += c * c * e.rgb * e.a;
    }
    return float4(acc / 16.0, 1);
}

// ---- separable Gaussian (9 taps over +-radius), bilinear source

sampler2D BlurSrc : register(s0);

float4 PsBlur(float2 uv : TEXCOORD0) : COLOR0
{
    const float w[5] = { 0.2270270, 0.1945946, 0.1216216, 0.0540541, 0.0162162 };
    float2 step = Blur.xy * (Blur.z / 4.0);
    float4 acc = tex2D(BlurSrc, uv) * w[0];
    [unroll] for (int i = 1; i < 5; ++i)
    {
        acc += tex2D(BlurSrc, uv + step * i) * w[i];
        acc += tex2D(BlurSrc, uv - step * i) * w[i];
    }
    return acc;
}

// ---- point lights, additive quads into the light target; uv = -1..1 across the quad

float4 PsLight(float2 uv : TEXCOORD0, float4 color : TEXCOORD1) : COLOR0
{
    float d = saturate(1.0 - dot(uv, uv));
    return float4(color.rgb * d * d, 1);
}

// ---- final composite at output resolution

float4 Lighting : register(c2);   // (shadow opacity, ao strength, ambient, light intensity)
float4 Grade : register(c3);      // (contrast, saturation, vignette, sharpen)
float4 Extra : register(c4);      // (bloom, map height in uv, fog opacity, time in seconds)
float4 ShadowTint : register(c5);
float4 World : register(c6);      // (world x, world y of frame pixel 0,0; frame width, height)
float4 Water : register(c7);      // (wave distortion px, glints, foam, 0)

sampler2D FinalIndex : register(s0);
sampler2D FinalPalette : register(s1);
sampler2D FinalMask : register(s2);
sampler2D ShadowBlur : register(s3);    // r = shadow, a = water (blurred once)
sampler2D OcclusionBlur : register(s4); // g = occluders, b = fog, a = water (blurred twice: soft edges)
sampler2D LightBuf : register(s5);
sampler2D FinalEmisLut : register(s6);

float Square(float x)
{
    return x * x;
}

float3 FinalColour(float2 t, out float index)
{
    index = tex2D(FinalIndex, t).r;
    return tex2D(FinalPalette, float2(PaletteU(index), 0)).rgb;
}

// ---- water, in world pixels so the animation stays on the map while the camera moves

float Hash(float2 cell)
{
    cell = fmod(cell, 256.0);   // keeps sin() arguments small enough for full precision
    return frac(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
}

float Noise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), f.x), lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), f.x), f.y);
}

// Linear colour of the frame at a fractional frame-pixel position, bilinear over water texels only
// (a land texel counts as the centre colour, so shores do not bleed).
float3 WaterSample(float2 framePx, float3 centre)
{
    float2 base = floor(framePx - 0.5), f = framePx - 0.5 - base;
    float2 texel = Frame.xy / World.zw;
    const float2 corners[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };
    float3 c[4];
    [unroll] for (int k = 0; k < 4; ++k)
    {
        float2 tc = (base + corners[k] + 0.5) * texel;
        float index;
        float3 s = FinalColour(tc, index);
        c[k] = IsWaterIndex(index) ? s * s : centre;
    }
    return lerp(lerp(c[0], c[1], f.x), lerp(c[2], c[3], f.x), f.y);
}

// Returns the water's own linear colour; `highlight` receives light added on top after shading.
float3 WaterColour(float2 uv, float3 centre, out float3 highlight)
{
    float T = Extra.w;
    float2 framePx = uv * World.zw;
    float2 w = framePx + World.xy;

    float2 wave = float2(sin(w.y * 0.11 + T * 1.7) + 0.5 * sin(w.x * 0.07 + w.y * 0.05 + T * 1.1),
                         cos(w.x * 0.09 + T * 1.3) + 0.5 * cos(w.y * 0.06 - w.x * 0.04 + T * 0.9)) / 1.5;
    float3 c = WaterSample(framePx + wave * Water.x, centre);

    float waterAround = tex2D(OcclusionBlur, uv).a;         // about 0.5 at the shoreline, 1 in open water
    float open = saturate((waterAround - 0.5) * 2.5);
    c *= lerp(float3(1.08, 1.06, 1.0), float3(0.88, 0.95, 1.02), open);

    float ridges = Noise(w / 18.0 + T * float2(0.30, 0.20)) + Noise(w / 11.0 - T * float2(0.25, 0.35));
    float caustic = pow(saturate(1.0 - abs(ridges - 1.0)), 4.0);
    c *= 1.0 + Water.y * 0.35 * (caustic - 0.3);

    // Sun glints: pinpoints that twinkle, grouped in slowly drifting patches of reflection.
    float patch = smoothstep(0.45, 0.8, Noise(w / 70.0 + T * float2(0.03, 0.02)));
    float twinkle = Noise(w / 1.6 + T * float2(1.1, 0.7)) * Noise(w / 2.3 - T * float2(0.8, 1.2) + 17.0);
    highlight = float3(0.8, 0.95, 0.95) * smoothstep(0.6, 0.82, twinkle) * patch * Water.y;

    // Foam lines rolling in on a narrow band along the shore.
    float shore = saturate((0.85 - waterAround) / 0.35);
    float rolls = 0.5 + 0.5 * sin(waterAround * 45.0 + T * 2.2 + Noise(w / 7.0) * 4.0);
    float foam = Square(shore) * smoothstep(0.6, 0.9, rolls) * (0.6 + 0.4 * Noise(w / 3.0 + T * 0.4));
    highlight += float3(0.55, 0.65, 0.60) * foam * Water.z * 0.5;
    return c;
}

float4 PsFinal(float2 uv : TEXCOORD0) : COLOR0
{
    float2 t = uv * Frame.xy;
    float index;
    float3 srgb = FinalColour(t, index);
    float cls = floor(tex2D(FinalMask, t).r * 255.0 + 0.5);
    if (cls == CLASS_NONE)
        return float4(srgb, 1);

    if (Grade.w > 0)
    {
        float i0;
        float3 n = FinalColour(t + float2(Frame.z * Frame.x, 0), i0) + FinalColour(t - float2(Frame.z * Frame.x, 0), i0) +
                   FinalColour(t + float2(0, Frame.w * Frame.y), i0) + FinalColour(t - float2(0, Frame.w * Frame.y), i0);
        srgb = saturate(srgb + (srgb - n * 0.25) * Grade.w);
    }
    float3 c = srgb * srgb;

    bool ground = cls == CLASS_TERRAIN;
    bool water = ground && IsWaterIndex(index);
    float3 highlight = 0;
    if (water)
        c = WaterColour(uv, c, highlight);

    float shadow = ground ? saturate(tex2D(ShadowBlur, uv).r * 1.3) : 0;
    float occlusion = ground ? saturate(tex2D(OcclusionBlur, uv).g * 1.6) * Lighting.y : 0;
    // Opacities are perceived darkness (0.5 = half as bright on screen), hence squared for linear light.
    float3 shadowColour = ShadowTint.rgb / dot(ShadowTint.rgb, 1.0 / 3.0) * Square(1.0 - Lighting.x);
    float3 shade = lerp(1.0, shadowColour, shadow) * (1.0 - occlusion);

    float3 light = tex2D(LightBuf, uv).rgb * Lighting.w;
    float4 e = tex2D(FinalEmisLut, float2(PaletteU(index), 0.5));
    float self = e.a * (1.0 + Extra.x);

    c = c * (Lighting.z * shade + light * (1.0 - 0.5 * shadow)) + c * self * 0.35 + light * Extra.x * 0.15;
    c += highlight * (1.0 - 0.8 * shadow) * (1.0 - occlusion);

    c *= lerp(1.0, Square(1.0 - Extra.z), saturate(tex2D(OcclusionBlur, uv).b));

    float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(luma, c, Grade.y);
    c = max(0, (sqrt(max(c, 0)) - 0.5) * Grade.x + 0.5);

    float2 v = (uv - float2(0.5, Extra.y * 0.5)) / float2(0.5, Extra.y * 0.5);
    c *= 1.0 - Grade.z * saturate(dot(v, v) * 0.5 - 0.15);
    return float4(saturate(c), 1);
}
