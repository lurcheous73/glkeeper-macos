#include "stdafx.h"
#include "DK2AssetLoader.h"
#include "DK2WADArchive.h"
#include "DK2EngineTextures.h"

//////////////////////////////////////////////////////////////////////////

DK2AssetLoader gDK2AssetLoader;

static bool IsEnhancedAssetMode()
{
    const char* mode = std::getenv("KEEPER_ENHANCED");
    return mode && std::strcmp(mode, "1") == 0;
}

//////////////////////////////////////////////////////////////////////////
bool DK2AssetLoader::Initialize()
{
    std::string texturesCacheLocation;
    if (!gFiles.LocateEngineTexturesCache(texturesCacheLocation))
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot locate engine textures cache");
        return false;
    } 

    gConsole.LogMessage(eLogLevel_Info, "Engine textures cache location '%s'", texturesCacheLocation.c_str());
    if (!mEngineTexturesCache.ScanDungeonKeeperTexturesCache(texturesCacheLocation))
        return false;

    if (!LoadWADs())
        return false;

    gConsole.LogMessage(eLogLevel_Info, "Resource data provider is initialized");
    return true;
}

void DK2AssetLoader::Shutdown()
{
    FreeWADs();

    mEngineTexturesCache.Shutdown();
}

bool DK2AssetLoader::LoadWADs()
{
    const static std::string archiveNamesList[] = 
    { 
        "Meshes.WAD", 
        "EngineTextures.WAD", 
        "FrontEnd.WAD", 
        "Paths.WAD", 
        "Sprite.WAD" 
    };

    cxx_assert(mArchives.empty());
    for (const std::string& rollerArchiveName: archiveNamesList)
    {
        DK2WADArchive* archiveInstance = new DK2WADArchive (rollerArchiveName);

        std::string archiveFilePath;
        if (!gFiles.LocateWAD(rollerArchiveName, archiveFilePath) || !archiveInstance->OpenArchive(archiveFilePath))
        {
            gConsole.LogMessage(eLogLevel_Warning, "Cannot open WAD archive '%s'", rollerArchiveName.c_str());
        }

        mArchives.push_back(archiveInstance);
    }
    return !mArchives.empty();
}

void DK2AssetLoader::FreeWADs()
{
    for (DK2WADArchive* rollerArchive: mArchives)
    {
        SafeDelete(rollerArchive);
    }
    mArchives.clear();
}

bool DK2AssetLoader::LoadImageData(const std::string& theTextureName, BitmapImage& outputBitmap)
{
    // 0. Enhanced mode can overlay modern PNG assets without modifying DK2 data.
    if (IsEnhancedAssetMode())
    {
        std::string overrideName = theTextureName;
        std::replace(overrideName.begin(), overrideName.end(), '\\', '/');
        const std::size_t slashPos = overrideName.find_last_of('/');
        const std::size_t dotPos = overrideName.find_last_of('.');
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
            overrideName.erase(dotPos);

        std::string enhancedPath;
        if (gFiles.PathToFile("enhanced/textures/" + overrideName + ".png", enhancedPath))
        {
            if (outputBitmap.LoadFromFile(enhancedPath))
            {
                gConsole.LogMessage(eLogLevel_Debug, "Using enhanced texture override '%s'", enhancedPath.c_str());
                return true;
            }
            gConsole.LogMessage(eLogLevel_Warning, "Cannot load enhanced texture override '%s'", enhancedPath.c_str());
        }
    }

    // 1. search in engine textures cache
    DK2EngineTextureID textureID;
    if (mEngineTexturesCache.FindTextureByName(theTextureName, textureID))
    {
        if (!mEngineTexturesCache.ExtractTexture(textureID, outputBitmap))
        {
            gConsole.LogMessage(eLogLevel_Warning, "Cannot extract texture '%s'", theTextureName.c_str());
            return false;
        }
        return true;
    }
    // 2. search in wads
    if (LoadFromWADs(theTextureName, mReadBuffer))
    {
        bool isLoaded = false;
        if (cxx::ends_with_icase(theTextureName, ".444"))
        {
            isLoaded = mEngineTexturesCache.Decompress444(mReadBuffer, outputBitmap);
        }
        else
        {
            isLoaded = outputBitmap.LoadFromMemory(mReadBuffer.data(), mReadBuffer.size());
        }

        if (!isLoaded)
        {
            gConsole.LogMessage(eLogLevel_Warning, "Cannot extract texture '%s'", theTextureName.c_str());
            return false;
        }
        return true;
    }

    gConsole.LogMessage(eLogLevel_Warning, "Cannot load texture '%s'", theTextureName.c_str());
    return false;
}

bool DK2AssetLoader::IsImageExists(const std::string& imageName) const
{
    if (IsEnhancedAssetMode())
    {
        std::string overrideName = imageName;
        std::replace(overrideName.begin(), overrideName.end(), '\\', '/');
        const std::size_t slashPos = overrideName.find_last_of('/');
        const std::size_t dotPos = overrideName.find_last_of('.');
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos))
            overrideName.erase(dotPos);

        std::string enhancedPath;
        if (gFiles.PathToFile("enhanced/textures/" + overrideName + ".png", enhancedPath))
            return true;
    }

    // 1. search in engine textures cache
    DK2EngineTextureID textureID;
    if (mEngineTexturesCache.FindTextureByName(imageName, textureID))
        return true;

    // 2. search in wads
    for (DK2WADArchive* wad_archive: mArchives)
    {
        DK2WADArchiveEntryID entry_id;
        if (wad_archive->FindEntryByName(imageName, entry_id))
            return true;
    }
    return false;
}

bool DK2AssetLoader::LoadKMFModelData(const std::string& theMeshName, DK2KMFModel& outputModel)
{
    if (!LoadFromWADs(theMeshName, mReadBuffer))
        return false;

    cxx::memory_istream memorystream((char*)&mReadBuffer[0], (char*) &mReadBuffer[0] + mReadBuffer.size());
    std::istream instream(&memorystream);
    return DK2_KMF_LoadFromStream(instream, outputModel);
}

bool DK2AssetLoader::LoadBF4FontData(const std::string& theFontName, DK2FontDesc& outputFontData)
{
    std::string resourcePath;
    if (!gFiles.LocateFont(theFontName, resourcePath))
    {
        gConsole.LogMessage(eLogLevel_Warning, "Font '%s' is not found", theFontName.c_str());
        return false;
    }
    std::ifstream fileStream(resourcePath, std::ios::in | std::ios::binary);
    if (fileStream.is_open())
        return DK2_BF4_LoadFromStream(fileStream, outputFontData);

    return false;
}

bool DK2AssetLoader::GetArchiveNames(std::vector<std::string>& namesList) const
{
    namesList.clear();
    namesList.reserve(mArchives.size());
    for (DK2WADArchive* rollerArchive: mArchives)
    {
        namesList.push_back(rollerArchive->GetArchiveName());
    }
    return !namesList.empty();
}

bool DK2AssetLoader::GetArchiveEntryNames(const std::string& archiveName, std::vector<std::string>& entryNames) const
{
    entryNames.clear();
    if (DK2WADArchive* archive = GetArchiveByName(archiveName))
    {
        return archive->GetEntryNames(entryNames);
    }
    return false;
}

bool DK2AssetLoader::LoadFromWADs(const std::string& theResourceName, ByteArray& theOutputData)
{
    theOutputData.clear();
    for (DK2WADArchive* wad_archive: mArchives)
    {
        DK2WADArchiveEntryID entry_id;
        if (!wad_archive->FindEntryByName(theResourceName, entry_id))
        {
            continue;
        }
        // try to load found entry
        return wad_archive->ExtractEntryData(entry_id, theOutputData);
    }
    return false;
}

DK2WADArchive* DK2AssetLoader::GetArchiveByName(const std::string& archiveName) const
{
    for (DK2WADArchive* rollerArchive: mArchives)
    {
        if (rollerArchive->GetArchiveName() == archiveName)
            return rollerArchive;
    }
    return nullptr;
}
