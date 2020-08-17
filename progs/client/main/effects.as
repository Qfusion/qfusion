namespace CGame {

void AddRaceGhostShell( CGame::Scene::Entity @ent ) {
	CGame::Scene::Entity shell;
	float alpha = cg_raceGhostsAlpha.value;

	alpha = bound( alpha, 0.0f, 1.0f );

	shell = ent;
	@shell.customSkin = null;

	if( ( shell.renderfx & RF_WEAPONMODEL ) != 0 ) {
		return;
	}

	@shell.customShader = cgs.media.shaderRaceGhostEffect;
	shell.renderfx |= ( RF_FULLBRIGHT | RF_NOSHADOW );

	Vec4 c = ColorToVec4( shell.shaderRGBA );
	shell.shaderRGBA = Vec4ToColor( Vec4( c[0]*alpha, c[1]*alpha, c[2]*alpha, alpha ) );

	CGame::Scene::AddEntityToScene( @shell );
}

void AddShellEffects( CGame::Scene::Entity @ent, int effects ) {
	if( ( effects & EF_RACEGHOST ) != 0 ) {
		AddRaceGhostShell( @ent );
	}
}

}
