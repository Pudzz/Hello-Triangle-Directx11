struct Light
{
	float3 ambientLightColor;
	float ambientLightStrength;

	float3 dynamicLightColor;
	float dynamicLightStrength;

	float3 dynamicLightPosition;
	float3 dynamicAttenuation;	
};

cbuffer cBufferLight
{
	Light light;
};

// :::::::: inputs to pixel shader :::::::: //
struct pShader_input {
	float4 inPosition : SV_POSITION;
	float3 inWorldPos : POSITION;	// for lightning
	float3 inColor : COL;
	float3 inNormal : NOR;	
	float2 inTexCoord : TEXCOORD; // For texture
};

Texture2D objTexture : TEXTURE: register(t0);
SamplerState objSamplerState : SAMPLER: register(s0);

// :::::::: how to handle inputs :::::::: //	Return float4 pixelcolor
float4 ps_main(pShader_input input) : SV_TARGET
{
	// color from texture
	float3 sampleColor = objTexture.Sample(objSamplerState, input.inTexCoord);

	// Ambient brightness and color setup
	float3 ambientLight = light.ambientLightColor * light.ambientLightStrength;
	float3 appliedFinalLight = ambientLight;

	// Get normalized vector from pixel to light
	float3 vecToLight = normalize(light.dynamicLightPosition - input.inWorldPos);

	// Det dot-product to se how intense light is, angle between vectors, 
	float3 diffuseLightIntensity = max(dot(input.inNormal, vecToLight),0); // "max" makes sure that the intensity-value isn't gonna be less than 0.0f

	// * * * Attenuation * * * //
	// Calculate lightIntensity with attentuation
	float distanceVecToLight = distance(light.dynamicLightPosition, input.inWorldPos);	// distance, not normalized
	// Get factor from equation
	float attenuationFactor = 1 / (light.dynamicAttenuation[0]) + (light.dynamicAttenuation[1] * distanceVecToLight) + (light.dynamicAttenuation[2] * (distanceVecToLight * distanceVecToLight));

	diffuseLightIntensity *= attenuationFactor;
	// * * *    * * *    * * * //

	float3 diffuseLight = diffuseLightIntensity * light.dynamicLightStrength * light.dynamicLightColor;

	// ambient light + colorlight/brighness/falloff factor
	appliedFinalLight += diffuseLight;

	// Final color pixel = texturecolor * ambientlight
	float3 finalcolor = sampleColor * appliedFinalLight;

	return float4(finalcolor, 1.0f);



};