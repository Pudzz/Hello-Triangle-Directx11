cbuffer constantBuffer
{	
	float4x4 worldViewProjection;	
	float4x4 world;	
};

// * * * * * inputs to vertex shader * * * * * //
struct vShader_input {
	float3 inPosition : POS;
	float3 inWorld : POS;	// Light, using input position coordinates
	float3 inColor : COL;	
	float3 inNormal : NOR;	
	float2 inTexCoord : TEXCOORD;
};

// * * * * * outputs from vertex shader * * * * * //
struct vShader_output {
	float4 outPosition : SV_POSITION;	// required output of VS 
	float3 outWorld : POSITION;	// Light
	float3 outColor : COLOR;	
	float3 outNormal : NORMAL;
	float2 outTexCoord : TEXCOORD;
};

// * * * * * how to handle inputs pos / col / nor / texcoord * * * * * //
vShader_output vs_main(vShader_input input) {
	vShader_output output = (vShader_output)0;	// zero out memory

	output.outPosition = mul(float4(input.inPosition, 1.0f), worldViewProjection);
	output.outWorld = mul(float4(input.inPosition, 1.0f), world);	// Light
	output.outNormal = normalize(mul(float4(input.inNormal, 1.0f), world));
	output.outTexCoord = input.inTexCoord;
	

	return output;
};