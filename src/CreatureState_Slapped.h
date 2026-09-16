#pragma once

#include "CreatureState.h"

class CreatureState_Slapped final: public CreatureState
{
public:
    CreatureState_Slapped();

protected:
    void HandleEnterState(eCreatureState prevState) override;
    void HandleLeaveState(eCreatureState nextState) override;
    void HandleUpdateLogic(float stepDeltaTime) override;
};
