#ifndef AI_DANGERS_DETECTOR_H
#define AI_DANGERS_DETECTOR_H

#include "ai_local.h"

struct Danger
{
    Danger(const Vec3 &hitPoint, const Vec3 &direction, float damage, const edict_t *attacker = nullptr, bool splash = false)
        : hitPoint(hitPoint), direction(direction), damage(damage), attacker(attacker), splash(splash) {
#ifdef _DEBUG
        float len = direction.LengthFast();
        if (fabsf(len - 1.0f) > 0.1f)
        {
            printf("Denormalized danger direction: |%f %f %f| = %f\n", direction.x(), direction.y(), direction.z(), len);
            abort();
        }
#endif
    }

    // Sorting by this operator is fast but should be used only
    // to prepare most dangerous entities of the same type.
    // Ai decisions should be made by more sophisticated code.
    bool operator<(const Danger &that) const { return this->damage < that.damage; }

    Vec3 hitPoint;
    Vec3 direction;
    float damage;
    const edict_t *attacker;
    bool splash;
};

class DangersDetector
{
    const edict_t *const self; // Bot reference

public:
    DangersDetector(const edict_t *bot): self(bot) {}

    static constexpr unsigned MAX_ROCKETS = 3;
    static constexpr unsigned MAX_PLASMA_BEAMS = 3;
    static constexpr unsigned MAX_GRENADES = 3;
    static constexpr unsigned MAX_BLASTS = 3;
    static constexpr unsigned MAX_LASER_BEAMS = 3;

    StaticVector<Danger, MAX_ROCKETS> rocketDangers;
    StaticVector<Danger, MAX_PLASMA_BEAMS> plasmaDangers;
    StaticVector<Danger, MAX_GRENADES> grenadeDangers;
    StaticVector<Danger, MAX_BLASTS> blastsDangers;
    StaticVector<Danger, MAX_LASER_BEAMS> laserDangers;

    const Danger *primaryDanger;

    bool FindDangers();
private:
    void Clear();
    void RegisterDanger(const Danger &danger);
    template<unsigned N> void ScaleAndRegisterDangers(StaticVector<Danger, N> &danger, float damageScale);
    template<unsigned N, unsigned M>
    bool FindProjectileDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t *, M> &entities, float dangerRadius);
    template<unsigned N, unsigned M>
    bool FindPlasmaDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t*, M> &plasmas, float plasmaSplashRadius);
    template<unsigned N, unsigned M>
    bool FindLaserDangers(StaticVector<Danger, N> &dangers, StaticVector<const edict_t*, M> &lasers);
};

#endif //QFUSION_DANGERS_DETECTOR_H
