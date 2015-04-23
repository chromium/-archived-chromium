/*****************************************************************************/
/*                                                                           */
/* File: fur.fx                                                              */
/*                                                                           */
/*****************************************************************************/

// fur.fx

//---------------------------------------------------------------------------//

int shellcount = 20;
int shellnumber = 0;

float FurScale = 0;
float FurLength = 0;
float UVScale = 0;


texture FurTexture
< 
    string TextureType = "2D";
>;


// transformations
float4x4 worldViewProj : WORLDVIEWPROJ;

//------------------------------------
struct vertexInput {
    float3 position				: POSITION;
    float3 normal				: NORMAL;
    //float4 color                : COLOR0;
    float4 texCoordDiffuse		: TEXCOORD0;
    float4 tex1					: TEXCOORD1;
};

struct vertexOutput {
    float4 HPOS		: POSITION;    
    //float4 color    : COLOR;
    float4 T0	    : TEXCOORD0;
    float3 normal   : TEXCOORD1;

};


//------------------------------------
vertexOutput VS_TransformAndTexture(vertexInput IN) 
{
    vertexOutput OUT;

	//float3 P = IN.position.xyz;
	//float3 P = IN.position.xyz + (IN.normal * (FurDistance * (float)shellnumber));
	
	//float3 P = IN.position.xyz + (IN.normal * FurScale * 10.0f) + (IN.normal*FurLength);
	//float3 P = IN.position.xyz + (IN.normal * FurScale * 10.0f) + (IN.normal*FurLength);
	
	float3 P = IN.position.xyz + (IN.normal * FurLength);
	
	//OUT.T0 = IN.texCoordDiffuse * 2.0f;
	//OUT.T0 = IN.tex1 * FurScale;
	OUT.T0 = IN.texCoordDiffuse * UVScale;
	//OUT.T0 = IN.tex1 / 3.0f;
	OUT.HPOS = mul(float4(P, 1.0f), worldViewProj);
    //OUT.color = IN.color;
    OUT.normal = IN.normal;
    return OUT;
}


//------------------------------------
sampler TextureSampler = sampler_state 
{
    texture = <FurTexture>;
    AddressU  = WRAP;        
    AddressV  = WRAP;
    AddressW  = WRAP;
    MIPFILTER = LINEAR;
    MINFILTER = LINEAR;
    MAGFILTER = LINEAR;
};


//-----------------------------------
float4 PS_Textured( vertexOutput IN): COLOR
{
	float4 diffuseTexture = tex2D( TextureSampler, IN.T0 );
	//float4 diffuseTexture2 = tex2D( TextureSampler2, IN.T0 );
  
	//return (float4(furColor.xyz, FurStrength) * diffuseTexture);
	return diffuseTexture;
	//return float4(0.0f, 1.0f, 1.0f, 0.3f); //rrggbbaa
}

//-----------------------------------

technique Fur	        
{
    pass Shell
    <
    	string script="Draw=Geometry;";
    >
    {		
		VertexShader = compile vs_1_1 VS_TransformAndTexture();
		PixelShader  = compile ps_1_3 PS_Textured();
		AlphaBlendEnable = true;
		SrcBlend = srcalpha;
		//DestBlend = one;
		DestBlend = invsrcalpha;
		//DestBlend = srccolor;
		//DestBlend = invsrccolor;
		//DestBlend = srcalpha;
		//DestBlend = destalpha;
		//DestBlend = invdestalpha;
		//DestBlend = destcolor;
		//DestBlend = invdestcolor;    
		
		//CullMode = None;
		//CullMode = CCW;
    }
}

