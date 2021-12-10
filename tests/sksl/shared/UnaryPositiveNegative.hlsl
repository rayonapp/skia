cbuffer _UniformBuffer : register(b0, space0)
{
    float4 _10_colorWhite : packoffset(c0);
    float4 _10_colorGreen : packoffset(c1);
    float4 _10_colorRed : packoffset(c2);
};


static float4 sk_FragColor;

struct SPIRV_Cross_Output
{
    float4 sk_FragColor : SV_Target0;
};

float4 main(float2 _24)
{
    float2 x = _10_colorWhite.xy;
    x = -x;
    float4 _41 = 0.0f.xxxx;
    if (all(bool2(x.x == (-1.0f).xx.x, x.y == (-1.0f).xx.y)))
    {
        _41 = _10_colorGreen;
    }
    else
    {
        _41 = _10_colorRed;
    }
    return _41;
}

void frag_main()
{
    float2 _20 = 0.0f.xx;
    sk_FragColor = main(_20);
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.sk_FragColor = sk_FragColor;
    return stage_output;
}
