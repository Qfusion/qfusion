namespace CGame {

const int ITEM_RESPAWN_TIME = 1000;

void UpdateItemEnt( CEntity @cent ) {
	cent.refEnt.reset();

	@cent.item = GS::FindItemByTag( cent.current.itemNum );
	if( @cent.item is null ) {
		return;
	}

	cent.effects |= cent.item.effects;

	if( cg_simpleItems.boolean && !cent.item.simpleIcon.empty() ) {
		cent.refEnt.rtype = RT_SPRITE;
		@cent.refEnt.model = null;
		@cent.skel = null;
		cent.refEnt.renderfx = RF_NOSHADOW | RF_FULLBRIGHT;
		cent.refEnt.frame = cent.refEnt.oldFrame = 0;
		cent.refEnt.radius = cg_simpleItemsSize.value <= 32 ? cg_simpleItemsSize.value : 32;
		if( cent.refEnt.radius < 1.0f ) {
			cent.refEnt.radius = 1.0f;
		}

		if( cg_simpleItems.integer == 2 ) {
			cent.effects &= ~EF_ROTATE_AND_BOB;
		}

		@cent.refEnt.customShader = CGame::RegisterShader( cent.item.simpleIcon );
	} else {
		cent.refEnt.rtype = RT_MODEL;
		cent.refEnt.frame = cent.current.frame;
		cent.refEnt.oldFrame = cent.prev.frame;

		// set up the model
		@cent.refEnt.model = cgs.modelDraw[cent.current.modelindex];
		@cent.skel = CGame::SkeletonForModel( cent.refEnt.model );
	}
}

void AddItemEnt( CEntity @cent ) {
	int msec;
	Item @item = @cent.item;

	if( @item == null ) {
		return;
	}

	// respawning items
	if( cent.respawnTime != 0 ) {
		msec = cg.time - cent.respawnTime;
	} else {
		msec = ITEM_RESPAWN_TIME;
	}

	if( msec >= 0 && msec < ITEM_RESPAWN_TIME ) {
		cent.refEnt.scale = float( msec ) / ITEM_RESPAWN_TIME;
	} else {
		cent.refEnt.scale = 1.0f;
	}

	if( cent.refEnt.rtype != RT_SPRITE ) {
		// weapons are special
		if( ( item.type & IT_WEAPON ) != 0) {
			cent.refEnt.scale *= 1.40f;
		}

		// Ugly hack for release. Armor models are way too big
		if( ( item.type & IT_ARMOR ) != 0 ) {
			cent.refEnt.scale *= 0.85f;
		}
		if( item.tag == HEALTH_SMALL ) {
			cent.refEnt.scale *= 0.85f;
		}

		// flags are special
		if( ( cent.effects & EF_FLAG_TRAIL ) != 0 ) {
			AddFlagModelOnTag( @cent, cent.refEnt.shaderRGBA, null );
			return;
		}

		AddGenericEnt( @cent );
		return;
	} else {
		if( ( cent.effects & EF_GHOST ) != 0 ) {
			cent.refEnt.shaderRGBA = COLOR_REPLACEA( cent.refEnt.shaderRGBA, 100 );
			cent.refEnt.renderfx |= RF_GREYSCALE;
		}
	}

	// offset the item origin up
	cent.refEnt.origin.z += cent.refEnt.radius + 2;
	cent.refEnt.origin2.z += cent.refEnt.radius + 2;
	if( ( cent.effects & EF_ROTATE_AND_BOB ) != 0 ) {
		EntAddBobEffect( @cent );
	}

	cent.refEnt.axis.identity();
	CGame::Scene::AddEntityToScene( @cent.refEnt );
}

}