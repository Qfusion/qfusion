#include "world_state.h"

// Use this macro so one have to write condition that matches the corresponding case and not its negation
#define TEST_OR_FAIL(condition)  \
do                               \
{                                \
    if (!(condition))            \
        return false;            \
}                                \
while (0)

template <typename T>
static inline bool TestCareFlags(T thisFlags, T thatFlags)
{
    T careMask = thisFlags ^ std::numeric_limits<T>::max();
    T thatCareMask = thatFlags ^ std::numeric_limits<T>::max();
    // There are vars that this world state cares about and that one do not
    if (careMask & ~thatCareMask)
        return false;

    return true;
}

#define TEST_GENERIC_COMPARABLE_VARS_SATISFACTION(values, flags, ops)             \
do                                                                                \
{                                                                                 \
    if (!TestCareFlags(flags, that.flags)) return false;                          \
    decltype(flags) mask = 1;                                                     \
    for (auto i = 0; i < sizeof(values) / sizeof(values[0]); ++i, mask <<= 1)     \
    {                                                                             \
        if (flags & mask) continue;                                               \
        switch (this->GetVarSatisfyOp(ops, i))                                    \
        {                                                                         \
            case SatisfyOp::EQ: TEST_OR_FAIL(values[i] == that.values[i]); break; \
            case SatisfyOp::NE: TEST_OR_FAIL(values[i] != that.values[i]); break; \
            case SatisfyOp::GT: TEST_OR_FAIL(values[i] > that.values[i]); break;  \
            case SatisfyOp::GE: TEST_OR_FAIL(values[i] >= that.values[i]); break; \
            case SatisfyOp::LS: TEST_OR_FAIL(values[i] < that.values[i]); break;  \
            case SatisfyOp::LE: TEST_OR_FAIL(values[i] <= that.values[i]); break; \
        }                                                                         \
    }                                                                             \
} while (0)

bool WorldState::IsSatisfiedBy(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that`state quickly
    if (!TestCareFlags(boolVarsIgnoreFlags, that.boolVarsIgnoreFlags))
        return false;

    auto boolVarsCareMask = boolVarsIgnoreFlags ^ std::numeric_limits<decltype(boolVarsIgnoreFlags)>::max();
    // If values masked for this ignore flags do not match
    if ((boolVarsValues & boolVarsCareMask) != (that.boolVarsValues & boolVarsCareMask))
        return false;

    TEST_GENERIC_COMPARABLE_VARS_SATISFACTION(unsignedVarsValues, unsignedVarsIgnoreFlags, unsignedVarsSatisfyOps);
    TEST_GENERIC_COMPARABLE_VARS_SATISFACTION(floatVarsValues, floatVarsIgnoreFlags, floatVarsSatisfyOps);
    TEST_GENERIC_COMPARABLE_VARS_SATISFACTION(shortVarsValues, shortVarsIgnoreFlags, shortVarsSatisfyOps);

    for (int i = 0, offset = 0; i < NUM_ORIGIN_VARS; ++i, offset += 4)
    {
        const OriginVar::PackedFields &packed = *(OriginVar::PackedFields *)(&originVarsData[offset + 3]);
        const OriginVar::PackedFields &thatPacked = *(OriginVar::PackedFields *)(&that.originVarsData[offset + 3]);

        if (packed.ignore)
            continue;
        if (thatPacked.ignore)
            return false;

        const unsigned short epsilon = packed.epsilon;
        const short *thisOriginData = originVarsData + offset;
        const short *thatOriginData = that.originVarsData + offset;

        switch ((SatisfyOp)packed.satisfyOp)
        {
            case SatisfyOp::EQ:
                if (DistanceSquared(thisOriginData, thatOriginData) > epsilon * epsilon)
                    return false;
                break;
            case SatisfyOp::NE:
                if (DistanceSquared(thisOriginData, thatOriginData) < epsilon * epsilon)
                    return false;
                break;
            default:
                abort();
        }
    }

    if (!SniperRangeTacticalSpotVar().IsSatisfiedBy(that.SniperRangeTacticalSpotVar()))
        return false;
    if (!FarRangeTacticalSpotVar().IsSatisfiedBy(that.FarRangeTacticalSpotVar()))
        return false;
    if (!MiddleRangeTacticalSpotVar().IsSatisfiedBy(that.MiddleRangeTacticalSpotVar()))
        return false;
    if (!CloseRangeTacticalSpotVar().IsSatisfiedBy(that.CloseRangeTacticalSpotVar()))
        return false;
    if (!CoverSpotVar().IsSatisfiedBy(that.CoverSpotVar()))
        return false;

    if (!RunAwayTeleportOriginVar().IsSatisfiedBy(that.RunAwayTeleportOriginVar()))
        return false;
    if (!RunAwayJumppadOriginVar().IsSatisfiedBy(that.RunAwayJumppadOriginVar()))
        return false;
    if (!RunAwayElevatorOriginVar().IsSatisfiedBy(that.RunAwayElevatorOriginVar()))
        return false;

    return true;
}

uint32_t WorldState::Hash() const
{
    uint32_t result = 37;

    decltype(unsignedVarsIgnoreFlags) unsignedVarsMask = 1;
    for (int i = 0; i < NUM_UNSIGNED_VARS; ++i, unsignedVarsMask <<= 1)
    {
        if (unsignedVarsIgnoreFlags & unsignedVarsMask)
            continue;
        result = result * 17 + unsignedVarsValues[i];
        result = result * 17 + (unsigned)GetVarSatisfyOp(unsignedVarsSatisfyOps, i) + 1;
    }

    decltype(floatVarsIgnoreFlags) floatVarsMask = 1;
    for (int i = 0; i < NUM_FLOAT_VARS; ++i)
    {
        if (floatVarsIgnoreFlags & floatVarsMask)
            continue;
        result = result * 17 + *((uint32_t *)(&floatVarsValues[i]));
        result = result * 17 + (unsigned)GetVarSatisfyOp(floatVarsSatisfyOps, i) + 1;
    }

    decltype(shortVarsIgnoreFlags) shortVarsMask = 1;
    for (int i = 0; i < NUM_SHORT_VARS; ++i)
    {
        if (shortVarsIgnoreFlags & shortVarsMask)
        {
            result = result * 17 + shortVarsValues[i];
            result = result * 17 + (unsigned)GetVarSatisfyOp(shortVarsSatisfyOps, i) + 1;
        }
    }

    result = result * 17;
    result += boolVarsValues & (boolVarsIgnoreFlags ^ std::numeric_limits<decltype(boolVarsIgnoreFlags)>::max());

    for (int i = 0; i < NUM_ORIGIN_VARS; ++i)
    {
        const auto &packed = *(OriginVar::PackedFields *)&originVarsData[i * 4 + 3];
        if (!(packed.ignore))
        {
            for (int j = 0; j < 4; ++j)
                result = result * 17 + originVarsData[i * 4 + j];
        }
    }

    result = result * 17 + SniperRangeTacticalSpotVar().Hash();
    result = result * 17 + FarRangeTacticalSpotVar().Hash();
    result = result * 17 + MiddleRangeTacticalSpotVar().Hash();
    result = result * 17 + CloseRangeTacticalSpotVar().Hash();
    result = result * 17 + CoverSpotVar().Hash();

    result = result * 17 + RunAwayTeleportOriginVar().Hash();
    result = result * 17 + RunAwayJumppadOriginVar().Hash();
    result = result * 17 + RunAwayElevatorOriginVar().Hash();

    return result;
}

#define TEST_VARS_EQUALITY(values, flags, ops)                           \
do                                                                       \
{                                                                        \
    if (flags != that.flags)                                             \
        return false;                                                    \
    decltype(flags) mask = 1;                                            \
    for (int i = 0; i < sizeof(values) / sizeof(values[0]); ++i)         \
    {                                                                    \
        if (!(flags & mask))                                             \
        {                                                                \
            if (values[i] != that.values[i])                             \
                return false;                                            \
            if (GetVarSatisfyOp(ops, i) != GetVarSatisfyOp(that.ops, i)) \
                return false;                                            \
        }                                                                \
        mask <<= 1;                                                      \
    }                                                                    \
}                                                                        \
while (0)

bool WorldState::operator==(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that` state quickly

    if (boolVarsIgnoreFlags != that.boolVarsIgnoreFlags)
        return false;

    auto boolVarsCareFlags = boolVarsIgnoreFlags ^ std::numeric_limits<decltype(boolVarsIgnoreFlags)>::max();
    if ((boolVarsValues & boolVarsCareFlags) != (that.boolVarsValues & boolVarsCareFlags))
        return false;

    TEST_VARS_EQUALITY(unsignedVarsValues, unsignedVarsIgnoreFlags, unsignedVarsSatisfyOps);
    TEST_VARS_EQUALITY(floatVarsValues, floatVarsIgnoreFlags, floatVarsSatisfyOps);
    TEST_VARS_EQUALITY(shortVarsValues, shortVarsIgnoreFlags, shortVarsSatisfyOps);

    for (int i = 0, offset = 0; i < NUM_ORIGIN_VARS; ++i, offset += 4)
    {
        const auto &packed = *((OriginVar::PackedFields *)&originVarsData[offset + 3]);
        const auto &thatPacked = *((OriginVar::PackedFields *)&that.originVarsData[offset + 3]);
        if (!packed.ignore)
        {
            if (thatPacked.ignore)
                return false;

            for (int j = 0; j < 4; ++j)
            {
                if (originVarsData[offset + j] != that.originVarsData[offset + j])
                    return false;
            }
        }
        else if (!thatPacked.ignore)
            return false;
    }

    if (SniperRangeTacticalSpotVar() != that.SniperRangeTacticalSpotVar())
        return false;
    if (FarRangeTacticalSpotVar() != that.FarRangeTacticalSpotVar())
        return false;
    if (MiddleRangeTacticalSpotVar() != that.MiddleRangeTacticalSpotVar())
        return false;
    if (CloseRangeTacticalSpotVar() != that.CloseRangeTacticalSpotVar())
        return false;
    if (CoverSpotVar() != that.CoverSpotVar())
        return false;

    if (RunAwayTeleportOriginVar() != that.RunAwayTeleportOriginVar())
        return false;
    if (RunAwayJumppadOriginVar() != that.RunAwayJumppadOriginVar())
        return false;
    if (RunAwayElevatorOriginVar() != that.RunAwayElevatorOriginVar())
        return false;

    return true;
}
