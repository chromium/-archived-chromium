struct a2v {
  float4 pos : POSITION;
  float3 normal : NORMAL;
};

struct v2f {
  float4 pos : POSITION;
  float3 n : TEXCOORD0;
  float3 l : TEXCOORD1;
};

float4x4 worldViewProj : WorldViewProjection;
float4x4 world : World;
float4x4 worldIT : WorldInverseTranspose;
float3 lightWorldPos;
float4 lightColor;

v2f vsMain(a2v IN) {
  v2f OUT;
  OUT.pos = mul(IN.pos, worldViewProj);
  OUT.n = mul(float4(IN.normal,0), worldIT).xyz;
  OUT.l = lightWorldPos-mul(IN.pos, world).xyz;
  return OUT;
}

float4 fsMain(v2f IN): COLOR {
  float3 l=normalize(IN.l);
  float3 n=normalize(IN.n);
  float4 litR=lit(dot(n,l),0,0);
  return emissive+lightColor*(ambient+diffuse*litR.y);
}

technique {
  pass p0 {
    VertexShader = compile vs_2_0 vsMain();
    PixelShader = compile ps_2_0 fsMain();
  }
}
