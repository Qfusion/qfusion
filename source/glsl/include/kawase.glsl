///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Developed by Masaki Kawase, Bunkasha Games
// Used in DOUBLE-S.T.E.A.L. (aka Wreckless)
// From his GDC2003 Presentation: Frame Buffer Postprocessing Effects in  DOUBLE-S.T.E.A.L (Wreckless)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
vec3 KawaseBlurFilter( sampler2D tex, vec2 texCoord, vec2 pixelSize, float iteration )
{
    vec2 texCoordSample;
    vec2 halfPixelSize = pixelSize / 2.0;
    vec2 dUV = ( pixelSize.xy * vec2( iteration, iteration ) ) + halfPixelSize.xy;

    vec3 cOut;

    // Sample top left pixel
    texCoordSample.x = texCoord.x - dUV.x;
    texCoordSample.y = texCoord.y + dUV.y;
    
    cOut = qf_texture( tex, texCoordSample ).xyz;

    // Sample top right pixel
    texCoordSample.x = texCoord.x + dUV.x;
    texCoordSample.y = texCoord.y + dUV.y;

    cOut += qf_texture( tex, texCoordSample ).xyz;

    // Sample bottom right pixel
    texCoordSample.x = texCoord.x + dUV.x;
    texCoordSample.y = texCoord.y - dUV.y;
    cOut += qf_texture( tex, texCoordSample ).xyz;

    // Sample bottom left pixel
    texCoordSample.x = texCoord.x - dUV.x;
    texCoordSample.y = texCoord.y - dUV.y;

    cOut += qf_texture( tex, texCoordSample ).xyz;

    // Average 
    cOut *= 0.25f;
    
    return cOut;
}
