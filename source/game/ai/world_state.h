#ifndef QFUSION_WORLD_STATE_H
#define QFUSION_WORLD_STATE_H

#include "ai_local.h"

float DamageToKill(float health, float armor, float armorProtection, float armorDegradation);

inline float DamageToKill(float health, float armor)
{
    return DamageToKill(health, armor, g_armor_protection->value, g_armor_degradation->value);
}

class WorldState
{
    friend class FloatBaseVar;
    friend class BoolVar;
public:
    enum class SatisfyOp: unsigned char
    {
        EQ,
        NE,
        GT,
        GE,
        LS,
        LE
    };
private:
    edict_t *self;
    // WorldState operations such as copying and testing for satisfaction must be fast,
    // so vars components are stored in separate arrays for tight data packing.
    // Var types visible for external code are just thin wrappers around pointers to these values.

    enum
    {
        GoalItemWaitTime,

        NUM_UNSIGNED_VARS
    };

    enum
    {
        NUM_FLOAT_VARS
    };

    enum
    {
        Health,
        Armor,
        RawDamageToKill,

        NUM_SHORT_VARS
    };

    enum
    {
        HasQuad,
        HasShell,
        EnemyHasQuad,
        HasThreateningEnemy,
        HasJustPickedGoalItem,

        HasPositionalAdvantage,
        CanHitEnemy,
        EnemyCanHit,
        HasJustKilledEnemy,

        HasGoodSniperRangeWeapons,
        HasGoodFarRangeWeapons,
        HasGoodMiddleRangeWeapons,
        HasGoodCloseRangeWeapons,

        EnemyHasGoodSniperRangeWeapons,
        EnemyHasGoodFarRangeWeapons,
        EnemyHasGoodMiddleRangeWeapons,
        EnemyHasGoodCloseRangeWeapons,

        NUM_BOOL_VARS
    };

    enum
    {
        BotOrigin,
        EnemyOrigin,
        GoalItemOrigin,

        NUM_ORIGIN_VARS
    };

    enum
    {
        SniperRangeTacticalSpot,
        FarRangeTacticalSpot,
        MiddleRangeTacticalSpot,
        CloseRangeTacticalSpot,
        CoverSpot,

        NUM_ORIGIN_LAZY_VARS
    };

    uint32_t boolVarsValues;
    static_assert(8 * sizeof(decltype(boolVarsValues)) >= NUM_BOOL_VARS, "Values capacity overflow");

    unsigned unsignedVarsValues[NUM_UNSIGNED_VARS];
    float floatVarsValues[NUM_FLOAT_VARS];
    short shortVarsValues[NUM_SHORT_VARS];
    short originVarsData[NUM_ORIGIN_VARS * 4];
    short originLazyVarsData[NUM_ORIGIN_LAZY_VARS * 4];

    uint32_t boolVarsIgnoreFlags;
    uint8_t unsignedVarsIgnoreFlags;
    uint8_t floatVarsIgnoreFlags;
    uint8_t shortVarsIgnoreFlags;

    static_assert(8 * (sizeof(decltype(boolVarsIgnoreFlags))) >= NUM_BOOL_VARS, "Flags capacity overflow");
    static_assert(8 * (sizeof(decltype(unsignedVarsIgnoreFlags))) >= NUM_UNSIGNED_VARS, "Flags capacity overflow");
    static_assert(8 * (sizeof(decltype(floatVarsIgnoreFlags))) >= NUM_FLOAT_VARS, "Flags capacity overflow");
    static_assert(8 * (sizeof(decltype(shortVarsIgnoreFlags))) >= NUM_SHORT_VARS, "Flags capacity overflow");

    // 4 bits for a SatisfyOp is enough, pack ops for two vars in a single byte
    uint8_t unsignedVarsSatisfyOps[NUM_UNSIGNED_VARS / 2 + 1];
    uint8_t floatVarsSatisfyOps[NUM_FLOAT_VARS / 2 + 1];
    uint8_t shortVarsSatisfyOps[NUM_SHORT_VARS / 2 + 1];

    inline SatisfyOp GetVarSatisfyOp(const uint8_t *ops, int varIndex) const
    {
        const uint8_t &byte = ops[varIndex / 2];
        // Try to avoid branches, use shift to select hi or lo part of a byte
        auto shift = (varIndex % 2) * 4;
        // Do a left-shift to move the part value to rightmost 4 bits, then apply a mask for these 4 bits
        return (SatisfyOp)((byte >> shift) & 0xF);
    }

    inline void SetVarSatisfyOp(uint8_t *ops, int varIndex, SatisfyOp value)
    {
        uint8_t &byte = ops[varIndex / 2];
        auto varShift = (varIndex % 2) * 4;
        // The other packed op (hi or lo part) should be preserved.
        // If varShift is 4, complementaryShift is 0 and vice versa.
        auto complementaryShift = (((varIndex % 2) + 1) % 2) * 4;

#ifdef _DEBUG
        if (((0xF << varShift) | (0xF << complementaryShift)) != 0xFF)
            abort();
#endif

        // This mask allows to extract the kept part of a byte
        uint8_t keptPartMask = (uint8_t)(0xF << complementaryShift);
        //
        uint8_t keptPart = ((byte << complementaryShift) & keptPartMask);
        uint8_t newPart = (unsigned char)value << varShift;
        // Combine parts
        byte = keptPart | newPart;
    }

    inline const short *BotOriginData() const { return originVarsData + BotOrigin * 4; }
    inline const short *EnemyOriginData() const { return originVarsData + EnemyOrigin * 4; }

    const short *GetSniperRangeTacticalSpot();
    const short *GetFarRangeTacticalSpot();
    const short *GetMiddleRangeTacticalSpot();
    const short *GetCloseRangeTacticalSpot();
    const short *GetCoverSpot();
public:
    WorldState(edict_t *self_): self(self_) {}

#define DECLARE_COMPARABLE_VAR_CLASS(className, type)                               \
    class className                                                                 \
    {                                                                               \
        friend class WorldState;                                                    \
    protected:                                                                      \
        WorldState *parent;                                                         \
        short index;                                                                \
        className(const WorldState *parent_, short index_)                          \
            : parent(const_cast<WorldState *>(parent_)), index(index_) {}           \
    public:                                                                         \
        inline const type &Value() const                                            \
        {                                                                           \
            return parent->type##VarsValues[index];                                 \
        }                                                                           \
        inline className &SetValue(type value)                                      \
        {                                                                           \
            parent->type##VarsValues[index] = value; return *this;                  \
        }                                                                           \
        inline operator type() const { return parent->type##VarsValues[index]; }    \
        inline bool Ignore() const                                                  \
        {                                                                           \
            return (parent->type##VarsIgnoreFlags & (1 << index)) != 0;             \
        }                                                                           \
        inline className &SetIgnore(bool ignore)                                    \
        {                                                                           \
            if (ignore)                                                             \
                parent->type##VarsIgnoreFlags |= 1 << index;                        \
            else                                                                    \
                parent->type##VarsIgnoreFlags &= ~(1 << index);                     \
            return *this;                                                           \
        }                                                                           \
        inline WorldState::SatisfyOp SatisfyOp() const                              \
        {                                                                           \
            return parent->GetVarSatisfyOp(parent->type##VarsSatisfyOps, index);    \
        }                                                                           \
        inline className &SetSatisfyOp(WorldState::SatisfyOp op)                    \
        {                                                                           \
            parent->SetVarSatisfyOp(parent->type##VarsSatisfyOps, index, op);       \
            return *this;                                                           \
        }                                                                           \
        inline bool IsSatisfiedBy(type value) const                                 \
        {                                                                           \
            switch (parent->GetVarSatisfyOp(parent->type##VarsSatisfyOps, index))   \
            {                                                                       \
                case WorldState::SatisfyOp::EQ: return Value() == value;            \
                case WorldState::SatisfyOp::NE: return Value() != value;            \
                case WorldState::SatisfyOp::GT: return Value() > value;             \
                case WorldState::SatisfyOp::GE: return Value() >= value;            \
                case WorldState::SatisfyOp::LS: return Value() < value;             \
                case WorldState::SatisfyOp::LE: return Value() <= value;            \
            }                                                                       \
        }                                                                           \
    }

    DECLARE_COMPARABLE_VAR_CLASS(UnsignedVar, unsigned);

    DECLARE_COMPARABLE_VAR_CLASS(FloatVar, float);

    DECLARE_COMPARABLE_VAR_CLASS(ShortVar, short);

    class BoolVar
    {
        friend class WorldState;
        WorldState *parent;
        short index;
        BoolVar(const WorldState *parent_, short index_)
            : parent(const_cast<WorldState *>(parent_)), index(index_) {}
    public:
        inline bool Value() const { return (parent->boolVarsValues & (1 << index)) != 0; }
        inline BoolVar &SetValue(bool value)
        {
            value ? parent->boolVarsValues |= (1 << index) : parent->boolVarsValues &= ~(1 << index);
            return *this;
        }
        inline operator bool() const { return Value(); }
        inline bool IsSatisfiedBy(bool value) const
        {
            return Value() == value;
        }
        inline bool Ignore() const { return (parent->boolVarsIgnoreFlags & (1 << index)) != 0; }
        inline BoolVar SetIgnore(bool ignore)
        {
            ignore ? parent->boolVarsIgnoreFlags |= (1 << index) : parent->boolVarsIgnoreFlags &= ~(1 << index);
            return *this;
        }
    };

    // Stores a 3-dimensional world space origin vector. Dimensions are rounded up to 4 units.
    class OriginVar
    {
        friend class WorldState;
        WorldState *parent;
        short index;
        OriginVar(const WorldState *parent_, short index_)
            : parent(const_cast<WorldState *>(parent_)), index(index_) {}

        inline short *Data() { return &parent->originVarsData[index * 4]; }
        inline const short *Data() const { return &parent->originVarsData[index * 4]; }

        struct PackedFields
        {
            bool ignore: 1;
            unsigned char satisfyOp: 5;
            unsigned short epsilon: 10;

            bool operator==(const PackedFields &that) const
            {
                return *((const short *)this) == *((const short *)&that);
            }
        };

        static_assert(sizeof(PackedFields) == sizeof(short), "");
        static_assert(alignof(PackedFields) == alignof(short), "");

        inline PackedFields &Packed()
        {
            return *(PackedFields *)&parent->originLazyVarsData[index * 4 + 3];
        }

        inline const PackedFields &Packed() const
        {
            return *(const PackedFields *)&parent->originLazyVarsData[index * 4 + 3];
        }

        inline float DistanceTo(const OriginVar &that) const
        {
#ifdef _DEBUG
            if (this->Ignore())
                AI_FailWith("OriginVar", "GetDistance(): `this` var is ignored\n");
            if (that.Ignore())
                AI_FailWith("OriginVar", "GetDistance(): `that` var is ignored\n");
            // Its might be legal from coding point of view, but does not make sense
            if (this->parent != that.parent)
                AI_FailWith("OriginVar", "GetDistance(): vars belong to different world states\n");
#endif
            vec3_t unpackedThis, unpackedThat;
            VectorCopy(Data(), unpackedThis);
            VectorCopy(that.Data(), unpackedThat);
            VectorScale(unpackedThis, 4.0f, unpackedThis);
            VectorScale(unpackedThat, 4.0f, unpackedThat);
            return DistanceFast(unpackedThis, unpackedThat);
        }
    public:
        // Each coordinate is rounded up to 4 units
        // Thus maximal rounding distance error = sqrt(dx*dx + dy*dy + dz*dz) = sqrt(4*4 + 4*4 + 4*4)
        static constexpr float MAX_ROUNDING_SQUARE_DISTANCE_ERROR = 3 * 4 * 4;

        inline Vec3 Value() const
        {
            return Vec3(4 * Data()[0], 4 * Data()[1], 4 * Data()[2]);
        }
        inline OriginVar &SetValue(float x, float y, float z)
        {
            Data()[0] = (short)(((int)x) / 4);
            Data()[1] = (short)(((int)y) / 4);
            Data()[2] = (short)(((int)z) / 4);
            return *this;
        }
        inline OriginVar &SetValue(const Vec3 &value)
        {
            return SetValue(value.X(), value.Y(), value.Z());
        }
        inline OriginVar &SetValue(const vec3_t value)
        {
            return SetValue(value[0], value[1], value[2]);
        }
        inline bool Ignore() const
        {
            return Packed().ignore;
        }
        inline OriginVar &SetIgnore(bool ignore)
        {
            Packed().ignore = ignore;
            return *this;
        }
        inline OriginVar SetSatisfyOp(WorldState::SatisfyOp op, float epsilon)
        {
#ifdef _DEBUG
            if (op != SatisfyOp::EQ && op != SatisfyOp::NE)
                abort();
            if (epsilon < 4.0f || epsilon >= 4096.0f)
                abort();
#endif
            // Up to 10 bits
            unsigned short packedEpsilon = (unsigned short)((unsigned)epsilon / 4);
            unsigned char packedOp = (unsigned char)op;
            Packed().epsilon = packedEpsilon;
            Packed().satisfyOp = packedOp;
            return *this;
        }

        inline WorldState::SatisfyOp SatisfyOp() const
        {
            return (WorldState::SatisfyOp)(Packed().satisfyOp);
        }

        inline unsigned short SatisfyEpsilon() const
        {
            return (unsigned short)(Packed().epsilon);
        }

        inline bool operator!=(const OriginVar &that) const
        {
            if (!Packed().ignore)
            {
                if (!(Packed() == that.Packed()))
                    return true;

                const short *thisData = &parent->originVarsData[index * 4];
                const short *thatData = &that.parent->originVarsData[index * 4];
                return !VectorCompare(thisData, thatData);
            }

            return !that.Packed().ignore;
        }
    };

    class OriginLazyVar
    {
        friend class OriginVar;
    public:
        typedef const short *(WorldState::*ValueSupplier)();
    private:
        friend class WorldState;
        WorldState *parent;
        ValueSupplier supplier;
        short index;

        OriginLazyVar(const WorldState *parent_, short index_, ValueSupplier supplier_)
            : parent(const_cast<WorldState *>(parent_)), supplier(supplier_), index(index_) {}

        inline short *Data() { return &parent->originLazyVarsData[index * 4]; }
        inline const short *Data() const { return &parent->originLazyVarsData[index * 4]; }

        struct PackedFields
        {
            bool ignore: 1;
            unsigned short stateBits: 4;
            unsigned char satisfyOp: 1;
            unsigned short epsilon: 10;

            bool operator==(const PackedFields &that) const
            {
                return *((const short *)this) == *((const short *)&that);
            }
        };

        static_assert(sizeof(PackedFields) == sizeof(short), "");
        static_assert(alignof(PackedFields) == alignof(short), "");

        inline PackedFields &Packed()
        {
            return *(PackedFields *)&parent->originLazyVarsData[index * 4 + 3];
        }
        inline const PackedFields &Packed() const
        {
            return *(const PackedFields *)&parent->originLazyVarsData[index * 4 + 3];
        }

        inline unsigned char StateBits() const
        {
            return (unsigned char)Packed().stateBits;
        }
        // It gets called from a const function, thats why it is const too
        inline void SetStateBits(unsigned char stateBits) const
        {
            const_cast<OriginLazyVar *>(this)->Packed().stateBits = stateBits;
        }

        // This values are chosen in this way to allow zero-cost conversion to bool from ABSENT/PRESENT state.
        static constexpr unsigned char ABSENT = 0;
        static constexpr unsigned char PRESENT = 1;
        static constexpr unsigned char PENDING = 2;
    public:
        // Each coordinate is rounded up to 4 units
        // Thus maximal rounding distance error = sqrt(dx*dx + dy*dy + dz*dz) = sqrt(4*4 + 4*4 + 4*4)
        static constexpr float MAX_ROUNDING_SQUARE_DISTANCE_ERROR = 3 * 4 * 4;

        inline Vec3 Value() const
        {
            if (StateBits() == PRESENT)
                return Vec3(4 * Data()[0], 4 * Data()[1], 4 * Data()[2]);

            AI_FailWith("OriginLazyVar", "Attempt to get a value of var #%hd which is not in PRESENT state\n", index);
        }

        inline bool IsPresent() const
        {
            unsigned char stateBits = StateBits();
            if (stateBits != PENDING)
                return stateBits;

            const short *packedValues = (parent->*supplier)();
            if (packedValues)
            {
                short *data = const_cast<short*>(Data());
                VectorCopy(packedValues, data);
                SetStateBits(PRESENT);
                return true;
            }
            SetStateBits(ABSENT);
            return false;
        }

        inline void Reset()
        {
            SetStateBits(PENDING);
        }

        inline bool Ignore() const
        {
            return Packed().ignore;
        }
        inline OriginLazyVar &SetIgnore(bool ignore)
        {
            Packed().ignore = ignore;
            return *this;
        }
        inline bool IgnoreOrAbsent() const
        {
            return Ignore() || !IsPresent();
        }

        inline OriginLazyVar SetSatisfyOp(SatisfyOp op, float epsilon)
        {
#ifdef _DEBUG
            if (op != WorldState::SatisfyOp::EQ && op != WorldState::SatisfyOp::NE)
                abort();
            if (epsilon < 4.0f || epsilon >= 4096.0f)
                abort();
#endif
            // Up to 10 bits
            unsigned short packedEpsilon = (unsigned short)((unsigned)epsilon / 4);
            // A single bit
            unsigned char packedOp = (unsigned char)op;
            static_assert((unsigned short)WorldState::SatisfyOp::EQ == 0, "SatisfyOp can't be packed in a single bit");
            static_assert((unsigned short)WorldState::SatisfyOp::NE == 1, "SatisfyOp can't be packed in a single bit");
            unsigned short opVals = (packedOp << 10) | packedEpsilon;
            Packed().epsilon = packedEpsilon;
            Packed().satisfyOp = opVals;
            return *this;
        }

        inline WorldState::SatisfyOp SatisfyOp() const
        {
            return (WorldState::SatisfyOp)(Packed().satisfyOp);
        }

        inline unsigned short SatisfyEpsilon() const
        {
            return Packed().epsilon;
        }

        inline bool operator==(const OriginLazyVar &that) const
        {
#ifdef _DEBUG
            if (this->index != that.index)
                AI_FailWith("OriginLazyVar", "IsSatisfiedBy(): vars index mismatch\n");
#endif
            if (!Packed().ignore)
            {
                if (that.Packed().ignore)
                    return false;
                auto stateBits = StateBits();
                if (stateBits != that.StateBits())
                    return false;
                if (stateBits != PRESENT)
                    return true;

                if (!(Packed() == that.Packed()))
                    return false;

                const int offset = index * 4;
                const short *thisVarsData = parent->originLazyVarsData + offset;
                const short *thatVarsData = that.parent->originLazyVarsData + offset;
                return VectorCompare(thisVarsData, thatVarsData);
            }
            // `that` should be ignored too
            return that.Packed().ignore;
        }

        inline bool operator!=(const OriginLazyVar &that) { return !(*this == that); }

        inline uint32_t Hash() const
        {
            auto stateBits = StateBits();
            if (stateBits != PRESENT)
                return stateBits;
            const unsigned short *data = (const unsigned short*)Data();
            return (data[0] | (data[1] << 16)) ^ (data[2] | (data[3] << 16));
        }

        inline bool IsSatisfiedBy(const OriginLazyVar &that) const
        {
#ifdef _DEBUG
            if (this->index != that.index)
                AI_FailWith("OriginLazyVar", "IsSatisfiedBy(): vars index mismatch\n");
#endif
            if (Packed().ignore)
                return true;
            if (that.Packed().ignore)
                return false;

            auto stateBits = this->StateBits();
            // Do not force a lazy value to be computed
            if (stateBits != that.StateBits())
                return false;
            if (stateBits != PRESENT)
                return true;

            const short *thisVarsData = parent->originLazyVarsData + index * 4;
            const short *thatVarsData = that.parent->originLazyVarsData + index * 4;
            unsigned short epsilon = SatisfyEpsilon();
            switch (SatisfyOp())
            {
                case WorldState::SatisfyOp::EQ:
                    if (DistanceSquared(thisVarsData, thatVarsData) > epsilon * epsilon)
                        return false;
                    break;
                case WorldState::SatisfyOp::NE:
                    if (DistanceSquared(thisVarsData, thatVarsData) < epsilon * epsilon)
                        return false;
                    break;
                default:
                    abort();
            }
            return true;
        }

        inline float DistanceTo(const OriginVar &that) const
        {
#ifdef _DEBUG
            if (this->Ignore())
                AI_FailWith("OriginLazyVar::GetDistance(const OriginVar &)", "`this` var is ignored\n");
            if (that.Ignore())
                AI_FailWith("OriginLazyVar::GetDistance(const OriginVar &)", "`that` var is ignored\n");
            if (!this->IsPresent())
                AI_FailWith("OriginLazyVar::GetDistance(const OriginVar &)", "`this` var is not present\n");
            if (this->parent != that.parent)
                AI_FailWith("OriginLazyVar::GetDistance(const OriginVar &)", "vars belong to different world states\n");
#endif
            vec3_t unpackedThis, unpackedThat;
            VectorCopy(Data(), unpackedThis);
            VectorCopy(that.Data(), unpackedThat);
            VectorScale(unpackedThis, 4.0f, unpackedThis);
            VectorScale(unpackedThat, 4.0f, unpackedThat);
            return DistanceFast(unpackedThis, unpackedThat);
        }
    };

#define DECLARE_UNSIGNED_VAR(varName) UnsignedVar varName##Var() const { return UnsignedVar(this, varName); }
#define DECLARE_SHORT_VAR(varName) ShortVar varName##Var() const { return ShortVar(this, varName); }
#define DECLARE_BOOL_VAR(varName) BoolVar varName##Var() const { return BoolVar(this, varName); }
#define DECLARE_ORIGIN_VAR(varName) OriginVar varName##Var() const { return OriginVar(this, varName); }
#define DECLARE_ORIGIN_LAZY_VAR(varName) OriginLazyVar varName##Var() const \
{                                                                           \
    return OriginLazyVar(this, varName, &WorldState::Get##varName);         \
}

    bool IsSatisfiedBy(const WorldState &that) const;

    uint32_t Hash() const;
    bool operator==(const WorldState &that) const;

    inline void SetIgnoreAll(bool ignore)
    {
        if (ignore)
        {
            unsignedVarsIgnoreFlags = std::numeric_limits<decltype(unsignedVarsIgnoreFlags)>::max();
            floatVarsIgnoreFlags = std::numeric_limits<decltype(floatVarsIgnoreFlags)>::max();
            shortVarsIgnoreFlags = std::numeric_limits<decltype(shortVarsIgnoreFlags)>::max();
            boolVarsIgnoreFlags = std::numeric_limits<decltype(boolVarsIgnoreFlags)>::max();
        }
        else
        {
            unsignedVarsIgnoreFlags = 0;
            floatVarsIgnoreFlags = 0;
            shortVarsIgnoreFlags = 0;
            boolVarsIgnoreFlags = 0;
        }

        for (unsigned i = 0; i < NUM_ORIGIN_VARS; ++i)
            ((OriginVar::PackedFields *)&originVarsData[i * 4 + 3])->ignore = ignore;

        for (unsigned i = 0; i < NUM_ORIGIN_LAZY_VARS; ++i)
            ((OriginLazyVar::PackedFields *)&originLazyVarsData[i * 4 + 3])->ignore = ignore;
    }

    DECLARE_UNSIGNED_VAR(GoalItemWaitTime)

    DECLARE_SHORT_VAR(Health)
    DECLARE_SHORT_VAR(Armor)
    DECLARE_SHORT_VAR(RawDamageToKill)

    DECLARE_BOOL_VAR(HasQuad)
    DECLARE_BOOL_VAR(HasShell)
    DECLARE_BOOL_VAR(EnemyHasQuad)
    DECLARE_BOOL_VAR(HasThreateningEnemy)
    DECLARE_BOOL_VAR(HasJustPickedGoalItem)

    DECLARE_BOOL_VAR(HasPositionalAdvantage)
    DECLARE_BOOL_VAR(CanHitEnemy)
    DECLARE_BOOL_VAR(EnemyCanHit)
    DECLARE_BOOL_VAR(HasJustKilledEnemy)

    DECLARE_BOOL_VAR(HasGoodSniperRangeWeapons)
    DECLARE_BOOL_VAR(HasGoodFarRangeWeapons)
    DECLARE_BOOL_VAR(HasGoodMiddleRangeWeapons)
    DECLARE_BOOL_VAR(HasGoodCloseRangeWeapons)

    DECLARE_BOOL_VAR(EnemyHasGoodSniperRangeWeapons)
    DECLARE_BOOL_VAR(EnemyHasGoodFarRangeWeapons)
    DECLARE_BOOL_VAR(EnemyHasGoodMiddleRangeWeapons)
    DECLARE_BOOL_VAR(EnemyHasGoodCloseRangeWeapons)

    DECLARE_ORIGIN_VAR(BotOrigin)
    DECLARE_ORIGIN_VAR(EnemyOrigin)
    DECLARE_ORIGIN_VAR(GoalItemOrigin)

    DECLARE_ORIGIN_LAZY_VAR(SniperRangeTacticalSpot)
    DECLARE_ORIGIN_LAZY_VAR(FarRangeTacticalSpot)
    DECLARE_ORIGIN_LAZY_VAR(MiddleRangeTacticalSpot)
    DECLARE_ORIGIN_LAZY_VAR(CloseRangeTacticalSpot)
    DECLARE_ORIGIN_LAZY_VAR(CoverSpot)

    inline float DistanceToEnemy() const { return BotOriginVar().DistanceTo(EnemyOriginVar()); }
    inline float DistanceToGoalItem() const { return BotOriginVar().DistanceTo(GoalItemOriginVar()); }

    inline float DistanceToSniperRangeTacticalSpot() const
    {
        return SniperRangeTacticalSpotVar().DistanceTo(BotOriginVar());
    }
    inline float DistanceToFarRangeTacticalSpot() const
    {
        return FarRangeTacticalSpotVar().DistanceTo(BotOriginVar());
    }
    inline float DistanceToMiddleRangeTacticalSpot() const
    {
        return MiddleRangeTacticalSpotVar().DistanceTo(BotOriginVar());
    }
    inline float DistanceToCloseRangeTacticalSpot() const
    {
        return CloseRangeTacticalSpotVar().DistanceTo(BotOriginVar());
    }
    inline float DistanceToCoverSpot() const
    {
        return CoverSpotVar().DistanceTo(BotOriginVar());
    }

    inline void ResetTacticalSpots()
    {
        SniperRangeTacticalSpotVar().Reset();
        FarRangeTacticalSpotVar().Reset();
        MiddleRangeTacticalSpotVar().Reset();
        CloseRangeTacticalSpotVar().Reset();
        CoverSpotVar().Reset();
    }

    constexpr static float FAR_RANGE_MAX = 2.5f * 900.0f;
    constexpr static float MIDDLE_RANGE_MAX = 900.0f;
    constexpr static float CLOSE_RANGE_MAX = 175.0f;

    inline bool EnemyIsOnSniperRange() const
    {
        return DistanceToEnemy() > FAR_RANGE_MAX;
    }
    inline bool EnemyIsOnFarRange() const
    {
        return DistanceToEnemy() > MIDDLE_RANGE_MAX && DistanceToEnemy() <= FAR_RANGE_MAX;
    }
    inline bool EnemyIsOnMiddleRange() const
    {
        return DistanceToEnemy() > CLOSE_RANGE_MAX && DistanceToEnemy() <= MIDDLE_RANGE_MAX;
    }
    inline bool EnemyIsOnCloseRange() const
    {
        return DistanceToEnemy() <= CLOSE_RANGE_MAX;
    }

    inline float DamageToBeKilled() const
    {
        float damageToBeKilled = ::DamageToKill(HealthVar(), ArmorVar());
        if (HasShellVar())
            damageToBeKilled *= 4.0f;
        if (EnemyHasQuadVar())
            damageToBeKilled /= 4.0f;
        return damageToBeKilled;
    }

    inline float DamageToKill() const
    {
        float damageToKill = RawDamageToKillVar();
        if (HasQuadVar())
            damageToKill /= 4.0f;
        return damageToKill;
    }

    inline float KillToBeKilledDamageRatio() const
    {
        return DamageToKill() / DamageToBeKilled();
    }
};

#endif
