namespace CGame {
    
class PModel {
	//static data
	PlayerModel @pmodelinfo;
	SkinHandle @skin;

	//dynamic
	GS::Anim::PModelAnimState animState;

	array<Vec3> angles(GS::Anim::PMODEL_PARTS);                // for rotations
	array<Vec3> oldangles(GS::Anim::PMODEL_PARTS);             // for rotations

	//effects
	CGame::Scene::Orientation projectionSource;     // for projectiles
	// weapon. Not sure about keeping it here
	int64 flashTime;
	int64 barrelTime;
} 

array<PModel> entPModels(MAX_EDICTS);      //a pmodel handle for each cg_entity

}
