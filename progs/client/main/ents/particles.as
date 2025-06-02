namespace CGame {

void AddParticlesEnt(CEntity@ cent) {
    // origin = origin
    // angles = angles
    // sound = sound
    // light = light color
    // frame = speed
    // team = RGBA
    // modelindex = shader
    // modelindex2 = radius (spread)
    // effects & 0xFF = size
    // skinNum/counterNum = time (fade in seconds);
    // effects = spherical, bounce, gravity,
    // weapon = frequency

    Vec3 dir, accel, angles;
    float speed;
    int spriteTime;
    int spriteRadius;
    int mintime;
    int bounce = 0;
    bool expandEffect = false;
    bool shrinkEffect = false;

    // duration of each particle
    spriteTime = cent.current.counterNum;
    if (spriteTime == 0) {
        return;
    }

    spriteRadius = cent.current.effects & 0xFF;
    if (spriteRadius == 0) {
        return;
    }

    if (cent.current.weapon == 0) { // weapon is count per second
        return;
    }

    mintime = 1000 / cent.current.weapon;

    if (cent.localEffects[LEF_ROCKETTRAIL_LAST_DROP] + mintime > cg.time) {
        return;
    }

    cent.localEffects[LEF_ROCKETTRAIL_LAST_DROP] = cg.time;

    speed = cent.current.frame;

    if (((cent.current.effects >> 8) & 1) != 0) { // SPHERICAL DROP
        angles.x = float(brandom(0, 360));
        angles.y = float(brandom(0, 360));
        angles.z = float(brandom(0, 360));

        dir = angles.anglesToForward();
        dir.normalize();
        dir *= speed;
    } else {   // DIRECTIONAL DROP
        float r, u;
        double alpha;
        double s;
        int seed = cg.time % 255;
        Rand rnd(seed);
        int spread = uint(cent.current.modelindex2) * 25;

        // interpolate dropping angles
        for (int i = 0; i < 3; i++)
            angles[i] = LerpAngle(cent.prev.angles[i], cent.current.angles[i], cg.lerpfrac);

        Mat3 axis;
        angles.anglesToAxis(axis);

        alpha = M_PI * rnd.Cdouble(); // [-PI ..+PI]
        s = abs(rnd.Cdouble()); // [0..1]
        r = float(s * cos(alpha) * spread);
        u = float(s * sin(alpha) * spread);

        // apply spread on the direction
        dir = axis.z * 1024.0f;
        dir += axis.x * r;
        dir += axis.y * u;

        dir.normalize();
        dir *= speed;
    }

    // interpolate origin
    for (int i = 0; i < 3; i++)
        cent.refEnt.origin[i] = cent.refEnt.origin2[i] = cent.prev.origin[i] + cg.lerpfrac * (cent.current.origin[i] - cent.prev.origin[i]);

    if (((cent.current.effects >> 9) & 1) != 0) { // BOUNCES ON WALLS/FLOORS
        bounce = 35;
    }

    accel = Vec3(0, 0, 0);
    if (((cent.current.effects >> 10) & 1) != 0) { // GRAVITY
        accel = Vec3(-0.2f, -0.2f, -175.0f);
    }

    if (((cent.current.effects >> 11) & 1) != 0) { // EXPAND_EFFECT
        expandEffect = true;
    }

    if (((cent.current.effects >> 12) & 1) != 0) { // SHRINK_EFFECT
        shrinkEffect = true;
    }

    Vec4 color = ColorToVec4(cent.refEnt.shaderRGBA);
    Vec4 light = ColorToVec4(cent.current.light);

    LE::SpawnSprite(
        cent.refEnt.origin, dir, accel,
        spriteRadius, spriteTime, bounce, expandEffect, shrinkEffect,
        color[0], color[1], color[2], color[3],
        cent.current.light != 0 ? spriteRadius * 4 : 0, // light radius
        light[0], light[1], light[2],
        cent.refEnt.customShader
    );
}

void UpdateParticlesEnt(CEntity@ cent) {
    // set entity color based on team
    cent.refEnt.shaderRGBA = ColorForEntity(@cent, false);
    // set up the data in the old position
    @cent.refEnt.model = null;
    @cent.refEnt.customShader = @cgs.imagePrecache[cent.current.modelindex];
    cent.refEnt.origin = cent.prev.origin;
    cent.refEnt.origin2 = cent.prev.origin2;
}

}