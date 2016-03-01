#ifndef AI_BOT_H
#define AI_BOT_H

#include "../../gameshared/q_math.h"
#include "../g_local.h"
#include "ai_local.h"

class Vec3;

class Vec3Like
{
    friend class Vec3Ref;
    friend class Vec3;
private:
    Vec3Like(float *data): dataPtr(dataPtr) {}
public:
    float * const dataPtr;

    float &x() { return dataPtr[0]; }
    float &y() { return dataPtr[1]; }
    float &z() { return dataPtr[2]; }

    float x() const { return dataPtr[0]; }
    float y() const { return dataPtr[1]; }
    float z() const { return dataPtr[2]; }

    void operator += (const Vec3Like &that)
    {
        x() += that.x();
        y() += that.y();
        z() += that.z();
    }
    void operator += (const vec3_t that) { *this += Vec3Like(const_cast<float *>(that)); }

    void operator -= (const Vec3Like &that)
    {
        x() -= that.x();
        y() -= that.y();
        z() -= that.z();
    }
    void operator -= (const vec3_t that) { *this -= Vec3Like(const_cast<float *>(that)); }

    void operator *= (float scale)
    {
        x() *= scale;
        y() *= scale;
        z() *= scale;
    }

    // Can't implement these methods here since Vec3 is not defined yet
    Vec3 operator * (float scale) const;
    Vec3 operator + (const Vec3Like &that) const;
    Vec3 operator + (const vec3_t that) const;
    Vec3 operator - (const Vec3Like &that) const;
    Vec3 operator - (const vec3_t that) const;
};

class Vec3Ref: public Vec3Like
{
public:
    Vec3Ref(vec3_t that): Vec3Like(that) {}
};

class Vec3: public Vec3Like
{
private:
    vec3_t data;
public:
    Vec3(vec3_t that): Vec3Like(data)
    {
        VectorCopy(that, data);
    }
    Vec3(float x, float y, float z): Vec3Like(data)
    {
        this->x() = x;
        this->y() = y;
        this->z() = z;
    }
    Vec3(const Vec3Like &that): Vec3Like(data)
    {
        this->x() = that.x();
        this->y() = that.y();
        this->z() = that.z();
    }
};

inline Vec3 operator * (float scale, const Vec3Like &v)
{
    return v * scale;
}

inline Vec3 Vec3Like::operator * (float scale) const
{
    return Vec3(scale * x(), scale * y(), scale * z());
}
inline Vec3 Vec3Like::operator + (const Vec3Like &that) const
{
    return Vec3(x() + that.x(), y() + that.y(), z() + that.z());
}
inline Vec3 Vec3Like::operator + (const vec3_t that) const
{
    return *this + Vec3Like(const_cast<float *>(that));
}
inline Vec3 Vec3Like::operator - (const Vec3Like &that) const
{
    return Vec3(x() - that.x(), y() - that.y(), z() - that.z());
}
inline Vec3 Vec3Like::operator - (const vec3_t that) const
{
    return *this - Vec3Like(const_cast<float *>(that));
}

class EdictRef
{
public:
    edict_t * const self;
    EdictRef(edict_t *ent): self(ent) {}
};

class Ai: public EdictRef
{
public:
    Ai(edict_t *self): EdictRef(self) {}

    bool NodeReachedGeneric() const;
    bool NodeReachedSpecial() const;
    bool NodeReachedPlatformStart() const;
    bool NodeReachedPlatformEnd() const;

    bool ReachabilityVisible(vec3_t point) const;

    bool IsVisible(edict_t *other) const;
    bool IsInFront(edict_t *other) const;
    bool IsInFront2D(edict_t *other) const;
    bool NewEnemyInView(edict_t *enemy);
    unsigned int CurrentLinkType() const;

    bool AttemptWalljump();

    float ReactionTime() const { return ai().pers.cha.reaction_time; }
    float Offensiveness() const { return ai().pers.cha.offensiveness; }
    float Campiness() const { return ai().pers.cha.campiness; }
    float Firerate() const { return ai().pers.cha.firerate; }
protected:
    ai_handle_t &ai() { return *self->ai; }
    const ai_handle_t &ai() const { return *self->ai; }
};

class Bot: public Ai
{
public:
    Bot(edict_t *self): Ai(self) {}

    void SpecialMove(vec3_t lookdir, vec3_t pathdir, usercmd_t *ucmd);
    void Move(usercmd_t *ucmd);
    void MoveWander(usercmd_t *ucmd);
    bool FindRocket(vec3_t away_from_rocket);
    void CombatMovement(usercmd_t *ucmd);
    void FindEnemy();
    bool ChangeWeapon(int weapon);
    float ChooseWeapon();
    bool CheckShot(vec3_t point);
    void PredictProjectileShot(vec3_t fire_origin, float projectile_speed, vec3_t target, vec3_t target_velocity);
    bool FireWeapon(usercmd_t *ucmd);
    float PlayerWeight(edict_t *enemy);
    void UpdateStatus();
    void BlockedTimeout();
    void SayVoiceMessages();
    void GhostingFrame();
    void RunFrame();
};

#endif
