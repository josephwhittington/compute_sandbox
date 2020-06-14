#pragma pack_matrix(row_major)

#define MAX_INSTANCES 128

struct InputVertex
{
    float3 position : POSITION;
    float3 color : COLOR;
    uint inst_id : SV_InstanceID;
};

struct OutputVertex
{
    float4 position : SV_Position;
    float3 worldpos : WORLDPOS;
    float3 color : COLOR;
};

cbuffer WORLD : register(b0)
{
    float4x4 worldmat;
    float4x4 viewmat;
    float4x4 projectionMat;
};

cbuffer MATS : register(b1)
{
    float4x4 inst_positions[MAX_INSTANCES];
}

OutputVertex main(InputVertex input)
{
    OutputVertex output;
    
    output.color = input.color;

    output.position = float4(input.position, 1);
    output.position = mul(output.position, inst_positions[input.inst_id]);
    output.worldpos = output.position.xyz;
    output.position = mul(output.position, viewmat);
    output.position = mul(output.position, projectionMat);
    
    return output;
}