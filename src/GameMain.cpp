#include "stdafx.h"
#include "GameMain.h"
#include "GameRenderManager.h"
#include "UiManager.h"
#include "DK2AssetLoader.h"
#include "DK2EngineTextures.h"
#include "DK2SoundBank.h"
#include "ShadersManager.h"
#include "TextureManager.h"
#include "MeshAssetManager.h"
#include "FontManager.h"
#include "GameWorld.h"
#include "FrameMemoryManager.h"
#include "Version.h"
#include "TextManager.h"
#include "GameEventBus.h"
#include "GameSession.h"
#include "LevelsDatabase.h"
#include "UiCursor.h"
#ifdef __APPLE__
#include "MacMoviePlayer.h"
#endif

//////////////////////////////////////////////////////////////////////////

GameMain gGame;

//////////////////////////////////////////////////////////////////////////

bool GameMain::Initialize()
{
    gDebug.Initialize();

    if (!gConsole.Initialize())
    {
        cxx_assert(false);
    }

    gConsole.LogMessage(eLogLevel_Info, "Game initialization");
    gConsole.LogMessage(eLogLevel_Info, "Version: %s", GAME_VERSION_STRING);

    if (!gFrameMemoryManager.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Could not initialize frame memory manager");
        return false;
    }

    if (!gTime.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Could not initialize time manager");
        return false;
    }

    if (!gFiles.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Error, "Cannot initialize filesystem");
        return false;
    }

    if (!gGameProfile.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Error, "Cannot init game profile manager");
        return false;
    }

    const GameProfile::UserSettings& userSettings = gGameProfile.GetUserSettings();

    gFiles.InitTextLocation(userSettings.mLanguage);

    if (!gRenderDevice.Initialize(userSettings.mScreenResolution, userSettings.mEnableFullscreen, userSettings.mEnableVSync))
    {
        gConsole.LogMessage(eLogLevel_Error, "Cannot initialize render device");
        return false;
    }

#ifdef __APPLE__
    // GLKeeper renders the original Dungeon Keeper cursor itself. Showing the
    // macOS hardware arrow at the same time produces two independently drawn
    // pointers and makes the cursor appear out of sync. Keep one authoritative
    // cursor on macOS until a native themed hardware cursor implementation exists.
    gRenderDevice.EnableHwCursor(false);
#else
    gRenderDevice.EnableHwCursor(userSettings.mEnableHwCursor);
#endif

    if (!gDK2AssetLoader.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot initialize game assets data provider");
        return false;
    }

    gShadersManager.Initialize();
    gTextureManager.Initialize();
    gMeshAssetManager.Initialize();
    gFontManager.Initialize();

    if (!gUiManager.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot initialize ui system");
        return false;
    }

    if (!gGameRenderer.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot initialize render engine");
        return false;
    }

    // second phase initialization
    SetGamestate(eGamestate::TitleScreen);
    if (mTitleScreen.Activate())
    {
        MiniUpdateFrame();
    }

    if (!gTexts.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot initialize texts");
    }

    if (!gLevelsDatabase.Initialize())
    {
        gConsole.LogMessage(eLogLevel_Error, "Cannot initialize levels database");
    }

    mTitleScreen.Deactivate();
    SetGamestate(eGamestate::None);
    // subscribe to events
    gGameEventBus.Subscribe(eGameEvent_StartScenarioRequest, this);
    gGameEventBus.Subscribe(eGameEvent_QuitGameRequest, this);
    gGameEventBus.Subscribe(eGameEvent_ReturnToFrontendRequest, this);

    Random::SetLocalThreadSeed(12345); // todo: init seed properly

    gConsole.LogMessage(eLogLevel_Info, "Ready");
    return true;
}

void GameMain::Shutdown()
{
    gConsole.LogMessage(eLogLevel_Info, "Game shutdown");

    SetGamestate(eGamestate::None);
    gGameSession.ShutdownSession();

    //
    if (mTestScreen.IsActive())
    {
        mTestScreen.Deactivate();
        mTestScreen.Cleanup();
    }
    //

    gTexts.Shutdown();
    gGameRenderer.Shutdown();
    gUiManager.Shutdown();
    gFontManager.Shutdown();
    gMeshAssetManager.Shutdown();
    gTextureManager.Shutdown();
    gShadersManager.Shutdown();
    gDK2AssetLoader.Shutdown();
    gRenderDevice.Shutdown();
    gLevelsDatabase.Shutdown();
    gFiles.Shutdown();
    gConsole.Shutdown();
    gDebug.Shutdown();
    gTime.Shutdown();
    gFrameMemoryManager.Shutdown();
}

void GameMain::Run()
{
    mQuitRequested = false;

    if (!Initialize())
    {
        Terminate();
    }

    if (GetCurrentGamestate() == eGamestate::None)
    {
        if (!StartFrontend())
        {
            Terminate();
        }
    }

    if (GetCurrentGamestate() == eGamestate::None)
    {
        mTestScreen.Activate();
    }

    for (; !mQuitRequested; )
    {
        UpdateFrame();
    }

    Shutdown();
}

void GameMain::Terminate()
{    
    Shutdown(); // leave gracefully
    exit(EXIT_FAILURE);
}

void GameMain::RequestQuit()
{
    mQuitRequested = true;
}

void GameMain::InputEvent(KeyInputEvent& inputEvent)
{
    gInputs.SetKeyState(inputEvent.mKeycode, inputEvent.mPressed);
    gUiManager.InputEvent(inputEvent);

    if (inputEvent.IsKeyPressed(KEYCODE_TILDE))
    {
        mConsoleScreen.ToggleConsole();
        inputEvent.SetConsumed();
    }

    // toggle fullscreen
    if (inputEvent.IsKeyPressed(KEYCODE_ENTER) && inputEvent.HasModifiers(KEYMOD_CTRL))
    {
        // todo:
    }

    if (!inputEvent.mConsumed && gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.InputEvent(inputEvent);
    }
}

void GameMain::InputEvent(KeyCharEvent& inputEvent)
{
    gUiManager.InputEvent(inputEvent);
}

void GameMain::InputEvent(MouseButtonInputEvent& inputEvent)
{
    gInputs.SetMouseButtonState(inputEvent.mButton, inputEvent.mPressed);

    inputEvent.mMousePosition = gInputs.GetMousePosition();
    gUiManager.InputEvent(inputEvent);

    if (!inputEvent.mConsumed && gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.InputEvent(inputEvent);
    }
}

void GameMain::InputEvent(MouseMovedInputEvent& inputEvent)
{
    inputEvent.mDelta = inputEvent.mMousePosition - gInputs.GetMousePosition();

    gInputs.SetMousePosition(inputEvent.mMousePosition);
    gUiManager.InputEvent(inputEvent);

    if (!inputEvent.mConsumed && gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.InputEvent(inputEvent);
    }
}

void GameMain::InputEvent(MouseScrollInputEvent& inputEvent)
{
    // plug in cursor position
    inputEvent.mMousePosition = gInputs.GetMousePosition();
    gUiManager.InputEvent(inputEvent);

    if (!inputEvent.mConsumed && gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.InputEvent(inputEvent);
    }
}

void GameMain::UpdateFrame()
{
    gFrameMemoryManager.ResetFrameMemory();
    gTime.UpdateFrame();

    float uiDeltaTime = gTime.GetFrameDelta(eGameClock::Ui);
    gUiManager.UpdateFrame(uiDeltaTime);

    // variable delta time frame update
    {
        float gameDeltaTime = gTime.GetFrameDelta(eGameClock::Gametime);
        if (gGameSession.IsInState(eGameSessionState_Active))
        {
            gGameSession.UpdateFrame(gameDeltaTime);
        }
    }

    // fixed physics update
    for (int istep = 0, NumSteps = gTime.GetFixedSteps(eFixedClock::GamePhysics); istep < NumSteps; ++istep)
    {
        float stepTime = gTime.GetFixedDelta(eFixedClock::GamePhysics);
        UpdatePhysics(stepTime);
    }

    // fixed logic update
    for (int istep = 0, NumSteps = gTime.GetFixedSteps(eFixedClock::GameLogic); istep < NumSteps; ++istep)
    {
        float stepTime = gTime.GetFixedDelta(eFixedClock::GameLogic);
        UpdateLogic(stepTime);
    }

    gGameEventBus.DispatchEvents();

    gGameRenderer.RenderFrame();

    gDebug.UpdateFrame();
}

void GameMain::OpenConsoleScreen()
{
    if (!mConsoleScreen.IsActive())
    {
        mConsoleScreen.ToggleConsole();
    }
}

void GameMain::HideConsoleScreen()
{
    if (mConsoleScreen.IsActive())
    {
        mConsoleScreen.ToggleConsole();
    }
}

void GameMain::ScreenSizeChanged(const Point2D& screenSize)
{
    gUiManager.ScreenSizeChanged(screenSize);
}

void GameMain::UpdateLoadingProgress(float progress)
{
    if (!mLoadingScreen.IsActive())
    {
        mLoadingScreen.StartLoading();
    }
    mLoadingScreen.UpdateLoadingProgress(progress);
    MiniUpdateFrame();
}

void GameMain::HandleGameEvent(const GameEvent& eventData)
{
    if (eventData.mEventId == eGameEvent_QuitGameRequest)
    {
        RequestQuit();
        return;
    }

    if (eventData.mEventId == eGameEvent_StartScenarioRequest)
    {
        if (!StartScenario(eventData.mStartScenarioRequest.mScenarioName))
        {
            Terminate();
        }
        return;
    }

    if (eventData.mEventId == eGameEvent_ReturnToFrontendRequest)
    {
        if ((mCurrentGamestate == eGamestate::Gameplay) && !StartFrontend())
        {
            Terminate();
        }
        return;
    }
}

void GameMain::StartCampaignScenario()
{
    // todo
}

void GameMain::StartSkirmishScenario()
{
    // todo
}

void GameMain::StartMPDScenario()
{
    // todo
}

bool GameMain::StartScenario(const std::string& scenarioName)
{
    mLoadingScreen.StartLoading();

    gGameSession.ShutdownSession();

    SetGamestate(eGamestate::LoadingGameplay);

    UpdateLoadingProgress(0.0f);

    GameSessionStartupParams sessionParams;
    sessionParams.mSessionType = eGameSession_Level;
    sessionParams.mLevelName = scenarioName;

    bool isSuccess = gGameSession.Preload(*this, sessionParams);

    UpdateLoadingProgress(1.0f);

    mLoadingScreen.FinishLoading();

    if (isSuccess)
    {
        cxx_assert(gGameSession.IsInState(eGameSessionState_Loaded));
        SetGamestate(eGamestate::Gameplay);

        gGameSession.StartSession();
        MiniUpdateFrame();
    }
    else
    {
        SetGamestate(eGamestate::None);

        gGameSession.ShutdownSession();
    }
    return isSuccess;
}

bool GameMain::StartFrontend()
{
    mLoadingScreen.StartLoading();

    gGameSession.ShutdownSession();

    SetGamestate(eGamestate::LoadingFrontend);

    UpdateLoadingProgress(0.0f);

    GameSessionStartupParams sessionParams;
    sessionParams.mSessionType = eGameSession_Frontend;
    sessionParams.mLevelName = "FrontEnd3DLevel";

    bool isSuccess = gGameSession.Preload(*this, sessionParams);

    UpdateLoadingProgress(1.0f);

    mLoadingScreen.FinishLoading();

    if (isSuccess)
    {
        cxx_assert(gGameSession.IsInState(eGameSessionState_Loaded));
        SetGamestate(eGamestate::Frontend);

        gGameSession.StartSession();
        MiniUpdateFrame();
    }
    else
    {
        SetGamestate(eGamestate::None);

        gGameSession.ShutdownSession();
    }
    return isSuccess;
}

void GameMain::MiniUpdateFrame()
{
    gFrameMemoryManager.ResetFrameMemory();
    gTime.UpdateFrame();

    float uiDeltaTime = gTime.GetFrameDelta(eGameClock::Ui);
    gUiManager.UpdateFrame(uiDeltaTime);
    gGameRenderer.RenderFrame();
}

void GameMain::SetGamestate(eGamestate newGamestate)
{
    mCurrentGamestate = newGamestate;
    // setup cursor
    bool showCursor = (mCurrentGamestate == eGamestate::Gameplay) || (mCurrentGamestate == eGamestate::Frontend);
    if (showCursor)
    {
        gUiCursor.StateOff(UiCursor::eCursorState_Hidden);
    }
    else
    {
        gUiCursor.StateOn(UiCursor::eCursorState_Hidden);
    }
}

void GameMain::UpdateLogic(float stepDeltaTime)
{
    if (gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.UpdateLogic(stepDeltaTime);
    }
}

void GameMain::UpdatePhysics(float stepDeltaTime)
{
    if (gGameSession.IsInState(eGameSessionState_Active))
    {
        gGameSession.UpdatePhysics(stepDeltaTime);
    }
}

//////////////////////////////////////////////////////////////////////////

static int ExportSoundBanks(const char* destination)
{
    if (!destination || !destination[0])
        return EXIT_FAILURE;

    if (!gFiles.Initialize())
        return EXIT_FAILURE;

    std::string soundRoot;
    if (!gFiles.PathToDirectory("Data/Sound/Sfx", soundRoot))
    {
        gFiles.Shutdown();
        return EXIT_FAILURE;
    }

    DK2SoundExportStats stats;
    const bool success = DK2ExportSoundBanks(soundRoot, destination, stats);
    std::printf("Exported %zu sounds from %zu DK2 banks to %s "
        "(%zu entries, %zu skipped, %zu failed)\n",
        stats.mExported, stats.mBanks, destination, stats.mEntries,
        stats.mSkipped, stats.mFailed);
    gFiles.Shutdown();
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int ExportEngineTextures(const char* destination)
{
    if (!destination || !destination[0])
        return EXIT_FAILURE;

    if (!gFiles.Initialize())
        return EXIT_FAILURE;

    std::string cacheLocation;
    if (!gFiles.LocateEngineTexturesCache(cacheLocation))
    {
        gFiles.Shutdown();
        return EXIT_FAILURE;
    }

    DK2EngineTexturesCache cache;
    if (!cache.ScanDungeonKeeperTexturesCache(cacheLocation))
    {
        gFiles.Shutdown();
        return EXIT_FAILURE;
    }

    const std::filesystem::path outputRoot(destination);
    std::error_code ec;
    std::filesystem::create_directories(outputRoot, ec);
    if (ec)
    {
        cache.Shutdown();
        gFiles.Shutdown();
        return EXIT_FAILURE;
    }

    size_t exported = 0;
    size_t failed = 0;
    const size_t textureCount = cache.GetTexturesCount();
    for (size_t textureIndex = 0; textureIndex < textureCount; ++textureIndex)
    {
        std::string textureName;
        if (!cache.GetTextureNameByID(static_cast<DK2EngineTextureID>(textureIndex), textureName))
        {
            ++failed;
            continue;
        }
        std::replace(textureName.begin(), textureName.end(), '\\', '/');
        const std::filesystem::path outputPath = outputRoot / (textureName + ".png");
        std::filesystem::create_directories(outputPath.parent_path(), ec);
        if (ec)
        {
            ++failed;
            ec.clear();
            continue;
        }
        BitmapImage image;
        if (!cache.ExtractTexture(static_cast<DK2EngineTextureID>(textureIndex), image) ||
            !image.SaveToFile(outputPath.string()))
        {
            ++failed;
            continue;
        }
        ++exported;
    }

    std::printf("Exported %zu/%zu DK2 textures to %s (%zu failed)\n",
        exported, textureCount, outputRoot.string().c_str(), failed);
    cache.Shutdown();
    gFiles.Shutdown();
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef __APPLE__
static void PlayMacStartupMovies(bool enhanced)
{
    const char* rootEnv = std::getenv("KEEPER_DATA_ROOT");
    if (!rootEnv || !rootEnv[0])
        return;

    const std::filesystem::path root(rootEnv);
    const std::filesystem::path original = root / "NativeData/original/video";
    const std::filesystem::path enhancedDir = root / "NativeData/enhanced/video";
    const char* startupMovies[] = {"BullfrogIntro.mp4", "INTRO.mp4"};

    for (const char* name : startupMovies)
    {
        std::filesystem::path movie;
        if (enhanced && std::filesystem::is_regular_file(enhancedDir / name))
            movie = enhancedDir / name;
        else if (std::filesystem::is_regular_file(original / name))
            movie = original / name;
        if (!movie.empty())
            MacPlayMovie(movie.string().c_str());
    }
}
#endif

//////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    bool enhancedMode = false;
    bool skipIntro = false;
    const char* exportTexturesPath = nullptr;
    const char* exportSoundsPath = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--enhanced") == 0)
        {
            enhancedMode = true;
#if defined(_WIN32)
            _putenv_s("KEEPER_ENHANCED", "1");
#else
            setenv("KEEPER_ENHANCED", "1", 1);
#endif
        }
        else if (std::strcmp(argv[i], "--original") == 0)
        {
            enhancedMode = false;
#if defined(_WIN32)
            _putenv_s("KEEPER_ENHANCED", "0");
#else
            unsetenv("KEEPER_ENHANCED");
#endif
        }
        else if (std::strcmp(argv[i], "--export-textures") == 0 && (i + 1) < argc)
        {
            exportTexturesPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--export-sounds") == 0 && (i + 1) < argc)
        {
            exportSoundsPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--nointro") == 0)
        {
            skipIntro = true;
        }
    }

    if (exportTexturesPath)
        return ExportEngineTextures(exportTexturesPath);

    if (exportSoundsPath)
        return ExportSoundBanks(exportSoundsPath);

#ifdef __APPLE__
    if (!skipIntro)
        PlayMacStartupMovies(enhancedMode);
#endif

    //// simulate memory leak
    //new int;

    gGame.Run();
    return 0;
}

//////////////////////////////////////////////////////////////////////////
