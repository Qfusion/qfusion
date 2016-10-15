#include "world_state.h"

// Use this macro so one have to write condition that matches the corresponding case and not its negation
#define TEST_OR_FAIL(condition)  \
do                               \
{                                \
    if (!(condition))            \
        return false;            \
}                                \
while (0)

#define TEST_GENERIC_CMP_SATISFY_OP(ops, values)                          \
switch (ops[i])                                                           \
{                                                                         \
    case SatisfyOp::EQ: TEST_OR_FAIL(values[i] == that.values[i]); break; \
    case SatisfyOp::NE: TEST_OR_FAIL(values[i] != that.values[i]); break; \
    case SatisfyOp::GT: TEST_OR_FAIL(values[i] > that.values[i]); break;  \
    case SatisfyOp::GE: TEST_OR_FAIL(values[i] >= that.values[i]); break; \
    case SatisfyOp::LS: TEST_OR_FAIL(values[i] < that.values[i]); break;  \
    case SatisfyOp::LE: TEST_OR_FAIL(values[i] <= that.values[i]); break; \
}

bool WorldState::IsSatisfiedBy(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that`state quickly
    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (boolVarsIgnoreFlags[i])
            continue;

        if (that.boolVarsIgnoreFlags[i])
            return false;

        if (boolVarsValues[i] != that.boolVarsValues[i])
            return false;
    }

    for (int i = 0; i < NUM_UNSIGNED_VARS; ++i)
    {
        if (unsignedVarsIgnoreFlags[i])
            continue;

        if (that.unsignedVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(unsignedVarsSatisfyOps, unsignedVarsValues);
    }

    for (int i = 0; i < NUM_FLOAT_VARS; ++i)
    {
        if (floatVarsIgnoreFlags[i])
            continue;

        if (that.floatVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(floatVarsSatisfyOps, floatVarsValues);
    }

    for (int i = 0; i < NUM_SHORT_VARS; ++i)
    {
        if (shortVarsIgnoreFlags[i])
            continue;

        if (that.shortVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(shortVarsSatisfyOps, shortVarsValues);
    }

    for (int i = 0, offset = 0; i < NUM_ORIGIN_VARS; ++i, offset += 4)
    {
        if (originVarsIgnoreFlags[i])
            continue;

        if (that.originVarsIgnoreFlags[i])
            return false;

        const OriginVar::PackedFields &packed = *(OriginVar::PackedFields *)(&originVarsData[offset + 3]);
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

    return true;
}

uint32_t WorldState::Hash() const
{
    uint32_t result = 37;

    for (int i = 0; i < NUM_UNSIGNED_VARS; ++i)
    {
        if (!(unsignedVarsIgnoreFlags[i]))
        {
            result = result * 17 + unsignedVarsValues[i];
            result = result * 17 + (unsigned)unsignedVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_FLOAT_VARS; ++i)
    {
        if (!(floatVarsIgnoreFlags[i]))
        {
            result = result * 17 + *((uint32_t *)(&floatVarsValues[i]));
            result = result * 17 + (unsigned)floatVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_SHORT_VARS; ++i)
    {
        if (!(shortVarsIgnoreFlags[i]))
        {
            result = result * 17 + shortVarsValues[i];
            result = result * 17 + (unsigned)shortVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (!(boolVarsIgnoreFlags[i]))
        {
            result = result * 17 + (unsigned)boolVarsValues[i] + 1;
        }
    }

    for (int i = 0; i < NUM_ORIGIN_VARS; ++i)
    {
        if (!(originVarsIgnoreFlags[i]))
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

    return result;
}

#define TEST_VARS_TYPE(numVars, ignoreMask, values, ops)          \
for (int i = 0; i < numVars; ++i)                                 \
{                                                                 \
    if (!(ignoreMask[i]))                                         \
    {                                                             \
        if (that.ignoreMask[i])                                   \
            return false;                                         \
        if (values[i] != that.values[i] || ops[i] != that.ops[i]) \
            return false;                                         \
    }                                                             \
    else if (that.ignoreMask[i])                                  \
        return false;                                             \
}

bool WorldState::operator==(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that` state quickly
    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (!(boolVarsIgnoreFlags[i]))
        {
            if (that.boolVarsIgnoreFlags[i])
                return false;
            if (boolVarsValues[i] != that.boolVarsValues[i])
                return false;
        }
        else if (that.boolVarsIgnoreFlags[i])
            return false;
    }

    TEST_VARS_TYPE(NUM_UNSIGNED_VARS, unsignedVarsIgnoreFlags, unsignedVarsValues, unsignedVarsSatisfyOps);
    TEST_VARS_TYPE(NUM_FLOAT_VARS, floatVarsIgnoreFlags, floatVarsValues, floatVarsSatisfyOps);
    TEST_VARS_TYPE(NUM_SHORT_VARS, shortVarsIgnoreFlags, shortVarsValues, shortVarsSatisfyOps);

    for (int i = 0; i < NUM_ORIGIN_VARS; ++i)
    {
        if (!(originVarsIgnoreFlags[i]))
        {
            if (that.originVarsIgnoreFlags[i])
                return false;
            for (int j = 0; j < 4; ++j)
            {
                if (originVarsData[i * 4 + j] != that.originVarsData[i * 4 + j])
                    return false;
            }
        }
        else if (!that.originVarsIgnoreFlags[i])
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

    return true;
}
