namespace CGame {
    
class CMedia {
    ModelHandle @modIlluminatiGib;
    ModelHandle @modFlag;

    ShaderHandle @shaderLaser;
    ShaderHandle @shaderFlagFlare;

    void PrecacheShaders() {
        @shaderLaser = CGame::RegisterShader( "gfx/misc/laser" );
        @shaderFlagFlare = CGame::RegisterShader( PATH_FLAG_FLARE_SHADER );
    }

    void PrecacheModels() {
        @modIlluminatiGib = CGame::RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
        @modFlag = CGame::RegisterModel( PATH_FLAG_MODEL );
    }
}

}
