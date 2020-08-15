namespace CGame {
    
class CMedia {
    ShaderHandle @shaderLaser;

    void PrecacheShaders() {
        @shaderLaser = CGame::RegisterShader( "gfx/misc/laser" );
    }
}

}
