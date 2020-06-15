#pragma pack_matrix(row_major)

// Defines
#define STRUCTURE_SIZE 80
#define FLOAT_SIZE 4
#define COLOR_SIZE 16
#define MATRIX_SIZE 64 
#define BYTE_OFFSET_TO_Y_POSITION 52

cbuffer MATS : register(b0)
{
    float4x4 W; float4x4 V; float4x4 P;
    float3 campos; float time;
}

// Raw buffer
RWByteAddressBuffer b : register(u0);

[numthreads(16, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    // X will be the index
    // Extract the y component of the transform
    uint byte_offset_to_y_pos =  DTid.x * STRUCTURE_SIZE + BYTE_OFFSET_TO_Y_POSITION;
    
    float xpos = asfloat(b.Load(byte_offset_to_y_pos - 4));
    float ypos = asfloat(b.Load(byte_offset_to_y_pos));
    float zpos = asfloat(b.Load(byte_offset_to_y_pos + 4));
    
    float f = fmod(xpos, ypos);
    float f1 = fmod(ypos, zpos);
    
    ypos += sin(time * f) * .1;
    xpos += cos(time * f1) * .1;
    
    b.Store(byte_offset_to_y_pos, asuint(ypos));
    b.Store(byte_offset_to_y_pos - 4, asuint(xpos));
}