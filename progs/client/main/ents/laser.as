namespace CGame {
    void LerpLaserbeamEnt(CEntity@ cent) {
        auto @cam = @Camera::GetMainCamera();
        // Find the owner entity
        auto @owner = @cgEnts[cent.current.ownerNum];

        // If predicting and this is the viewer entity, skip
        if (cam.playerPrediction && cg_predictLaserBeam.boolean
            && IsViewerEntity(cent.current.ownerNum)) {
            return;
        }

        owner.localEffects[LEF_LASERBEAM] = cg.time + 1;
        owner.laserCurved = (cent.current.type == ET_CURVELASERBEAM);
    }

    void UpdateLaserbeamEnt(CEntity@ cent) {
        auto @cam = @Camera::GetMainCamera();

        // If predicting and this is the viewer entity, skip
        if (cam.playerPrediction && cg_predictLaserBeam.boolean
            && IsViewerEntity(cent.current.ownerNum)) {
            return;
        }

        auto owner = @cgEnts[cent.current.ownerNum];
        if (owner.serverFrame != Snap.serverFrame) {
            Error("CG_UpdateLaserbeamEnt: owner is not in the snapshot");
        }

        owner.localEffects[LEF_LASERBEAM] = cg.time + 10;
        owner.laserCurved = (cent.current.type == ET_CURVELASERBEAM);

        // laser->s.origin is beam start
        // laser->s.origin2 is beam end

        owner.laserOriginOld = cent.prev.origin;
        owner.laserPointOld = cent.prev.origin2;

        owner.laserOrigin = cent.current.origin;
        owner.laserPoint = cent.current.origin2;
    }
}
