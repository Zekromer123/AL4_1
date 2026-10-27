#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingType;  // 0=None, 1=Lambert, 2=HalfLambert
    int32_t inkEdgeMask;   // box ink edges: 1=top 2=bottom 4=left 8=right, 16=enable (0 = off)
    float32_t pad;
};

struct DirectionalLightData
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
    int32_t active;
    float32_t3 pad;
};

struct PointLightData
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t radius;
    float32_t decay;
    int32_t active;
    float32_t pad;
};

struct SpotLightData
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
    float32_t cosFalloffStart;
    int32_t active;
};

ConstantBuffer<Material> gMaterial : register(b0);

cbuffer LightGroup : register(b1)
{
    float32_t3 ambientColor;
    float32_t ambientIntensity;
    DirectionalLightData dirLight;
    PointLightData pointLights[3];
    SpotLightData spotLights[3];
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ============================================================================
// Procedural Traditional Japanese Washi Paper Generator
// ============================================================================
float Hash21(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float ValueNoise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);

    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float PaperFBM(float2 p)
{
    float val = 0.0f;
    float amp = 0.5f;
    val += amp * ValueNoise2D(p); p = p * 2.03f + float2(13.5f, 27.2f); amp *= 0.5f;
    val += amp * ValueNoise2D(p); p = p * 2.01f + float2(42.1f, 11.8f); amp *= 0.5f;
    val += amp * ValueNoise2D(p);
    return val;
}

float WashiFiber(float2 p, float angle, float stretch)
{
    float s = sin(angle);
    float c = cos(angle);
    float2 rotP = float2(c * p.x - s * p.y, s * p.x + c * p.y);
    rotP.x *= stretch;
    rotP.y *= 1.2f;
    float n = ValueNoise2D(rotP);
    return pow(saturate(1.0f - abs(n * 2.0f - 1.0f)), 5.0f);
}

float3 GeneratePaperColor(float2 screenPos, float2 worldPos)
{
    float2 uvScroll = screenPos * 0.003f + worldPos * 0.015f;

    // 1. Base Washi paper palette
    float3 paperBase = float3(0.965f, 0.952f, 0.925f);   // Warm ivory / rice paper
    float3 paperPulp = float3(0.925f, 0.900f, 0.850f);   // Aged pulp cloud density
    float3 fiberTint = float3(0.780f, 0.720f, 0.630f);   // Subtle organic fiber tint

    // 2. Pulp cloud mottling
    float cloud1 = PaperFBM(uvScroll * 1.5f);
    float cloud2 = PaperFBM(uvScroll * 3.8f + float2(50.0f, 80.0f));
    float pulpDensity = cloud1 * 0.65f + cloud2 * 0.35f;

    float3 col = lerp(paperPulp, paperBase, saturate(pulpDensity * 1.15f - 0.05f));

    // 3. Washi Kozo Fibers (woven strands at natural varied angles)
    float fiber1 = WashiFiber(uvScroll * 5.0f, 0.52f, 9.0f);
    float fiber2 = WashiFiber(uvScroll * 6.5f, -0.85f, 11.0f);
    float fiber3 = WashiFiber(uvScroll * 4.2f, 1.85f, 8.0f);
    float fibers = saturate(fiber1 * 0.5f + fiber2 * 0.35f + fiber3 * 0.25f);
    col = lerp(col, fiberTint, fibers * 0.22f);

    // 4. Fine paper tooth / microscopic grain
    float fineNoise = Hash21(screenPos * 0.45f + float2(17.0f, 31.0f));
    float grain = (fineNoise - 0.5f) * 0.038f;
    col += grain;

    // 5. Sparse natural bark flecks
    float fleckNoise = Hash21(floor(uvScroll * 45.0f));
    if (fleckNoise > 0.992f)
    {
        float fleckMask = 1.0f - saturate(length(frac(uvScroll * 45.0f) - 0.5f) * 3.5f);
        col = lerp(col, float3(0.62f, 0.55f, 0.45f), fleckMask * 0.35f);
    }

    // 6. Subtle vignette
    float2 normCoord = (screenPos - float2(640.0f, 360.0f)) / 720.0f;
    float dist = length(normCoord);
    float vignette = 1.0f - smoothstep(0.75f, 1.35f, dist) * 0.06f;
    col *= vignette;

    return saturate(col);
}

// ============================================================================
// Paper & Ink (sumi-e) shading for 3D objects
// ============================================================================
static const float3 kInk = float3(0.075f, 0.065f, 0.060f);

// Diagonal brush hatching in world space (stable while the camera scrolls)
float InkHatch(float2 worldXY)
{
    float coord = (worldXY.x + worldXY.y) * 16.0f + ValueNoise2D(worldXY * 5.0f) * 1.2f;
    float d = abs(frac(coord) - 0.5f);
    float broken = step(0.35f, ValueNoise2D(worldXY * float2(9.0f, 3.0f))); // dry-brush gaps
    return (1.0f - smoothstep(0.10f, 0.20f, d)) * broken;
}

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // Background Detection:
    // SkyDome sphere has radius 500.0f in world space.
    // Gameplay entities and tiles are strictly within |worldPosition| <= 70.0f and |worldPosition.z| <= 5.0f.
    bool isBackground = (abs(input.worldPosition.z) > 40.0f || length(input.worldPosition) > 150.0f);
    if (isBackground)
    {
        float3 paperColor = GeneratePaperColor(input.position.xy, input.worldPosition.xy);
        output.color = float32_t4(paperColor, 1.0f);
        return output;
    }

    float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);

    // Damage white flash (over-bright color e.g. 4.0)
    if (gMaterial.enableLighting == 0)
    {
        if (gMaterial.color.r >= 2.0f)
        {
            output.color = gMaterial.color * texColor;
            return output;
        }
    }

    float32_t3 N = normalize(input.normal);
    float32_t3 worldPos = input.worldPosition;
    float32_t2 wxy = worldPos.xy;

    // --- 1. Two-tone sumi wash lighting -----------------------------------
    float lightAmt = 0.75f;
    if (dirLight.active != 0)
    {
        float NdotL = dot(N, -normalize(dirLight.direction));
        lightAmt = saturate((NdotL + 0.35f) / 1.35f);
    }
    float edgeNoise = (ValueNoise2D(wxy * 14.0f) - 0.5f) * 0.08f;
    float lit = smoothstep(0.40f, 0.47f, lightAmt + edgeNoise);
    float tone = lerp(0.72f, 1.0f, lit);

    // Point / spot lights add a soft warm wash (e.g. effects)
    float32_t3 extraLight = float32_t3(0.0f, 0.0f, 0.0f);
    for (int32_t i = 0; i < 3; i++)
    {
        if (pointLights[i].active != 0)
        {
            float32_t3 lightVec = worldPos - pointLights[i].position;
            float32_t dist = length(lightVec);
            float32_t NdotL = saturate((dot(N, -normalize(lightVec)) + 0.2f) / 1.2f);
            float32_t attenuation = pow(saturate(1.0f - dist / pointLights[i].radius), pointLights[i].decay);
            extraLight += pointLights[i].color.rgb * NdotL * pointLights[i].intensity * attenuation;
        }
    }
    for (int32_t j = 0; j < 3; j++)
    {
        if (spotLights[j].active != 0)
        {
            float32_t3 lightVec = worldPos - spotLights[j].position;
            float32_t dist = length(lightVec);
            float32_t3 lightDir = normalize(lightVec);
            float32_t NdotL = saturate((dot(N, -lightDir) + 0.2f) / 1.2f);
            float32_t distAtt = pow(saturate(1.0f - dist / spotLights[j].distance), spotLights[j].decay);
            float32_t cosTheta = dot(lightDir, spotLights[j].direction);
            float32_t spotFactor = saturate((cosTheta - spotLights[j].cosFalloffStart) / (spotLights[j].cosAngle - spotLights[j].cosFalloffStart));
            extraLight += spotLights[j].color.rgb * NdotL * spotLights[j].intensity * distAtt * spotFactor;
        }
    }

    // --- 2. Pigment soaked into washi paper -------------------------------
    float3 albedo = gMaterial.color.rgb * texColor.rgb;
    float lum = dot(albedo, float3(0.299f, 0.587f, 0.114f));
    albedo = lerp(lum.xxx, albedo, 0.85f);                                  // slightly muted pigments
    float streak = PaperFBM(wxy * float2(1.6f, 20.0f));                     // dry-brush streaks
    albedo *= lerp(0.90f, 1.05f, streak);
    float3 paper = GeneratePaperColor(input.position.xy, wxy);
    float3 col = albedo * paper * tone + albedo * extraLight * 0.5f;

    // --- 3. Ink hatching in the shadow side -------------------------------
    col = lerp(col, kInk, InkHatch(wxy) * (1.0f - lit) * 0.38f);

    // --- 4. Ink silhouette on round models (everything that is not a tile) --
    // Camera looks along +Z, so faces turned to the camera have N.z < 0.
    float NdotV = saturate(-N.z);
    int mask = gMaterial.inkEdgeMask;
    float rimInk = 0.0f;
    if ((mask & 16) == 0)
    {
        rimInk = 1.0f - smoothstep(0.30f, 0.48f, NdotV + edgeNoise * 1.5f);
    }

    // --- 5. Brush outline on box-shaped tiles ------------------------------
    float boxInk = 0.0f;
    if ((mask & 16) != 0)
    {
        float3 lp = input.localPosition;
        if (NdotV > 0.5f)
        {
            // Front face: ink only on the exposed edges (neighbouring tiles share no line)
            float e = 10.0f;
            if ((mask & 1) != 0) e = min(e, 1.0f - lp.y);
            if ((mask & 2) != 0) e = min(e, 1.0f + lp.y);
            if ((mask & 4) != 0) e = min(e, 1.0f + lp.x);
            if ((mask & 8) != 0) e = min(e, 1.0f - lp.x);
            e += (ValueNoise2D(wxy * 18.0f) - 0.5f) * 0.10f;   // wobbly hand-drawn line
            float fw = max(fwidth(e), 1e-4f);
            boxInk = 1.0f - smoothstep(0.13f, 0.13f + fw * 1.5f, e);
        }
        else
        {
            // Side / top faces seen at a grazing angle: dark ink wash
            boxInk = 0.55f + InkHatch(wxy * 1.7f) * 0.3f;
        }
    }

    float ink = saturate(max(rimInk, boxInk));
    col = lerp(col, kInk, ink * 0.93f);

    output.color = float32_t4(saturate(col), gMaterial.color.a * texColor.a);
    return output;
}
