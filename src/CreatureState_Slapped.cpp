#include "stdafx.h"
#include "CreatureState_Slapped.h"
#include "Creature.h"
#include "CreatureAnimConst.h"

CreatureState_Slapped::CreatureState_Slapped()
    : CreatureState(eCreatureState_Slapped)
{
}

void CreatureState_Slapped::HandleEnterState(eCreatureState prevState)
{
    GetCreature().GetLocomotion().ClearGoals();
    GetCreature().GetAnimator().ChangeState(CreatureAnimConst::StateSlapped);
    if (std::getenv("KEEPER_IMP_TRACE"))
        gConsole.LogMessage(eLogLevel_Info, "Imp slap enter %u", GetCreature().GetInstanceUid());
}

void CreatureState_Slapped::HandleLeaveState(eCreatureState nextState)
{
}

void CreatureState_Slapped::HandleUpdateLogic(float stepDeltaTime)
{
    (void)stepDeltaTime;
    Animator& animator = GetCreature().GetAnimator();
    if (GetStateDuration() > 0.15f && animator.IsCurrentState(CreatureAnimConst::StatePose))
    {
        if (std::getenv("KEEPER_IMP_TRACE"))
            gConsole.LogMessage(eLogLevel_Info, "Imp slap recovered %u", GetCreature().GetInstanceUid());
        GetCreature().ChangeState(eCreatureState_Idle);
    }
}
