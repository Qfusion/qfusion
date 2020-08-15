namespace CGame {
    
void QuickPolyBeam( const Vec3 &in start, const Vec3 &in end, int width, ShaderHandle @shader ) {
	if( shader is null ) {
		@shader = @cgs.media.shaderLaser;
	}
	CGame::Scene::SpawnPolyBeam( start, end, colorWhite, width, 1, 0, shader, 64, 0 );
}

}
