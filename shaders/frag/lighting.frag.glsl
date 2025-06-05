#version 450

layout(location = 0) in vec2 inUV; // from fullscreen‐triangle VS

layout(location = 0) out vec4 outColor; // final lit color

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

// 1) UBO with camera + light + debug settings
layout(set = 1, binding = 0) uniform UBOParams {
    vec3 camPos;
    float _padding0;
    vec4 lightDir; // (direction.xyz, unused)
    float exposure;
    float gamma;
    float radiance;
    float prefilteredCubeMipLevels;
    float scaleIBLAmbient;
    float debugViewInputs;
    float debugViewEquation;
} uboParams;

// 2) IBL samplers
layout(set = 2, binding = 1) uniform samplerCube samplerIrradiance;
layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D samplerBRDFLUT;

// 3) G-Buffer samplers (one sampler2D per G-buffer target)
layout(set = 3, binding = 0) uniform sampler2D gAlbedo;
layout(set = 3, binding = 1) uniform sampler2D gNormalRough;
layout(set = 3, binding = 2) uniform sampler2D gMatParams;
layout(set = 3, binding = 3) uniform sampler2D gPosition;
layout(set = 3, binding = 4) uniform sampler2D gDepth;

const float PI = 3.14159265359;

vec3 ReconstructWorldPos(vec2 uv, float depth) // depth is Vulkan NDC Z [0,1]
{
    vec4 clipPosH;
    clipPosH.xy = uv * 2.0 - 1.0; // Convert UV [0,1] to NDC XY [-1,1]
    clipPosH.z = depth; // Use Vulkan NDC Z [0,1] directly
    clipPosH.w = 1.0;

    vec4 viewPosH = ubo.invProjection * clipPosH; // ubo.invProjection should be inverse(ubo.projection)
    vec3 viewPos = viewPosH.xyz / viewPosH.w; // Perspective divide

    vec4 worldPosH = ubo.invView * vec4(viewPos, 1.0); // ubo.invView should be inverse(ubo.view)
    return worldPosH.xyz;
}

// Simple Fresnel Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith’s Schlick-GGX Geometry term
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith’s Geometry function, combining both view‐ and light‐terms
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

void main()
{
    // 1) Sample G-Buffer
    vec4 albSample = texture(gAlbedo, inUV);
    vec4 nrSample = texture(gNormalRough, inUV);
    vec4 mpSample = texture(gMatParams, inUV);
    float depth = texture(gDepth, inUV).r;
    // If using Sascha's position attachment:
    // vec3 fragPos = texture(gPosition, inUV).rgb; // Assuming world-space position is stored here

    // Albedo & alpha
    vec3 albedo = albSample.rgb;
    float alpha = albSample.a; // Note: Sascha uses albedo.a for specular map

    // Unpack normal (0..1 → −1..+1) and roughness
    vec3 N = normalize(nrSample.xyz * 2.0 - 1.0);
    float roughness = nrSample.w;

    // Unpack emissive and metallic
    vec3 emissive = mpSample.rgb;
    float metallic = mpSample.a;

    // Reconstruct world position
    // If using Sascha's position attachment, replace this with 'fragPos' from texture lookup
    // vec3 worldPos = ReconstructWorldPos(inUV, depth);
    vec3 worldPos = texture(gPosition, inUV).rgb;

    // Compute view and light vectors
    vec3 V = normalize(uboParams.camPos - worldPos);
    vec3 L = normalize(-uboParams.lightDir.xyz); // Singular directional light
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // Sascha's ambient equivalent (simplified for single light)
    float ambient = 0.01; // Or some small constant to provide minimal global illumination

    // Initialize fragment color with ambient term
    vec3 fragcolor = albedo * ambient; // Sascha's ambient

    // --- Sascha's Direct Lighting (adapted for singular directional light) ---
    // Note: Sascha uses albedo.a for specular intensity/power
    // If your gAlbedo.a is just alpha, you might need a dedicated specular map or a constant value.
    float specularStrength = alpha; // Assuming albedo.a holds specular strength (like Sascha's example)
    // If not, use a constant like 1.0 or derive from roughness/metallic.

    // No distance attenuation for directional light
    // Light to fragment vector L is already normalized

    // Diffuse part (Blinn-Phong style)
    vec3 singularLightColor = vec3(1);
    float singularLightIntensity = 1;
    vec3 diff = singularLightColor * albedo.rgb * NdotL * singularLightIntensity;

    // Specular part (Blinn-Phong style)
    vec3 R = reflect(-L, N); // Reflection vector for specular
    float RdotV = max(0.0, dot(R, V));
    // Use a fixed shininess or derive from roughness (e.g., pow(RdotV, 1.0/roughness))
    float shininess = 16.0; // Matches Sascha's example
    vec3 spec = singularLightColor * specularStrength * pow(RdotV, shininess) * singularLightIntensity;

    fragcolor += diff + spec;

    // --- Original IBL Calculations (Keep this if you want PBR-like reflections/global illumination) ---
    // PBR's F0 for IBL
    vec3 F0_PBR = mix(vec3(0.04), albedo, metallic);

    vec3 irradiance = texture(samplerIrradiance, N).rgb;
    vec3 diffuseIBL_base = irradiance * albedo;

    vec3 R_ibl = reflect(-V, N);
    vec3 prefiltered_ibl = textureLod(
            prefilteredMap,
            R_ibl,
            roughness * uboParams.prefilteredCubeMipLevels
        ).rgb;

    vec2 brdf_lut_sample = texture(samplerBRDFLUT, vec2(NdotV, roughness)).rg;

    vec3 specular_ibl_component = prefiltered_ibl * (F0_PBR * brdf_lut_sample.x + brdf_lut_sample.y);

    vec3 kS_ibl_fresnel = F0_PBR;
    vec3 kD_ibl = (vec3(1.0) - kS_ibl_fresnel) * (1.0 - metallic);
    vec3 diffuse_ibl_component = kD_ibl * diffuseIBL_base;

    vec3 ambient_PBR = (diffuse_ibl_component + specular_ibl_component) * uboParams.scaleIBLAmbient;

    // Combine Sascha's direct lighting with your PBR IBL
    fragcolor += ambient_PBR; // Add PBR-based ambient (IBL)
    fragcolor += emissive; // Add emissive from your G-buffer

    // 5) Tone‐map & gamma‐correct
    vec3 color = vec3(1.0) - exp(-fragcolor * uboParams.exposure);
    color = pow(color, vec3(1.0 / uboParams.gamma));

    outColor = vec4(color, 1);
}
