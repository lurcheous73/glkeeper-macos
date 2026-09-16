#include "stdafx.h"
#include "FrontendController.h"
#include "GameWorld.h"
#include "GameMain.h"
#include "MapUtils.h"
#include "GameEventBus.h"
#include "GameSession.h"
#include "Scene.h"
#include "LevelsDatabase.h"
#include "UiCursor.h"
#include "RoomManager.h"
#include "GameObjectManager.h"

FrontendController::FrontendController()
    : mFrontendUi(*this)
{
}

void FrontendController::OnOpenSinglePlayerMenuSelected()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_SinglePlayer);
}

void FrontendController::OnMyPetDungeonMenuSelected()
{   
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_MyPetDungeon);
}

void FrontendController::OnMyPetDungeonMenuCancelled()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_Main);
    if (std::getenv("KEEPER_FRONTEND_TRACE"))
        std::fprintf(stderr, "DK2FRONT main_page_shown active=%d hierarchy=%d\n",
            mFrontendUi.IsActive() ? 1 : 0, mFrontendUi.IsHierarchyLoaded() ? 1 : 0);
    if (std::getenv("KEEPER_FRONTEND_TRACE"))
        std::fprintf(stderr, "DK2FRONT start uiActive=%d rooms=%d objects=%zu scene=%d\n",
            mFrontendUi.IsActive() ? 1 : 0, gRoomManager.GetRoomCount(),
            gGameObjectManager.GetObjects().size(), gScene.GetActiveSceneObjectCount());
}

void FrontendController::OnMyPetDungeonLevelSelect(const std::string& fileName)
{
    ScenarioLevelInfo levelInfo;
    if (gLevelsDatabase.GetLevelInfo(fileName, levelInfo))
    {
        mFrontendUi.ConfigureMissionBriefing(levelInfo);
        mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_MissionBriefing);
    }
    else
    {
        cxx_assert(false);
    }
}

void FrontendController::OnOpenSkirmishMenuSelected()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_SkirmishMaps);
}

void FrontendController::OnNewCampaignSelected()
{
    ScenarioLevelInfo levelInfo;
    if (gLevelsDatabase.GetLevelInfo("level1", levelInfo))
    {
        mFrontendUi.ConfigureMissionBriefing(levelInfo);
        mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_MissionBriefing);
    }
}

void FrontendController::OnSinglePlayerCancelled()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_Main);
}

void FrontendController::OnSkirmishMapSelectCancelled()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_SinglePlayer);
}

void FrontendController::OnSkirmishMapSelectConfirmed(const std::string& fileName)
{
    gGameEventBus.Send_StartScenarioRequest(fileName);  
}

void FrontendController::OnMissionBriefingCancelled(bool isMyPetDungeon)
{
    if (isMyPetDungeon)
    {
        mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_MyPetDungeon);
    }
    else
    {
        mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_SinglePlayer);
    }
}

void FrontendController::OnMissionBriefingConfirmed(const std::string& fileName)
{
    gGameEventBus.Send_StartScenarioRequest(fileName);  
}

void FrontendController::OnQuitGameConfirmed()
{
    gGameEventBus.Send_QuitGameRequest();
}

void FrontendController::OnQuitGameCancelled()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_Main);
}

void FrontendController::OnQuitGameSelected()
{
    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_QuitGame);
}

void FrontendController::OnSessionLoaded()
{
    Player& localPlayer = gGameSession.GetLocalPlayer();

    // setup camera
    mCameraController.ResetCamera();

    glm::vec3 cameraTileCoord = MapUtils::ComputeTileCenter(localPlayer.GetStartCameraTilePosition());

    cameraTileCoord[1] = 1.65f; // height
    cameraTileCoord[2] -= 0.5f;
    mCameraController.SetStartPosition(cameraTileCoord);
    mCameraController.CaptureCamera(&gScene.GetCamera());
    if (std::getenv("KEEPER_FRONTEND_TRACE"))
    {
        const Camera& cam = gScene.GetCamera();
        std::fprintf(stderr,
            "DK2FRONT load startTile=%d,%d camPos=%.3f,%.3f,%.3f forward=%.3f,%.3f,%.3f rooms=%d objects=%zu scene=%d\n",
            localPlayer.GetStartCameraTilePosition().x, localPlayer.GetStartCameraTilePosition().y,
            cam.mPosition.x, cam.mPosition.y, cam.mPosition.z,
            cam.mForward.x, cam.mForward.y, cam.mForward.z,
            gRoomManager.GetRoomCount(), gGameObjectManager.GetObjects().size(), gScene.GetActiveSceneObjectCount());
        for (float pitch : {0.0f, -15.0f, 15.0f, -30.0f})
        {
            for (float yaw : {0.0f, 90.0f, 180.0f, 270.0f})
            {
                Camera probe = cam;
                probe.SetRotation({pitch, yaw, 0.0f});
                probe.ComputeMatricesAndFrustum(gRenderDevice.GetViewport());
                SceneRenderLists lists;
                gScene.CollectObjectsForRender(probe, lists);
                std::fprintf(stderr, "DK2FRONT probe pitch=%.0f yaw=%.0f opaque=%zu translucent=%zu fwd=%.3f,%.3f,%.3f\n",
                    pitch, yaw,
                    lists.mListsPerPass[eRenderPass_Opaque].size(),
                    lists.mListsPerPass[eRenderPass_Translucent].size(),
                    probe.mForward.x, probe.mForward.y, probe.mForward.z);
            }
        }
    }
}

void FrontendController::OnSessionStart()
{
    if (mFrontendUi.IsActive())
        return;

    gUiCursor.StateOn(UiCursor::eCursorState_PointOnThing);

    // prepare screen

    const bool uiActivated = mFrontendUi.Activate();
    if (std::getenv("KEEPER_FRONTEND_TRACE"))
        std::fprintf(stderr, "DK2FRONT ui activate=%d active=%d hierarchy=%d\n",
            uiActivated ? 1 : 0, mFrontendUi.IsActive() ? 1 : 0, mFrontendUi.IsHierarchyLoaded() ? 1 : 0);

    cxx::temp_vector<ScenarioLevelInfo> mapsList;
    mapsList.reserve(32);
    gLevelsDatabase.EnumSkirmishLevels([&mapsList](const ScenarioLevelInfo& levelInfo)
        {
            mapsList.push_back(levelInfo);
        });
    mFrontendUi.ConfigureSkirmishMaps(mapsList);

    mapsList.clear();
    gLevelsDatabase.EnumMyPetDungeonLevels([&mapsList](const ScenarioLevelInfo& levelInfo)
        {
            mapsList.push_back(levelInfo);
        });
    mFrontendUi.ConfigureMyPetDungeonMaps(mapsList);

    mFrontendUi.ShowMenuPage(FrontendUi::eMenuPage_Main);
}

void FrontendController::OnSessionShutdown()
{
    mCameraController.ReleaseCamera();
    if (mFrontendUi.IsActive())
    {
        mFrontendUi.Deactivate();
        mFrontendUi.Cleanup();
    }

    gUiCursor.StateOff(UiCursor::eCursorState_PointOnThing);
}

void FrontendController::UpdateFrame(float deltaTime)
{
    mCameraController.UpdateFrame(deltaTime);
}

void FrontendController::UpdateLogic(float stepDeltaTime)
{

}

void FrontendController::InputEvent(MouseButtonInputEvent& inputEvent)
{
    mCameraController.InputEvent(inputEvent);
}

void FrontendController::InputEvent(KeyInputEvent& inputEvent)
{
    mCameraController.InputEvent(inputEvent);
}

void FrontendController::InputEvent(MouseMovedInputEvent& inputEvent)
{
    mCameraController.InputEvent(inputEvent);
}

void FrontendController::InputEvent(MouseScrollInputEvent& inputEvent)
{
    mCameraController.InputEvent(inputEvent);
}