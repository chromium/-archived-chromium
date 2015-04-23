float4x4 worldViewProj : WORLDVIEWPROJECTION;
void vs(in float4 pos, out float4 opos) {       
  opos = mul(pos, worldViewProj);
}
float4 fs(): COLOR {
  return float3(0.33f, 0.57f, 0.10f);
}
technique t1 {
  pass p0 {
    VertexShader = compile vs_2_0 vs();
    PixelShader = compile ps_2_0 fs();
  }
}
