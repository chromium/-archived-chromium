// texture
texture Tex0 : DiffuseMap < 
	string name = "tiger.bmp"; 
	string UIName = "Base Texture";
	>;
	
// transformations
float4x4 worldViewProj : 	WORLDVIEWPROJECTION;

struct VS_OUTPUT
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
};

VS_OUTPUT VS(VS_OUTPUT IN) {
    VS_OUTPUT Out = (VS_OUTPUT)0;
    Out.position  = mul(IN.position, worldViewProj);
    Out.texcoord = IN.texcoord;
    return Out;
}

sampler Sampler = sampler_state
{
    Texture   = (Tex0);
    MinFilter = Linear;
    MagFilter = Point;
    MipFilter = None;
    AddressU = Mirror;
    AddressV = Wrap;
    AddressW = Clamp;
    MaxAnisotropy = 16;
    BorderColor = float4(1.0, 0.0, 0.0, 1.0);
};


float4 PS(VS_OUTPUT IN) : COLOR
{
    float4 color = tex2D(Sampler, IN.texcoord);
    return  color ;
}


technique DefaultTechnique
{
    pass P0
    {
        // shaders
        CullMode = None;
       	VertexShader = compile vs_2_0 VS();
        PixelShader  = compile ps_2_0 PS();
    }  
}

