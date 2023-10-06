struct PSInput
{
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float4 Normal : NORMAL;
    [[vk::location(1)]] float4 world_pos : AAA;
    [[vk::location(2)]] float2 TexCoord : TEXCOORD0;
};

struct PSOutput
{
    [[vk::location(0)]] float4 Color : SV_Target0;
};

struct PushConst
{
    [[vk::offset(64)]]float4 model_color;
};

struct LightSource
{
    float4 source_location;
    float4 source_color;
    float4 view_pos;
};

[[vk::push_constant]] PushConst push_data;

ConstantBuffer<LightSource> ubo_data : register(b0, space1);

PSOutput main(PSInput input)
{
    PSOutput output;

    input.Normal = normalize(input.Normal);

    float4 light = ubo_data.source_location - input.world_pos;

    //This is inverse of required f(d)
    float atten_fac = length(light) * 0.0005f;
    atten_fac = 1.0f / (atten_fac*atten_fac  + atten_fac + 1);

    light = normalize(light);

    float4 ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);



    float4 view = normalize(ubo_data.view_pos-input.world_pos);

    float diff_fac = dot(input.Normal, light);

    float4 half_vec = normalize(light + view);
    float spec_fac = pow(clamp(dot(input.Normal,half_vec),0.f,1.f), 50);

    output.Color = ambient +atten_fac*
                max(0.f,
                    min(1.f,diff_fac )) *
                ubo_data.source_color;

    //printf("%f %f %f %f \n",half_vec.x ,half_vec.y, half_vec.z, half_vec.w);
    //printf("%f\t",spec_fac);

    output.Color = output.Color * push_data.model_color+
    atten_fac*spec_fac*ubo_data.source_color;

    output.Color.w = 1.0f;

    return output;
}