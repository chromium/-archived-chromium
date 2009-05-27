float4x4 world : World;
float4x4 worldInverseTranspose : WorldInverseTranspose;
float4x4 worldViewProj : WorldViewProjection;
float4x4 viewInverse : ViewInverse;

////////////////
// tweakables //
////////////////

float BumpHeight <
  string UIName = "Bump Factor";
  float UIMin = 0.0;
  float UIMax = 2.0;
  float UIStep = 0.1;
> = 0.2;

texture normalMap : Normal <
  string UIName = "Normal Map";
  string name = "NMP_Ripples2_512.dds";
  string type = "2D";
>;

sampler2D NormalSampler = sampler_state {
  Texture = <normalMap>;
  MinFilter = Linear;
  MagFilter = Linear;
  MipFilter = Linear;
};

texture envMap : Environment <
  string UIName = "Cube Map";
  string type = "CUBE";
  string name = "sunol_cubemap.dds";
>;

samplerCUBE EnvSampler = sampler_state {
  Texture = <envMap>;
  MinFilter = Linear;
  MipFilter = Linear;
  MagFilter = Linear;
//  WrapS = ClampToEdge;
//  WrapT = ClampToEdge;
};

////////////////
// structures //
////////////////

struct a2v
{
  float4 Position : POSITION; //in object space
  float3 TexCoord : TEXCOORD0;
  float3 Tangent : TANGENT; // TANGENT; // ATTR14; // in object space
  float3 Binormal : BINORMAL; // BINORMAL; // ATTR15; // in object space
  float3 Normal : NORMAL; //in object space
};

struct v2f
{
  float4 Position : POSITION; //in projection space
  float2 TexCoord : TEXCOORD0;
  float3 worldEyeVec : TEXCOORD1;
  float3 WorldNormal : TEXCOORD2;
  float3 WorldTangent : TEXCOORD3;
  float3 WorldBinorm : TEXCOORD4;
};

/////////////
// shaders //
/////////////

v2f BumpReflectVS(a2v IN)
{
  v2f OUT;
  OUT.Position = mul(IN.Position, worldViewProj);
  OUT.TexCoord.xy = IN.TexCoord.xy;
  OUT.WorldNormal = mul(float4(IN.Normal, 0), worldInverseTranspose).xyz;
  OUT.WorldTangent = mul(float4(IN.Tangent, 0), worldInverseTranspose).xyz;
  OUT.WorldBinorm = mul(float4(IN.Binormal, 0), worldInverseTranspose).xyz;
  float3 worldPos = mul(IN.Position, world).xyz;
  OUT.worldEyeVec = normalize(worldPos - viewInverse[3].xyz);
  return OUT;
}

float4 BumpReflectPS(v2f IN) : COLOR {
  float2 bump = (tex2D(NormalSampler, IN.TexCoord.xy).xy * 2 - 1) * BumpHeight;
  float3 normal = normalize(IN.WorldNormal);
  float3 tangent = normalize(IN.WorldTangent);
  float3 binormal = normalize(IN.WorldBinorm);
  float3 nb = normal + bump.x * tangent + bump.y * binormal;
  nb = normalize(nb);
  float3 worldEyeVec = normalize(IN.worldEyeVec);
  float3 lookup = reflect(worldEyeVec, nb);
  return texCUBE(EnvSampler, float4(lookup, 1).xyz);
}

// #o3d VertexShaderEntryPoint BumpReflectVS
// #o3d PixelShaderEntryPoint BumpReflectPS

