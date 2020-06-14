struct OutputVertex
{
    float4 position : SV_Position;
    float3 worldpos : WORLDPOS;
    float3 color : COLOR;
};

float4 main(OutputVertex input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}