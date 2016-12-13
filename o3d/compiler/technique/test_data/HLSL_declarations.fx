// Variable declarations ----------------------------

extern aa;
nointerpolation ab;
shared ac;
static ad;
uniform ae;
volatile af;

static uniform nointerpolation ag;
shared extern const column_major ah;

extern const ai;
shared const aj;
static const ak;
uniform const al;

static row_major float2x2 am;
uniform column_major float3x2 an;



// Geometry Shader ----------------

Buffer<float4> g_Buffer;

float4 geom_fn() {
   float4 bufferdata = g_Buffer.Load(1);
   return bufferdata;
}
