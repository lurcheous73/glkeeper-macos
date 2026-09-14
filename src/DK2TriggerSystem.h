#pragma once

#include "ScenarioDefs.h"

#include <unordered_map>
#include <unordered_set>

class DK2TriggerSystem
{
public:
    bool Initialize(ScenarioDefinition& scenarioData, ePlayerID localPlayerId);
    void Shutdown();
    void Update(float deltaTime);
    bool IsActive() const { return mRootTriggerId != 0; }

private:
    const ScenarioTriggerNode* FindNode(unsigned short id) const;
    void ProcessChain(unsigned short id, std::unordered_set<unsigned short>& visitGuard);
    void ProcessGeneric(const ScenarioTriggerNode& node, std::unordered_set<unsigned short>& visitGuard);
    void ProcessAction(const ScenarioTriggerNode& node);
    bool EvaluateCondition(const ScenarioTriggerNode& node) const;
    bool Compare(int target, unsigned char comparison, int value) const;
    int TimerSeconds(unsigned char timerId) const;
    static unsigned int ReadLE32(const unsigned char* data);

private:
    ScenarioDefinition* mScenarioData = nullptr;
    ePlayerID mLocalPlayerId = ePlayerID_Null;
    unsigned short mRootTriggerId = 0;
    float mLevelStartTime = 0.0f;
    std::unordered_map<unsigned short, const ScenarioTriggerNode*> mNodes;
    std::unordered_set<unsigned short> mActivatedGenerics;
    std::unordered_set<unsigned short> mExecutedActions;
    std::unordered_map<unsigned short, bool> mConditionState;
    std::unordered_map<unsigned char, int> mFlags;
    std::unordered_map<unsigned char, float> mTimers;
};

extern DK2TriggerSystem gDK2TriggerSystem;
