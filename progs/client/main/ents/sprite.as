namespace CGame {

void AddSpriteEnt( CEntity @cent ) {
	if( cent.refEnt.scale == 0.0f )
		return;

	// if set to invisible, skip
	if( cent.current.modelindex == 0 )
		return;

	// bobbing & auto-rotation
	if( ( cent.effects & EF_ROTATE_AND_BOB ) != 0 )
		EntAddBobEffect( @cent );

	if( ( cent.effects & EF_TEAMCOLOR_TRANSITION ) != 0 )
		EntAddTeamColorTransitionEffect( @cent );

	// render effects
	cent.refEnt.renderfx = cent.renderfx;

	// add to refresh list
	CGame::Scene::AddEntityToScene( @cent.refEnt );

	if( cent.current.modelindex2 != 0 )
		AddLinkedModel( @cent );
}

void LerpSpriteEnt( CEntity @cent )
{
	int i;

	// interpolate origin
    Vec3 origin = cent.prev.origin + cg.lerpfrac * (cent.current.origin - cent.prev.origin);
    cent.refEnt.origin = origin;
    cent.refEnt.origin2 = origin;
    cent.refEnt.lightingOrigin = origin;
	cent.refEnt.radius = float( cent.prev.frame ) + cg.lerpfrac * float( cent.current.frame - cent.prev.frame );
}

void UpdateSpriteEnt( CEntity @cent )
{
    cent.refEnt.reset();
	cent.refEnt.renderfx = cent.renderfx;

	// set entity color based on team
	cent.refEnt.shaderRGBA = TeamColorForEntity( cent.current.number );

	// set up the sprite
	cent.refEnt.rtype = RT_SPRITE;
	@cent.refEnt.customShader = @cgs.imagePrecache[ cent.current.modelindex ];
	cent.refEnt.radius = cent.prev.frame;
    cent.refEnt.origin = cent.prev.origin;
    cent.refEnt.origin2 = cent.prev.origin;
    cent.refEnt.lightingOrigin = cent.prev.origin;
}

}