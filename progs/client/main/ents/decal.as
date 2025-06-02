namespace CGame {
    void LerpDecalEnt(CEntity@ cent) {
        // interpolate origin
        for (int i = 0; i < 3; i++) {
            cent.refEnt.origin[i] = cent.prev.origin[i] + cg.lerpfrac * (cent.current.origin[i] - cent.prev.origin[i]);
        }

        cent.refEnt.radius = cent.prev.frame + cg.lerpfrac * (cent.current.frame - cent.prev.frame);

        float a1 = float(cent.prev.modelindex2) / 255.0f * 360.0f;
        float a2 = float(cent.current.modelindex2) / 255.0f * 360.0f;
        cent.refEnt.rotation = LerpAngle(a1, a2, cg.lerpfrac);
    }

    void UpdateDecalEnt(CEntity@ cent) {
        cent.refEnt.shaderRGBA = ColorForEntity(@cent, false);
        // set up the null model, may be potentially needed for linked model
        @cent.refEnt.model = null;
        @cent.refEnt.customShader = @cgs.imagePrecache[cent.current.modelindex];
        cent.refEnt.radius = cent.prev.frame;
        cent.refEnt.rotation = float(cent.prev.modelindex2) / 255.0f * 360.0f;
        cent.refEnt.origin = cent.prev.origin;
        cent.refEnt.origin2 = cent.prev.origin2;
    }

    void AddDecalEnt(CEntity@ cent) {
        // if set to invisible, skip
        if (cent.current.modelindex == 0) {
            return;
        }

        if ((cent.effects & EF_TEAMCOLOR_TRANSITION) != 0) {
            EntAddTeamColorTransitionEffect( @cent );
        }

        Vec4 color = ColorToVec4(cent.refEnt.shaderRGBA);

        CGame::Scene::AddFragmentedDecalToScene(
            cent.refEnt.origin,
            cent.refEnt.origin2,
            cent.refEnt.rotation,
            cent.refEnt.radius,
            color[0], color[1], color[2], color[3],
            cent.refEnt.customShader);
    }
}
