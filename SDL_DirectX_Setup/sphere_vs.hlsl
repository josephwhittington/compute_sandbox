#pragma pack_matrix(row_major)

#define MAX_GEOMETRY_INST 256

struct Input
{
    float3 position : POSITION;
    float3 color : COLOR;
    uint inst_id : SV_InstanceID;
};

struct VS_OUT
{
    float4 position : SV_Position;
    float4 world_pos : WORLDPOS;
    float4 color : COLOR;
};

struct InstData
{
    float4x4 WORLD;
    float4 color;
};

cbuffer MATS : register(b0)
{
    float4x4 WORLD;
    float4x4 VIEW;
    float4x4 PROJ;
}
    
cbuffer INST : register(b1)
{
    InstData INST[MAX_GEOMETRY_INST];
}

VS_OUT main(Input input)
{
    VS_OUT output;
    
    output.position = mul(float4(input.position, 1), INST[input.inst_id].WORLD);
    output.world_pos = output.position;
    output.position = mul(output.position, VIEW);
    output.position = mul(output.position, PROJ);
    
    // Additional props
    output.color = INST[input.inst_id].color;
    
	return output;
}