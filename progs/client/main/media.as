namespace CGame {
    
class CMedia {
    ModelHandle @modIlluminatiGib;

    ShaderHandle @shaderLaser;

    void PrecacheShaders() {
        @shaderLaser = CGame::RegisterShader( "gfx/misc/laser" );
    }

    void PrecacheModels() {
        @modIlluminatiGib = CGame::RegisterModel( "models/objects/gibs/illuminati/illuminati1.md3" );
    }
}

}
