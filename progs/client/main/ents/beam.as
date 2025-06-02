namespace CGame {
    void AddBeamEnt( CEntity @cent ) {
        // wsw : jalfixme: missing the color (comes inside cent->current.colorRGBA)
	    QuickPolyBeam( cent.current.origin, cent.current.origin2, 
            int ( float( cent.current.frame ) * 0.5f ), @cgs.media.shaderLaser );
    }
}
