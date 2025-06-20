#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 projection;
    mat4 invProjection;
    mat4 view;
    mat4 invView;
    mat4 projectionView;
    vec3 cameraPos;
} ubo;

layout(set = 1, binding = 0) uniform UBOParams {
    vec3 camPos;
    float _padding0;
    vec4 lightDir;
    float exposure;
    float gamma;
    float radiance;
    float prefilteredCubeMipLevels;
    float scaleIBLAmbient;
    int debugViewInputs;
    int debugViewEquation;
} uboParams;

layout(set = 2, binding = 1) uniform samplerCube samplerIrradiance;
layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D samplerBRDFLUT;

layout(set = 3, binding = 0) uniform sampler2D gAlbedo;
layout(set = 3, binding = 1) uniform sampler2D gNormalRough;
layout(set = 3, binding = 2) uniform sampler2D gMatParams;
layout(set = 3, binding = 3) uniform sampler2D gPosition;
layout(set = 3, binding = 4) uniform sampler2D gDepth;
layout(set = 3, binding = 5) uniform writeonly image2D drawImage;

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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

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
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = (vec2(pixelCoords) + 0.5) / vec2(imageSize(drawImage));

    vec4 albSample = texture(gAlbedo, uv);
    vec4 nrSample = texture(gNormalRough, uv);
    vec4 mpSample = texture(gMatParams, uv);
    float depth = texture(gDepth, uv).r;

    vec3 albedo = albSample.rgb;
    vec4 outColor;

    // Shader inputs debug visualization
    if (uboParams.debugViewInputs > 0.0) {
        int index = int(uboParams.debugViewInputs);
        switch (index) {
            case 1:
            outColor.rgba = albSample;
            break;
            case 2:
            outColor.rgba = nrSample;
            break;
            case 3:
            outColor.rgba = mpSample;
            break;
            case 4:
            outColor.rgba = vec4(depth, depth, depth, 1);
            break;
        }
        imageStore(drawImage, pixelCoords, outColor);
        return;
    }
    float alpha = albSample.a;
    vec3 N = normalize(nrSample.xyz * 2.0 - 1.0);
    float roughness = nrSample.w;

    vec3 emissive = mpSample.rgb;
    float metallic = mpSample.a;

    vec3 worldPos = texture(gPosition, uv).rgb;

    vec3 V = normalize(uboParams.camPos - worldPos);
    vec3 L = normalize(-uboParams.lightDir.xyz);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float ambient = 0.01;

    vec3 fragcolor = albedo * ambient;

    float specularStrength = alpha;

    vec3 singularLightColor = vec3(1);
    float singularLightIntensity = 1;
    vec3 diff = singularLightColor * albedo.rgb * NdotL * singularLightIntensity;

    vec3 R = reflect(-L, N);
    float RdotV = max(0.0, dot(R, V));
    float shininess = 16.0;
    vec3 spec = singularLightColor * specularStrength * pow(RdotV, shininess) * singularLightIntensity;

    fragcolor += diff + spec;

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

    fragcolor += ambient_PBR;
    fragcolor += emissive;

    vec3 color = vec3(1.0) - exp(-fragcolor * uboParams.exposure);
    color = pow(color, vec3(1.0 / uboParams.gamma));
    outColor = vec4(color, 1);

    imageStore(drawImage, pixelCoords, outColor);
}
