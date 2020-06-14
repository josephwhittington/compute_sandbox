
struct VS_OUT
{
    float4 position : SV_Position;
    float4 world_pos : WORLDPOS;
    float4 color : COLOR;
};

float4 main(VS_OUT input) : SV_TARGET
{
	return input.color;
}