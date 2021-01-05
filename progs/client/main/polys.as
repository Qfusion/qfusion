namespace CGame {
    
void QuickPolyBeam( const Vec3 &in start, const Vec3 &in end, int width, ShaderHandle @shader ) {
	if( shader is null ) {
		@shader = @cgs.media.shaderLaser;
	}
	CGame::Scene::SpawnPolyBeam( start, end, colorWhite, width, 1, 0, shader, 64, 0 );
}

void ElectroPolyBeam( const Vec3 &in start, const Vec3 &in end, int team ) {
	ShaderHandle @shader = @cgs.media.shaderElectroBeamA;

	if( cg_ebbeam_time.value <= 0.0f || cg_ebbeam_width.integer <= 0 ) {
		return;
	}

	if( cg_teamColoredBeams.boolean && ( team == TEAM_ALPHA || team == TEAM_BETA ) ) {
		if( team == TEAM_ALPHA ) {
			@shader = @cgs.media.shaderElectroBeamAAlpha;
		} else {
			@shader = @cgs.media.shaderElectroBeamABeta;
		}
	}

	CGame::Scene::SpawnPolyBeam( start, end, colorWhite, cg_ebbeam_width.integer, 
		int( cg_ebbeam_time.value * 1000.0f ), int( cg_ebbeam_time.value * 1000.0f * 0.4f ), 
		shader, 128, 0 );
}

void InstaPolyBeam( const Vec3 &in start, const Vec3 &in end, int team ) {
	Vec4 tcolor( 1, 1, 1, 0.35f );

	if( cg_instabeam_time.value <= 0.0f || cg_instabeam_width.integer <= 0 ) {
		return;
	}

	if( cg_teamColoredInstaBeams.boolean && ( team == TEAM_ALPHA || team == TEAM_BETA ) ) {
		tcolor = TeamColor( team );

		float min = 90 * ( 1.0f / 255.0f );
		Vec4  min_team_color(min, min, min, tcolor.w);
		float total = tcolor[0] + tcolor[1] + tcolor[2];

		if( total < min ) {
			tcolor = min_team_color;
		}
	} else {
		tcolor.x = 1.0f;
		tcolor.y = 0.0f;
		tcolor.z = 0.4f;
	}

	tcolor.w = min( cg_instabeam_alpha.value, 1.0f );
	if( tcolor.w == 0.0f ) {
		return;
	}

	CGame::Scene::SpawnPolyBeam( start, end, tcolor, cg_instabeam_width.integer, 
		int( cg_instabeam_time.value * 1000.0f ), int( cg_instabeam_time.value * 1000 * 0.4f ),
		@cgs.media.shaderInstaBeam, 128, 0 );
}

}
