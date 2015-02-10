/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007, 2013 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 *
 * This file has been modified for OpenAL-MOB from the Original OpenAL-Soft.
 */
#include "config-oal.h"

#include "alMain.h"
#include "alSource.h"
#include "mixer_defs.h"

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define REAL_MERGE2(a,b) a##b
#define MERGE2(a,b) REAL_MERGE2(a,b)

#define MixDirect_Hrtf MERGE2(MixDirect_Hrtf_,SUFFIX)


__inline void ApplyCoeffsStep(ALuint Offset, ALfloat (*RESTRICT Values)[2],
                                     const ALuint irSize,
                                     ALfloat (*RESTRICT Coeffs)[2],
                                     const ALfloat (*RESTRICT CoeffStep)[2],
                                     ALfloat left, ALfloat right);
__inline void ApplyCoeffs(ALuint Offset, ALfloat (*RESTRICT Values)[2],
                                 const ALuint irSize,
                                 ALfloat (*RESTRICT Coeffs)[2],
                                 ALfloat left, ALfloat right);


void MixDirect_Hrtf(const DirectParams *params, const ALfloat *RESTRICT data, ALuint srcchan,
  ALuint OutPos, ALuint SamplesToDo, ALuint BufferSize)
{
    ALfloat (*RESTRICT DryBuffer)[BUFFERSIZE] = params->OutBuffer;
    ALfloat *RESTRICT ClickRemoval = params->ClickRemoval;
    ALfloat *RESTRICT PendingClicks = params->PendingClicks;
    const ALuint IrSize = params->Hrtf.Params.IrSize;
    const ALint *RESTRICT DelayStep = params->Hrtf.Params.DelayStep;
    const ALfloat (*RESTRICT CoeffStep)[2] = params->Hrtf.Params.CoeffStep;
    const ALfloat (*RESTRICT TargetCoeffs)[2] = params->Hrtf.Params.Coeffs[srcchan];
    const ALuint *RESTRICT TargetDelay = params->Hrtf.Params.Delay[srcchan];
    ALfloat *RESTRICT History = params->Hrtf.State->History[srcchan];
    ALfloat (*RESTRICT Values)[2] = params->Hrtf.State->Values[srcchan];
    ALint Counter = maxu(params->Hrtf.State->Counter, OutPos) - OutPos;
    ALuint Offset = params->Hrtf.State->Offset + OutPos;
    ALIGN(16) ALfloat Coeffs[HRIR_LENGTH][2];
    ALuint Delay[2];
    ALfloat left, right;
    ALuint pos;
    ALuint c;

    pos = 0;
    for(c = 0;c < IrSize;c++)
    {
        Coeffs[c][0] = TargetCoeffs[c][0] - (CoeffStep[c][0]*Counter);
        Coeffs[c][1] = TargetCoeffs[c][1] - (CoeffStep[c][1]*Counter);
    }

    Delay[0] = TargetDelay[0] - (DelayStep[0]*Counter);
    Delay[1] = TargetDelay[1] - (DelayStep[1]*Counter);

    if(LIKELY(OutPos == 0))
    {
        History[Offset&SRC_HISTORY_MASK] = data[pos];
        left  = lerp(History[(Offset-(Delay[0]>>HRTFDELAY_BITS))&SRC_HISTORY_MASK],
                     History[(Offset-(Delay[0]>>HRTFDELAY_BITS)-1)&SRC_HISTORY_MASK],
                     (Delay[0]&HRTFDELAY_MASK)*(1.0f/HRTFDELAY_FRACONE));
        right = lerp(History[(Offset-(Delay[1]>>HRTFDELAY_BITS))&SRC_HISTORY_MASK],
                     History[(Offset-(Delay[1]>>HRTFDELAY_BITS)-1)&SRC_HISTORY_MASK],
                     (Delay[1]&HRTFDELAY_MASK)*(1.0f/HRTFDELAY_FRACONE));

        ClickRemoval[FrontLeft]  -= Values[(Offset+1)&HRIR_MASK][0] +
                                    Coeffs[0][0] * left;
        ClickRemoval[FrontRight] -= Values[(Offset+1)&HRIR_MASK][1] +
                                    Coeffs[0][1] * right;
    }
    for(pos = 0;pos < BufferSize && Counter > 0;pos++)
    {
        History[Offset&SRC_HISTORY_MASK] = data[pos];
        left  = lerp(History[(Offset-(Delay[0]>>HRTFDELAY_BITS))&SRC_HISTORY_MASK],
                     History[(Offset-(Delay[0]>>HRTFDELAY_BITS)-1)&SRC_HISTORY_MASK],
                     (Delay[0]&HRTFDELAY_MASK)*(1.0f/HRTFDELAY_FRACONE));
        right = lerp(History[(Offset-(Delay[1]>>HRTFDELAY_BITS))&SRC_HISTORY_MASK],
                     History[(Offset-(Delay[1]>>HRTFDELAY_BITS)-1)&SRC_HISTORY_MASK],
                     (Delay[1]&HRTFDELAY_MASK)*(1.0f/HRTFDELAY_FRACONE));

        Delay[0] += DelayStep[0];
        Delay[1] += DelayStep[1];

        Values[(Offset+IrSize)&HRIR_MASK][0] = 0.0f;
        Values[(Offset+IrSize)&HRIR_MASK][1] = 0.0f;
        Offset++;

        ApplyCoeffsStep(Offset, Values, IrSize, Coeffs, CoeffStep, left, right);
        DryBuffer[FrontLeft][OutPos]  += Values[Offset&HRIR_MASK][0];
        DryBuffer[FrontRight][OutPos] += Values[Offset&HRIR_MASK][1];

        OutPos++;
        Counter--;
    }

    Delay[0] >>= HRTFDELAY_BITS;
    Delay[1] >>= HRTFDELAY_BITS;
    for(;pos < BufferSize;pos++)
    {
        History[Offset&SRC_HISTORY_MASK] = data[pos];
        left = History[(Offset-Delay[0])&SRC_HISTORY_MASK];
        right = History[(Offset-Delay[1])&SRC_HISTORY_MASK];

        Values[(Offset+IrSize)&HRIR_MASK][0] = 0.0f;
        Values[(Offset+IrSize)&HRIR_MASK][1] = 0.0f;
        Offset++;

        ApplyCoeffs(Offset, Values, IrSize, Coeffs, left, right);
        DryBuffer[FrontLeft][OutPos]  += Values[Offset&HRIR_MASK][0];
        DryBuffer[FrontRight][OutPos] += Values[Offset&HRIR_MASK][1];

        OutPos++;
    }
    if(LIKELY(OutPos == SamplesToDo))
    {
        History[Offset&SRC_HISTORY_MASK] = data[pos];
        left = History[(Offset-Delay[0])&SRC_HISTORY_MASK];
        right = History[(Offset-Delay[1])&SRC_HISTORY_MASK];

        PendingClicks[FrontLeft]  += Values[(Offset+1)&HRIR_MASK][0] +
                                     Coeffs[0][0] * left;
        PendingClicks[FrontRight] += Values[(Offset+1)&HRIR_MASK][1] +
                                     Coeffs[0][1] * right;
    }
}


#undef MixDirect_Hrtf

#undef MERGE2
#undef REAL_MERGE2

#undef UNLIKELY
#undef LIKELY
