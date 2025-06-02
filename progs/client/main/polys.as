namespace CGame {
    
void QuickPolyBeam( const Vec3 &in start, const Vec3 &in end, int width, ShaderHandle @shader ) {
	if( shader is null ) {
		@shader = @cgs.media.shaderLaser;
	}
	Scene::SpawnPolyBeam( start, end, colorWhite, width, 1, 0, shader, 64, 0 );
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

	Scene::SpawnPolyBeam( start, end, colorWhite, cg_ebbeam_width.integer, 
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

	Scene::SpawnPolyBeam( start, end, tcolor, cg_instabeam_width.integer, 
		int( cg_instabeam_time.value * 1000.0f ), int( cg_instabeam_time.value * 1000 * 0.4f ),
		@cgs.media.shaderInstaBeam, 128, 0 );
}

void LaserGunPolyBeam(const Vec3 start, const Vec3 end, const Vec4 color, int tag) {
    Vec4 tcolor(0, 0, 0, 0.35f);

	// dmh: if teamcolor is too dark, set color to default
    if (color.w > 0.0f) {
        tcolor = color;
        float min = 90.0f / 255.0f;
        float total = tcolor.x + tcolor.y + tcolor.z;
        if (total < min) {
            tcolor.x = tcolor.y = tcolor.z = min;
        }
    }

    Scene::SpawnPolyBeam(start, end, (color.w > 0.0f) ? tcolor : Vec4(0,0,0,0), 12, 1, 0, cgs.media.shaderLaserGunBeam, 64, tag);
}

/*
* ElectroPolyboardBeam
*
* Spawns a segmented lightning beam, pseudo-randomly disturbing each segment's 
* placement along the given line.
*
* For more information please refer to
* Mathematics for 3D Game Programming and Computer Graphics, section "Polyboards"
*/
void ElectroPolyboardBeam(const Vec3 start, const Vec3 end, int subdivisions, float phase, float range, const Vec4 color, int key, bool firstPerson) {
    Vec4 tcolor(0, 0, 0, 0.35f);

    if (color.w > 0.0f) {
        tcolor = color;
        float min = 90.0f / 255.0f;
        float total = tcolor.x + tcolor.y + tcolor.z;
        if (total < min) {
            tcolor.x = tcolor.y = tcolor.z = min;
        }
    }

    Vec3 from = start;
    Vec3 dir = end - start;
    float dist = dir.length();
    dir.normalize();

    int minSub = int(GS::Weapons::CURVELASERBEAM_SUBDIVISIONS - 10);
    int maxSub = int(GS::Weapons::CURVELASERBEAM_SUBDIVISIONS + 10);
	if (subdivisions < minSub) {
		subdivisions = minSub;
	} else if (subdivisions > maxSub) {
		subdivisions = maxSub;
	}
    int segments = int(subdivisions * ((dist + 500.0f) / range));
    const float frequency = 0.1244f;
    auto shader = cgs.media.shaderLaserGunBeam;

    Vec3 to, c, d, e, f, g, h;

    for (int i = 0; i <= segments; i++) {
        float frac = range * float(i) / float(segments);
        float amplitude = GetNoise(0, 0, 0, float(cg.realTime) + phase) * sqrt(float(i) / float(segments));
        float width;
        bool last = false;

        if (firstPerson) {
            width = (phase + 1) * 4.0f * float(i);
			if (i > 1)
	            amplitude *= (phase + 1) * (phase + 1);
			else
				amplitude = 0;
        } else {
            width = 12.0f;
            amplitude *= (phase + 1);
        }

        if (frac >= dist + width * 2.0f) {
            last = true;
            width = frac - dist - width * 2.0f;
        }

        float x = float(i) * (phase + 1) * 400.0f / float(segments);
        x *= frequency;
        float t = 0.01f * (-float(cg.time) * 0.01f * 130.0f);

        float y = sin(x);
        y += sin(x * 2.1f + t * 2.0f) * 4.5f;
        y += sin(x * 1.72f + t * 6.121f) * 4.0f;
        y += sin(x * 2.221f + t * 10.437f) * 5.0f;
        y += sin(x * 3.1122f + t * 8.269f) * 2.5f;
        y *= amplitude;

        to = start + dir * frac;

        d = Camera::GetMainCamera().origin - from;
        d.normalize();

        c = dir ^ d;

        e = from + c * width;
        f = from - c * width;

        e += c * y;
        f += c * y;

        if (i > 0) {
            Scene::SpawnPolyQuad(e, f, h, g, 1, 1, (color.w > 0.0f) ? tcolor : Vec4(0,0,0,0), 1, 0, shader, key);
        }

        from = to;
        g = e;
        h = f;

        if (last) {
            break;
        }
    }
}

}
