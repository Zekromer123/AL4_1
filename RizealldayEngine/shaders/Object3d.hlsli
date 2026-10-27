struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : TEXCOORD1;
    float32_t3 localPosition : TEXCOORD2; // model space (used for ink edges on boxes)
};
