// Shader from NVidia example documentation:
// ftp://download.nvidia.com/developer/SDK/Individual_Samples/
//       MEDIA/docPix/docs/FX_Shadowing.pdf

texture CTex : RENDERCOLORTARGET <
  float2 Dimensions = {256,256};
  string Format = "x8b8g8r8" ;
  string UIWidget = "None";
>;

sampler CSamp = sampler_state {
  texture = <CTex>;
  AddressU = CLAMP; AddressV = CLAMP;
  MipFilter = NONE;
  MinFilter = LINEAR;
  MagFilter = LINEAR;
};

texture DTex : RENDERDEPTHSTENCILTARGET <
  float2 Dimensions = {256,256};
  string format = "D24S8_SHADOWMAP";
  string UIWidget = "None";
>;

sampler DSamp = sampler_state {
  texture = <DTex>;
  AddressU = CLAMP; AddressV = CLAMP;
  MipFilter = NONE;
  MinFilter = LINEAR;
  MagFilter = LINEAR;
};

float Script : STANDARDSGLOBAL <
  string UIWidget = "none";
  string ScriptClass = "sceneorobject";
  string ScriptOrder = "standard";
  string ScriptOutput = "color";
  string Script = "Tech=Technique?Shadowed:Unshadowed;";
> = 0.8; // version #

// The following global variables are values to be used
// when clearing color and/or depth buffers
float4 ClearColor <
  string UIWidget = "color";
  string UIName = "background";
> = {0,0,0,0.0};

float ClearDepth <
  string UIWidget = "none";
> = 1.0;

float4 ShadowClearColor <
  string UIWidget = "none";
> = {1,1,1,0.0};

void lightingCalc(ShadowingVertexOutput IN,
                  out float3 litContrib,
                  out float3 ambiContrib) {
  float3 Nn = normalize(IN.WNormal);
  float3 Vn = normalize(IN.WView);
  Nn = faceforward(Nn,-Vn,Nn);
  float falloff = 1.0 / dot(IN.LightVec,IN.LightVec);
  float3 Ln = normalize(IN.LightVec);
  float3 Hn = normalize(Vn + Ln);
  float hdn = dot(Hn,Nn);
  float ldn = dot(Ln,Nn);
  float4 litVec = lit(ldn,hdn,SpecExpon);
  ldn = litVec.y * SpotLightIntensity;
  ambiContrib = SurfColor * AmbiLightColor;
  float3 diffContrib = SurfColor*(Kd * ldn * SpotLightColor);
  float3 specContrib = ((ldn * litVec.z * Ks) * SpotLightColor);
  float3 result = diffContrib + specContrib;
  float cone = tex2Dproj(SpotSamp,IN.LProj);
  litContrib = ((cone*falloff) * result);
}

float4 useShadowPS(ShadowingVertexOutput IN) : COLOR {
  float3 litPart, ambiPart;
  lightingCalc(IN,litPart,ambiPart);
  float4 shadowed = tex2Dproj(ShadDepthSampler,IN.LProj);
  return float4((shadowed.x*litPart)+ambiPart,1);
}

float4 unshadowedPS(ShadowingVertexOutput IN) : COLOR {
  float3 litPart, ambiPart;
  lightingCalc(IN,litPart,ambiPart);
}

technique Unshadowed <
  string Script = "Pass=NoShadow;";
> {
  pass NoShadow <
      string Script =
      "RenderColorTarget0=;"
      "RenderDepthStencilTarget=;"
      "RenderPort=;"
      "ClearSetColor=ClearColor;"
      "ClearSetDepth=ClearDepth;"
      "ClearDepth=Color;"
      "Clear=Depth;"
      "Draw=geometry;";
  > {
    VertexShader = compile vs_2_0
        shadowUseVS(WorldXf,
                    WorldITXf,
                    WorldViewProjXf,
                    ShadowViewProjXf,
                    ViewIXf,
                    ShadBiasXf,
                    SpotLightPos);
    ZEnable = true;
    ZWriteEnable = true;
    ZFunc = LessEqual;
    CullMode = None;
    PixelShader = compile ps_2_a unshadowedPS();
  }
}

technique Shadowed <
  string Script = "Pass=MakeShadow;"
                  "Pass=UseShadow;";
> {
  pass MakeShadow <
      string Script =
        "RenderColorTarget0=ColorShadMap;"
        "RenderDepthStencilTarget=ShadDepthTarget;"
        "RenderPort=light0;"
        "ClearSetColor=ShadowClearColor;"
        "ClearSetDepth=ClearDepth;"
        "Clear=Color;"
        "Clear=Depth;"
        "Draw=geometry;";
  > {
    VertexShader = compile vs_2_0
        shadowGenVS(WorldXf,
                    WorldITXf,
                    ShadowViewProjXf);
    ZEnable = true;
    ZWriteEnable = true;
    ZFunc = LessEqual;
    CullMode = None;
    // no pixel shader needed!
  }

  pass UseShadow <
      string Script =
        "RenderColorTarget0=;"
        "RenderDepthStencilTarget=;"
        " RenderPort=;"
        "ClearSetColor=ClearColor;"
        "ClearSetDepth=ClearDepth;"
        "Clear=Color;"
        "Clear=Depth;"
        "Draw=geometry;";
  > {
    VertexShader = compile vs_2_0
        shadowUseVS(WorldXf,
                    WorldITXf,
                    WorldViewProjXf,
                    ShadowViewProjXf,
                    ViewIXf,
                    ShadBiasXf,
                    SpotLightPos);
    ZEnable = true;
    ZWriteEnable = true;
    ZFunc = LessEqual;
    CullMode = None;
    PixelShader = compile ps_2_a useShadowPS();
  }
}
