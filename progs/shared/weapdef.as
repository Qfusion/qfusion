namespace GS {

namespace Weapons {

class FireDef {
    // ammo def
    eFireMode fireMode;
    ammo_tag_e ammoID;
    int usageCount;
    int projectileCount;

    // timings
    uint weaponUpTime;
    uint weaponDownTime;
    uint reloadTime;
    uint cooldownTime;
    uint timeout;
    bool smoothRefire;

    // damages
    float damage;
    float selfDamage;
    int knockback;
    int stun;
    int splashRadius;
    int minDamage;
    int minKnockback;

    // projectile def
    int speed;
    int spread;     // horizontal spread
    int vSpread;    // vertical spread

    // ammo amounts
    int weaponPickup;
    int ammoPickup;
    int ammoMax;
    int ammoLow;

    FireDef(
        eFireMode fireMode,
        ammo_tag_e ammoID,
        int usageCount,
        int projectileCount,
        uint weaponUpTime,
        uint weaponDownTime,
        uint reloadTime,
        uint cooldownTime,
        uint timeout,
        bool smoothRefire,
        float damage,
        float selfDamage,
        int knockback,
        int stun,
        int splashRadius,
        int minDamage,
        int minKnockback,
        int speed,
        int spread,
        int vSpread,
        int weaponPickup,
        int ammoPickup,
        int ammoMax,
        int ammoLow
    ) {
        this.fireMode = fireMode;
        this.ammoID = ammoID;
        this.usageCount = usageCount;
        this.projectileCount = projectileCount;
        this.weaponUpTime = weaponUpTime;
        this.weaponDownTime = weaponDownTime;
        this.reloadTime = reloadTime;
        this.cooldownTime = cooldownTime;
        this.timeout = timeout;
        this.smoothRefire = smoothRefire;
        this.damage = damage;
        this.selfDamage = selfDamage;
        this.knockback = knockback;
        this.stun = stun;
        this.splashRadius = splashRadius;
        this.minDamage = minDamage;
        this.minKnockback = minKnockback;
        this.speed = speed;
        this.spread = spread;
        this.vSpread = vSpread;
        this.weaponPickup = weaponPickup;
        this.ammoPickup = ammoPickup;
        this.ammoMax = ammoMax;
        this.ammoLow = ammoLow;
    }
}

class WeaponDef {
    const String @name;
    int weaponId;

    FireDef @fireDef;
    FireDef @fireDefWeak;

    WeaponDef(
        const String &in name,
        int weaponId,
        FireDef &in fireDef,
        FireDef &in fireDefWeak
    ) {
        @this.name = name;
        this.weaponId = weaponId;
        @this.fireDef = @fireDef;
        @this.fireDefWeak = @fireDefWeak;
    }
}

const int INSTANT = 0;

const int WEAPONDOWN_FRAMETIME = 50;
const int WEAPONUP_FRAMETIME = 50;
const int DEFAULT_BULLET_SPREAD = 350;

// List of weapon definitions
array<WeaponDef@> weaponDefs = {
    WeaponDef(
        "no weapon",
        WEAP_NONE,
        FireDef(
            FIRE_MODE_STRONG, // fire mode
            AMMO_NONE,        // ammo tag
            0,                // ammo usage per shot
            0,                // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME, // weapon up frametime
            WEAPONDOWN_FRAMETIME, // weapon down frametime
            0,                  // reload frametime
            0,                  // cooldown frametime
            0,                  // projectile timeout
            false,              // smooth refire

            // damages
            0,                  // damage
            0,                  // selfdamage ratio
            0,                  // knockback
            0,                  // stun
            0,                  // splash radius
            0,                  // splash minimum damage
            0,                  // splash minimum knockback

            // projectile def
            0,                  // speed
            0,                  // spread
            0,                  // v_spread

            // ammo
            0,                  // weapon pickup amount
            0,                  // pickup amount
            0,                  // max amount
            0                   // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,     // fire mode
            AMMO_NONE,          // ammo tag
            0,                  // ammo usage per shot
            0,                  // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME, // weapon up frametime
            WEAPONDOWN_FRAMETIME, // weapon down frametime
            0,                  // reload frametime
            0,                  // cooldown frametime
            0,                  // projectile timeout
            false,              // smooth refire

            // damages
            0,                  // damage
            0,                  // selfdamage ratio
            0,                  // knockback
            0,                  // stun
            0,                  // splash radius
            0,                  // splash minimum damage
            0,                  // splash minimum knockback

            // projectile def
            0,                  // speed
            0,                  // spread
            0,                  // v_spread

            // ammo
            0,                  // weapon pickup amount
            0,                  // pickup amount
            0,                  // max amount
            0                   // low ammo threshold
        )
    ),
    WeaponDef(
        "Gunblade",
        WEAP_GUNBLADE,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_GUNBLADE,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            600, // reload frametime
            0,   // cooldown frametime
            5000, // projectile timeout
            false, // smooth refire

            // damages
            35,   // damage
            1.0,  // selfdamage ratio
            45,   // knockback
            0,    // stun
            80,   // splash radius
            8,    // splash minimum damage
            10,   // splash minimum knockback

            // projectile def
            3000, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            0,    // weapon pickup amount
            0,    // pickup amount
            1,    // max amount
            0     // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_GUNBLADE,
            0, // ammo usage per shot
            0, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            600, // reload frametime
            0,   // cooldown frametime
            64,  // projectile timeout / projectile range for instant weapons
            false, // smooth refire

            // damages
            50,   // damage
            0,    // selfdamage ratio
            50,   // knockback
            0,    // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            0,    // speed
            0,    // spread
            0,    // v_spread

            // ammo
            0,    // weapon pickup amount
            0,    // pickup amount
            0,    // max amount
            0     // low ammo threshold
        )
    ),
    WeaponDef(
        "Machinegun",
        WEAP_MACHINEGUN,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_BULLETS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            100, // reload frametime
            0,   // cooldown frametime
            6000, // projectile timeout
            false, // smooth refire

            // damages
            10,   // damage
            0,    // selfdamage ratio
            10,   // knockback
            50,   // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            10,      // spread
            10,      // v_spread

            // ammo
            50,      // weapon pickup amount
            50,      // pickup amount
            100,     // max amount
            20       // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_BULLETS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            100, // reload frametime
            0,   // cooldown frametime
            6000, // projectile timeout
            false, // smooth refire

            // damages
            10,   // damage
            0,    // selfdamage ratio
            10,   // knockback
            50,   // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            10,      // spread
            10,      // v_spread

            // ammo
            0,       // weapon pickup amount
            0,       // pickup amount
            0,       // max amount
            0        // low ammo threshold
        )
    ),
    WeaponDef(
        "Riotgun",
        WEAP_RIOTGUN,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_SHELLS,
            1, // ammo usage per shot
            20, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            900, // reload frametime
            0,   // cooldown frametime
            8192, // projectile timeout / projectile range for instant weapons
            false, // smooth refire

            // damages
            5,    // damage
            0,    // selfdamage ratio (rg cant selfdamage)
            7,    // knockback
            85,   // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            160,     // spread
            90,      // v_spread

            // ammo
            10,      // weapon pickup amount
            10,      // pickup amount
            20,      // max amount
            3        // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_SHELLS,
            1, // ammo usage per shot
            25, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            900, // reload frametime
            0,   // cooldown frametime
            8192, // projectile timeout / projectile range for instant weapons
            false, // smooth refire

            // damages
            4,    // damage
            0,    // selfdamage ratio (rg cant selfdamage)
            5,    // knockback
            85,   // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            160,     // spread
            90,      // v_spread

            // ammo
            0,       // weapon pickup amount
            0,       // pickup amount
            0,       // max amount
            0        // low ammo threshold
        )
    ),
    WeaponDef(
        "Grenade Launcher",
        WEAP_GRENADELAUNCHER,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_GRENADES,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            800, // reload frametime
            0,   // cooldown frametime
            1250, // projectile timeout
            false, // smooth refire

            // damages
            80,   // damage
            1.00, // selfdamage ratio
            60,   // knockback
            1250, // stun
            125,  // splash radius
            15,   // splash minimum damage
            35,   // splash minimum knockback

            // projectile def
            1000, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            10,   // weapon pickup amount
            10,   // pickup amount
            20,   // max amount
            3     // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_GRENADES,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            800, // reload frametime
            0,   // cooldown frametime
            1250, // projectile timeout
            false, // smooth refire

            // damages
            80,   // damage
            1.00, // selfdamage ratio
            60,   // knockback
            1250, // stun
            135,  // splash radius
            15,   // splash minimum damage
            35,   // splash minimum knockback

            // projectile def
            1000, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            0,    // weapon pickup amount
            0,    // pickup amount
            0,    // max amount
            0     // low ammo threshold
        )
    ),
    WeaponDef(
        "Rocket Launcher",
        WEAP_ROCKETLAUNCHER,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_ROCKETS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            950, // reload frametime
            0,   // cooldown frametime
            10000, // projectile timeout
            false, // smooth refire

            // damages
            80,   // damage
            1.00, // selfdamage ratio
            60,   // knockback
            1250, // stun
            125,  // splash radius
            15,   // splash minimum damage
            35,   // splash minimum knockback

            // projectile def
            1150, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            5,    // weapon pickup amount
            10,   // pickup amount
            20,   // max amount
            3     // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_ROCKETS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            950, // reload frametime
            0,   // cooldown frametime
            10000, // projectile timeout
            false, // smooth refire

            // damages
            80,   // damage
            1.00, // selfdamage ratio
            60,   // knockback
            1250, // stun
            135,  // splash radius
            15,   // splash minimum damage
            35,   // splash minimum knockback

            // projectile def
            1150, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            0,    // weapon pickup amount
            0,    // pickup amount
            0,    // max amount
            0     // low ammo threshold
        )
    ),
    WeaponDef(
        "Plasmagun",
        WEAP_PLASMAGUN,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_PLASMA,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            100, // reload frametime
            0,   // cooldown frametime
            5000, // projectile timeout
            false, // smooth refire

            // damages
            15,   // damage
            0.5,  // selfdamage ratio
            20,   // knockback
            200,  // stun
            45,   // splash radius
            5,    // splash minimum damage
            1,    // splash minimum knockback

            // projectile def
            2500, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            50,   // weapon pickup amount
            100,  // pickup amount
            150,  // max amount
            20    // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_PLASMA,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            100, // reload frametime
            0,   // cooldown frametime
            5000, // projectile timeout
            false, // smooth refire

            // damages
            15,   // damage
            0.5,  // selfdamage ratio
            20,   // knockback
            200,  // stun
            45,   // splash radius
            5,    // splash minimum damage
            1,    // splash minimum knockback

            // projectile def
            2500, // speed
            0,    // spread
            0,    // v_spread

            // ammo
            0,    // weapon pickup amount
            0,    // pickup amount
            0,    // max amount
            0     // low ammo threshold
        )
    ),
    WeaponDef(
        "Lasergun",
        WEAP_LASERGUN,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_LASERS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            50, // reload frametime
            0,  // cooldown frametime
            850, // projectile timeout / projectile range for instant weapons
            true, // smooth refire

            // damages
            7,    // damage
            0,    // selfdamage ratio (lg cant damage)
            14,   // knockback
            300,  // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            50,      // weapon pickup amount
            100,     // pickup amount
            150,     // max amount
            20       // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_LASERS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            50, // reload frametime
            0,  // cooldown frametime
            850, // projectile timeout / projectile range for instant weapons
            true, // smooth refire

            // damages
            7,    // damage
            0,    // selfdamage ratio (lg cant damage)
            14,   // knockback
            300,  // stun
            0,    // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            0,       // weapon pickup amount
            0,       // pickup amount
            0,       // max amount
            0        // low ammo threshold
        )
    ),
    WeaponDef(
        "Electrobolt",
        WEAP_ELECTROBOLT,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_BOLTS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            1250, // reload frametime
            0,    // cooldown frametime
            900,  // min damage range
            false, // smooth refire

            // damages
            75,   // damage
            0,    // selfdamage ratio
            80,   // knockback
            1000, // stun
            0,    // splash radius
            75,   // minimum damage
            35,   // minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            5,       // weapon pickup amount
            10,      // pickup amount
            10,      // max amount
            3        // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_BOLTS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            1250, // reload frametime
            0,    // cooldown frametime
            900,  // min damage range
            false, // smooth refire

            // damages
            75,   // damage
            0,    // selfdamage ratio
            40,   // knockback
            1000, // stun
            0,    // splash radius
            75,   // minimum damage
            35,   // minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            0,       // weapon pickup amount
            0,       // pickup amount
            0,       // max amount
            0        // low ammo threshold
        )
    ),
    WeaponDef(
        "Instagun",
        WEAP_INSTAGUN,
        FireDef(
            FIRE_MODE_STRONG,
            AMMO_INSTAS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            1300, // reload frametime
            0,    // cooldown frametime
            8024, // range
            false, // smooth refire

            // damages
            200,  // damage
            0.1,  // selfdamage ratio (ig cant damage)
            95,   // knockback
            1000, // stun
            80,   // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            5,       // weapon pickup amount
            5,       // pickup amount
            5,       // max amount
            0        // low ammo threshold
        ),
        FireDef(
            FIRE_MODE_WEAK,
            AMMO_WEAK_INSTAS,
            1, // ammo usage per shot
            1, // projectiles fired each shot

            // timings (in msecs)
            WEAPONUP_FRAMETIME,
            WEAPONDOWN_FRAMETIME,
            1300, // reload frametime
            0,    // cooldown frametime
            8024, // range
            false, // smooth refire

            // damages
            200,  // damage
            0.1,  // selfdamage ratio (ig cant damage)
            95,   // knockback
            1000, // stun
            80,   // splash radius
            0,    // splash minimum damage
            0,    // splash minimum knockback

            // projectile def
            INSTANT, // speed
            0,       // spread
            0,       // v_spread

            // ammo
            5,       // weapon pickup amount
            5,       // pickup amount
            15,      // max amount
            0        // low ammo threshold
        )
    )
};

/*
* getWeaponDef
*/
WeaponDef@ getWeaponDef(int weapon) {
    return weaponDefs[weapon];
}

/*
* initWeapons
* Sets ammo pickup and max values for each weapon's ammo and weak ammo items.
*/
void initWeapons() {
    for (int i = WEAP_GUNBLADE; i < WEAP_TOTAL; i++) {
        WeaponDef@ weaponDef = getWeaponDef(i);
        if (@weaponDef !is null)
            continue;

        // Set strong ammo item properties
        if (weaponDef.fireDef.ammoID != AMMO_NONE) {
            Item@ ammo = GS::FindItemByTag(weaponDef.fireDef.ammoID);
            if (@ammo !is null) {
                ammo.quantity = weaponDef.fireDef.ammoPickup;
                ammo.inventoryMax = weaponDef.fireDef.ammoMax;
            }
        }

        // Set weak ammo item properties
        if (weaponDef.fireDefWeak.ammoID != AMMO_NONE) {
            Item@ weakAmmo = GS::FindItemByTag(weaponDef.fireDefWeak.ammoID);
            if (@weakAmmo !is null) {
                weakAmmo.quantity = weaponDef.fireDefWeak.ammoPickup;
                weakAmmo.inventoryMax = weaponDef.fireDefWeak.ammoMax;
            }
        }
    }
}

}

}
