#include "stdafx.h"
#include "FileSystem.h"
#include <filesystem>
#include "SimplePool.h"

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif

//////////////////////////////////////////////////////////////////////////

namespace sys = std::filesystem;

namespace
{
std::string NormalizeResourcePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.rfind("./", 0) == 0)
        path.erase(0, 2);
    return path;
}
}

//////////////////////////////////////////////////////////////////////////

FileSystem gFiles;

//////////////////////////////////////////////////////////////////////////

bool FileSystem::Initialize()
{
#ifdef _WIN32
    char buffer[MAX_PATH + 1] = {};
    if (::GetModuleFileNameA(NULL, buffer, MAX_PATH) == 0)
    {
        gConsole.LogMessage(eLogLevel_Warning, "GetModuleFileNameA failed");
        return false;
    }
    mExecutablePath.assign(buffer);
#elif defined(__APPLE__)
    uint32_t pathSize = PATH_MAX;
    std::vector<char> buffer(pathSize + 1, 0);
    if (_NSGetExecutablePath(buffer.data(), &pathSize) != 0)
    {
        buffer.assign(pathSize + 1, 0);
        if (_NSGetExecutablePath(buffer.data(), &pathSize) != 0)
            return false;
    }
    mExecutablePath = std::filesystem::weakly_canonical(buffer.data()).string();
#else
    mExecutablePath = (std::filesystem::current_path() / "GLKeeper").string();
#endif
    mWorkingDirectoryPath = std::filesystem::path(mExecutablePath).parent_path().string();

    // The native Mac launcher supplies a stable game-data root. This keeps the
    // engine independent of where the .app bundle itself is installed.
    const char* explicitDataRoot = std::getenv("KEEPER_DATA_ROOT");
    if (explicitDataRoot && explicitDataRoot[0] != '\0')
    {
        const std::string root = explicitDataRoot;
        AddSearchPlace(root + "/GLKeeper/data");
        AddSearchPlace(root + "/GLKeeper/dungeon-keeper");
        AddSearchPlace(root + "/NativeData/original");
        AddSearchPlace(root + "/NativeData");
    }
    else
    {
        const std::string debugDataPath = FSGetParentFolder(
            FSGetParentFolder(mWorkingDirectoryPath));
        AddSearchPlace(debugDataPath + "/data");
        AddSearchPlace(debugDataPath + "/dungeon-keeper");
        const std::string nativeDataPath = FSGetParentFolder(debugDataPath) + "/NativeData";
        AddSearchPlace(nativeDataPath + "/original");
        AddSearchPlace(nativeDataPath);
        AddSearchPlace(mWorkingDirectoryPath + "/data");
    }

    InitTextLocation("");
    return true;
}

void FileSystem::Shutdown()
{
    mSearchPlaces.clear();
    mExecutablePath.clear();
    mWorkingDirectoryPath.clear();
}

void FileSystem::InitTextLocation(std::string_view folderName)
{
    const char* textLocationPrefix = "Data/Text/";

    if (!folderName.empty())
    {
        mTextFolder = textLocationPrefix;
        mTextFolder.append(folderName);

        // ensure valid path
        std::string tempPath;
        if (LocateTextTableFile("Text.str", tempPath))
            return;
    }

    // fallback to default
    mTextFolder = textLocationPrefix;
    mTextFolder.append("Default");
}

bool FileSystem::LocateFont(const std::string& resourceName, std::string& resourcePath) const
{
    std::string subpath = cxx::va("fonts/%s", resourceName.c_str());
    if (PathToFile(subpath, resourcePath))
        return true;

    subpath = cxx::va("%s/%s", mTextFolder.c_str(), resourceName.c_str());
    return PathToFile(subpath, resourcePath);
}

bool FileSystem::LocateShader(const std::string& resourceName, std::string& resourcePath) const
{
    std::string subpath = cxx::va("shaders/%s", resourceName.c_str());
    return PathToFile(subpath, resourcePath);
}

bool FileSystem::LocateMapData(const std::string& resourceName, std::string& resourcePath) const
{
    std::string subpath;
    if (cxx::starts_with_icase(resourceName, "Data\\editor\\") ||
        cxx::starts_with_icase(resourceName, "Data/editor/"))
    {
        subpath = resourceName;
    }
    else
    {
        subpath = cxx::va("Data/editor/maps/%s", resourceName.c_str());
    }

    if (PathToFile(subpath, resourcePath))
        return true;

    subpath.append(".kwd");
    return PathToFile(subpath, resourcePath);
}

bool FileSystem::EnumMapFiles(EnumFilesCallback callback) const
{
    cxx_assert(callback);
    int filesFound = 0;
    for (const std::string& searchPlace : mSearchPlaces)
    {
        const sys::path mapsDirectory = sys::path {searchPlace} / "Data/editor/maps";
        const sys::path mapsExtension = ".kwd";
        if (!sys::exists(mapsDirectory))
            continue;

        sys::directory_iterator iter_directory_end;
        for (sys::directory_iterator iter_directory(mapsDirectory); 
            iter_directory != iter_directory_end; ++iter_directory)
        {
            if (!sys::is_regular_file(iter_directory->status()))
                continue;

            const sys::path& currentFile = iter_directory->path();
            if (!currentFile.has_extension() || mapsExtension != currentFile.extension())
                continue;

            callback(currentFile.stem().generic_string());
            ++filesFound;
        }
    }
    return filesFound > 0;
}

void FileSystem::AddSearchPlace(const std::string& pathToPlace)
{
    const std::string normalizedPath = NormalizeResourcePath(pathToPlace);
    const std::string lowerPathToPlace = cxx::lower_string(normalizedPath);
    auto foundIterator = std::find_if(
        mSearchPlaces.begin(),
        mSearchPlaces.end(), [&lowerPathToPlace](const std::string& stringArg)
        {
            return cxx::lower_string(stringArg) == lowerPathToPlace;
        });

    if (foundIterator == mSearchPlaces.end())
        mSearchPlaces.emplace_front(normalizedPath);
}

cxx::uniqueptr<BinaryInputStream> FileSystem::OpenBinaryFile(const std::string& fileName) const
{
    static SimplePool<FileInputStream> fileInputStreamsPool;

    std::string pathToFile;

    if (PathToFile(fileName, pathToFile))
    {
        FileInputStream* fileStream = fileInputStreamsPool.Acquire();
        cxx_assert(fileStream);

        cxx::uniqueptr<BinaryInputStream> binaryStream (fileStream, 
            [](BinaryInputStream* inputStream)
            {
                if (FileInputStream* fileStream = static_cast<FileInputStream*>(inputStream))
                {
                    fileStream->CloseFileStream();
                    fileInputStreamsPool.Return(fileStream);
                }
            });

        if (fileStream->OpenFileStream(pathToFile))
            return std::move(binaryStream);

        gConsole.LogMessage(eLogLevel_Warning, "Cannot open binary file '%s'", fileName.c_str());
        return nullptr;
    }

    gConsole.LogMessage(eLogLevel_Warning, "File not found '%s'", fileName.c_str());
    return nullptr;
}

bool FileSystem::PathToFile(const std::string& fileName, std::string& fullPath) const
{
    fullPath.clear();
    const std::string normalizedName = NormalizeResourcePath(fileName);

    for (const std::string& searchPlace : mSearchPlaces)
    {
        const sys::path pathto = sys::path {searchPlace} / normalizedName;
        if (sys::is_regular_file(pathto))
        {
            fullPath = pathto.generic_string();
            return true;
        }
    }
    return false;
}

bool FileSystem::PathToFileExists(const std::string& theName) const
{
    const std::string normalizedName = NormalizeResourcePath(theName);
    for (const std::string& searchPlace : mSearchPlaces)
    {
        const sys::path pathto = sys::path {searchPlace} / normalizedName;
        if (sys::is_regular_file(pathto))
            return true;
    }
    return false;
}

bool FileSystem::PathToDirectory(const std::string& theName, std::string& fullPath) const
{
    fullPath.clear();
    const std::string normalizedName = NormalizeResourcePath(theName);

    for (const std::string& searchPlace : mSearchPlaces)
    {
        const sys::path pathto = sys::path {searchPlace} / normalizedName;
        if (sys::is_directory(pathto))
        {
            fullPath = pathto.generic_string();
            return true;
        }
    } 
    return false;
}

void FSSplitPath(const std::string& filePath, 
    std::string* parentFolderPath, 
    std::string* fileNameWithoutExtension, 
    std::string* fileName, 
    std::string* fileExtension)
{
    sys::path stdFilePath{filePath};

    if (fileNameWithoutExtension)
        *fileNameWithoutExtension = stdFilePath.stem().generic_string();

    if (parentFolderPath)
        *parentFolderPath = stdFilePath.parent_path().generic_string();

    if (fileExtension)
        *fileExtension = stdFilePath.extension().generic_string();

    if (fileName)
        *fileName = stdFilePath.filename().generic_string();
}

bool FSIsDirectoryExists(const std::string & path)
{
#ifdef _WIN32
    DWORD attributes = ::GetFileAttributes(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
#else
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
#endif
}

bool FSIsFileExists(const std::string & path)
{
#ifdef _WIN32
    DWORD attributes = ::GetFileAttributes(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
#else
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
#endif
}

bool FSReadTextFromFile(const std::string& filePath, std::string& content)
{
    // clear text content
    content.clear();

    std::ifstream fileStream {filePath};
    if (!fileStream)
        return false;

    std::string stringLine {};
    while (std::getline(fileStream, stringLine, '\n'))
    {
        content.append(stringLine);
        content.append("\n");
    }

    return true;
}

bool FSReadTextLinesFromFile(const std::string& filePath, std::vector<std::string>& content)
{
    // clear text content
    content.clear();

    std::ifstream fileStream {filePath};
    if (!fileStream)
        return false;

    std::string stringLine {};
    while (std::getline(fileStream, stringLine, '\n'))
    {
        content.push_back(std::move(stringLine));
    }
    return true;
}

bool FSLoadJSON(const std::string& filePath, JsonDocument& document)
{
    std::string content;

    // parse file content
    return FSReadTextFromFile(filePath, content) && document.ParseDocument(content);
}


bool FSEnsureParentDirsExists(const std::string& path)
{
    if (path.empty())
        return false;

    std::filesystem::path srcPath {path};
    if (srcPath.has_extension()) // discard filename
    {
        srcPath = srcPath.parent_path();
    }

    if (srcPath.empty())
        return false;

    if (!std::filesystem::exists(srcPath))
    {
        std::error_code errcode;
        if (!std::filesystem::create_directories(srcPath, errcode))
            return false;
    }

    return true;
}

bool FileSystem::LocateEngineTexturesCache(std::string& thePath) const
{
    return PathToDirectory("DK2TextureCache", thePath);
}

bool FileSystem::LocateWAD(const std::string& theName, std::string& theResourcePath) const
{
    std::string subpath = cxx::va("data/%s", theName.c_str());
    return PathToFile(subpath, theResourcePath);
}

bool FileSystem::LocateTextTableFile(const std::string& fileName, std::string& resourcePath) const
{
    std::string subpath = cxx::va("%s/%s", mTextFolder.c_str(), fileName.c_str());
    return PathToFile(subpath, resourcePath);
}

bool FileSystem::EnumTextTableFiles(EnumFilesCallback callback) const
{
    cxx_assert(callback);
    int filesFound = 0;
    for (const std::string& searchPlace : mSearchPlaces)
    {
        const sys::path mapsDirectory = sys::path {searchPlace} / mTextFolder;
        const sys::path mapsExtension = ".str";
        if (!sys::exists(mapsDirectory))
            continue;

        sys::directory_iterator iter_directory_end;
        for (sys::directory_iterator iter_directory(mapsDirectory); 
            iter_directory != iter_directory_end; ++iter_directory)
        {
            if (!sys::is_regular_file(iter_directory->status()))
                continue;

            const sys::path& currentFile = iter_directory->path();
            if (!currentFile.has_extension() || mapsExtension != currentFile.extension())
                continue;

            callback(currentFile.filename().generic_string());
            ++filesFound;
        }
    }
    return filesFound > 0;
}
