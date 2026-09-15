#include "stdafx.h"
#include "DK2TriggerSystem.h"

#include "DK2SoundSystem.h"
#include "GameSession.h"
#include "TimeManager.h"

DK2TriggerSystem gDK2TriggerSystem;

unsigned int DK2TriggerSystem::ReadLE32(const unsigned char* data)
{
    return static_cast<unsigned int>(data[0]) |
        (static_cast<unsigned int>(data[1]) << 8) |
        (static_cast<unsigned int>(data[2]) << 16) |
        (static_cast<unsigned int>(data[3]) << 24);
}

bool DK2TriggerSystem::Initialize(ScenarioDefinition& scenarioData, ePlayerID localPlayerId)
{
    Shutdown();
    mScenarioData = &scenarioData;
    mLocalPlayerId = localPlayerId;
    mLevelStartTime = gTime.GetTime(eGameClock::Gametime);

    for (const ScenarioTriggerNode& node : scenarioData.mTriggers)
        mNodes[node.mId] = &node;

    if (PlayerDefinition* player = scenarioData.GetPlayerDefinition(localPlayerId))
        mRootTriggerId = static_cast<unsigned short>(player->mTriggerId);

    if (!mRootTriggerId && !scenarioData.mTriggers.empty())
    {
        for (const ScenarioTriggerNode& node : scenarioData.mTriggers)
        {
            if (node.mKind == eScenarioTrigger_Generic && (!mRootTriggerId || node.mId < mRootTriggerId))
                mRootTriggerId = node.mId;
        }
    }

    gConsole.LogMessage(eLogLevel_Info,
        "DK2 triggers: %zu nodes, local root %u", scenarioData.mTriggers.size(), mRootTriggerId);
    if (std::getenv("KEEPER_TRIGGER_TRACE"))
        std::fprintf(stderr, "DK2TRIGGER init nodes=%zu root=%u\n", scenarioData.mTriggers.size(), mRootTriggerId);
    return mRootTriggerId != 0;
}

void DK2TriggerSystem::Shutdown()
{
    mScenarioData = nullptr;
    mLocalPlayerId = ePlayerID_Null;
    mRootTriggerId = 0;
    mLevelStartTime = 0.0f;
    mNodes.clear();
    mActivatedGenerics.clear();
    mExecutedActions.clear();
    mConditionState.clear();
    mFlags.clear();
    mTimers.clear();
}

const ScenarioTriggerNode* DK2TriggerSystem::FindNode(unsigned short id) const
{
    auto it = mNodes.find(id);
    return (it == mNodes.end()) ? nullptr : it->second;
}

bool DK2TriggerSystem::Compare(int target, unsigned char comparison, int value) const
{
    switch (comparison)
    {
        case 0: return true;
        case 1: return target < value;
        case 2: return target <= value;
        case 3: return target == value;
        case 4: return target > value;
        case 5: return target >= value;
        case 6: return target != value;
        default: return false;
    }
}

int DK2TriggerSystem::TimerSeconds(unsigned char timerId) const
{
    auto it = mTimers.find(timerId);
    if (it == mTimers.end())
        return 0;
    return static_cast<int>(gTime.GetTime(eGameClock::Gametime) - it->second);
}

bool DK2TriggerSystem::EvaluateCondition(const ScenarioTriggerNode& node) const
{
    const unsigned char* data = node.mData;
    switch (node.mType)
    {
        case 0: // NONE
        case 72: // GUI_TRANSITION_ENDS - GLKeeper currently has no async transition gate
            return true;

        case 1: // FLAG
        {
            const int target = mFlags.count(data[1]) ? mFlags.at(data[1]) : 0;
            const int value = (data[2] == 1) ? static_cast<int>(ReadLE32(data + 4)) :
                (mFlags.count(data[3]) ? mFlags.at(data[3]) : 0);
            return Compare(target, data[0], value);
        }
        case 2: // TIMER
        {
            const int target = TimerSeconds(data[1]);
            const int value = (data[2] == 1) ? static_cast<int>(ReadLE32(data + 4)) : TimerSeconds(data[3]);
            return Compare(target, data[0], value);
        }
        case 30: // PLAYER_GOLD
        case 32: // PLAYER_MANA
        {
            const eGameResource resource = (node.mType == 30) ? eGameResource_Gold : eGameResource_Mana;
            const ePlayerID playerId = (data[3] == 0) ? mLocalPlayerId : static_cast<ePlayerID>(data[3]);
            const int target = static_cast<int>(gGameSession.GetPlayer(playerId).GetResourceAmount(resource));
            int value = static_cast<int>(ReadLE32(data + 4));
            if (data[2] != 1)
            {
                const ePlayerID otherId = (data[3] == 0) ? mLocalPlayerId : static_cast<ePlayerID>(data[3]);
                value = static_cast<int>(gGameSession.GetPlayer(otherId).GetResourceAmount(resource));
            }
            return Compare(target, data[0], value);
        }
        case 34: // LEVEL_TIME
        {
            const int elapsed = static_cast<int>(gTime.GetTime(eGameClock::Gametime) - mLevelStartTime);
            return Compare(elapsed, data[0], static_cast<int>(ReadLE32(data + 4)));
        }
        default:
            return false;
    }
}

void DK2TriggerSystem::ProcessAction(const ScenarioTriggerNode& node)
{
    if (mExecutedActions.count(node.mId))
        return;

    const unsigned char* data = node.mData;
    switch (node.mType)
    {
        case 7: // FLAG
        {
            int& flag = mFlags[data[0]];
            const unsigned char operation = data[1];
            const int value = static_cast<int>(ReadLE32(data + 4));
            if (operation & 0x08) flag = value;
            else if (operation & 0x10) flag += value;
            else if (operation & 0x20) flag -= value;
            break;
        }
        case 8: // INITIALIZE_TIMER
            mTimers[data[0]] = gTime.GetTime(eGameClock::Gametime);
            break;
        case 10: // WIN_GAME
            gGameSession.FinishSession(true);
            break;
        case 11: // LOSE_GAME
            gGameSession.FinishSession(false);
            break;
        case 24: // PLAY_SPEECH
        {
            const unsigned int speechId = ReadLE32(data);
            if (speechId)
            {
                gConsole.LogMessage(eLogLevel_Info, "DK2 trigger speech %u", speechId);
                if (std::getenv("KEEPER_TRIGGER_TRACE"))
                    std::fprintf(stderr, "DK2TRIGGER speech id=%u action=%u\n", speechId, node.mId);
                gDK2SoundSystem.QueueSpeech("speech_mentor", speechId, 1.0f);
            }
            break;
        }
        case 34: // SET_MUSIC_LEVEL
            gConsole.LogMessage(eLogLevel_Debug, "DK2 trigger music level %u", ReadLE32(data));
            break;
        default:
            break;
    }
    mExecutedActions.insert(node.mId);
}

void DK2TriggerSystem::ProcessGeneric(const ScenarioTriggerNode& node,
    std::unordered_set<unsigned short>& visitGuard)
{
    const bool active = EvaluateCondition(node);
    const bool wasActive = mConditionState[node.mId];
    mConditionState[node.mId] = active;

    if (active && (!wasActive || mActivatedGenerics.count(node.mId)))
    {
        if (!mActivatedGenerics.count(node.mId) && std::getenv("KEEPER_TRIGGER_TRACE"))
            std::fprintf(stderr, "DK2TRIGGER activate id=%u type=%u child=%u\n", node.mId, node.mType, node.mChildId);
        mActivatedGenerics.insert(node.mId);
    }

    if (mActivatedGenerics.count(node.mId) && node.mChildId)
        ProcessChain(node.mChildId, visitGuard);
}

void DK2TriggerSystem::ProcessChain(unsigned short id, std::unordered_set<unsigned short>& visitGuard)
{
    unsigned short current = id;
    while (current)
    {
        if (!visitGuard.insert(current).second)
            return;
        const ScenarioTriggerNode* node = FindNode(current);
        if (!node)
            return;

        if (node->mKind == eScenarioTrigger_Action)
            ProcessAction(*node);
        else
            ProcessGeneric(*node, visitGuard);

        current = node->mNextId;
    }
}

void DK2TriggerSystem::Update(float deltaTime)
{
    (void)deltaTime;
    if (!mRootTriggerId || !mScenarioData)
        return;
    std::unordered_set<unsigned short> visitGuard;
    ProcessChain(mRootTriggerId, visitGuard);
}
