#include "stdafx.h"
#include "DK2SoundBank.h"

#include <filesystem>
#include <fstream>

namespace sys = std::filesystem;

namespace
{
uint16_t ReadLE16(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<uint16_t>(data[offset]) |
        (static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t ReadLE32(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<uint32_t>(data[offset]) |
        (static_cast<uint32_t>(data[offset + 1]) << 8) |
        (static_cast<uint32_t>(data[offset + 2]) << 16) |
        (static_cast<uint32_t>(data[offset + 3]) << 24);
}

void WriteLE16(std::ostream& out, uint16_t value)
{
    const unsigned char b[2] = {
        static_cast<unsigned char>(value & 0xff),
        static_cast<unsigned char>((value >> 8) & 0xff)};
    out.write(reinterpret_cast<const char*>(b), sizeof(b));
}

void WriteLE32(std::ostream& out, uint32_t value)
{
    const unsigned char b[4] = {
        static_cast<unsigned char>(value & 0xff),
        static_cast<unsigned char>((value >> 8) & 0xff),
        static_cast<unsigned char>((value >> 16) & 0xff),
        static_cast<unsigned char>((value >> 24) & 0xff)};
    out.write(reinterpret_cast<const char*>(b), sizeof(b));
}

std::string ExtensionForType(unsigned char type)
{
    switch (type)
    {
        case 2:
        case 3: return ".wav";
        case 36:
        case 37: return ".mp2";
        default: return {};
    }
}

std::string TrimEntryName(const unsigned char* chars)
{
    std::string name(reinterpret_cast<const char*>(chars), 16);
    const std::size_t zero = name.find('\0');
    if (zero != std::string::npos)
        name.resize(zero);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
        name.pop_back();
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

sys::path SafeRelativePath(const std::string& rawName, std::size_t entryIndex,
    const std::string& extension)
{
    sys::path safe;
    for (const sys::path& component : sys::path(rawName))
    {
        const std::string value = component.string();
        if (value.empty() || value == "." || value == ".." || component.has_root_path())
            continue;
        safe /= component.filename();
    }

    if (safe.empty())
        safe = "sound_" + std::to_string(entryIndex);

    std::string leaf = safe.filename().string();
    std::string lowerLeaf = cxx::lower_string(leaf);
    if (!cxx::ends_with_icase(lowerLeaf, ".wav") &&
        !cxx::ends_with_icase(lowerLeaf, ".mp2"))
    {
        const std::size_t dot = leaf.find_last_of('.');
        if (dot != std::string::npos)
            leaf.resize(dot);
        leaf += extension;
        safe.replace_filename(leaf);
    }
    return safe;
}

bool WriteWaveHeader(std::ostream& out, uint32_t dataSize, uint16_t sampleRate,
    unsigned char bitsPerSample, uint16_t channels)
{
    const uint16_t blockAlign = static_cast<uint16_t>(channels * bitsPerSample / 8);
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
    out.write("RIFF", 4);
    WriteLE32(out, 36u + dataSize);
    out.write("WAVEfmt ", 8);
    WriteLE32(out, 16);
    WriteLE16(out, 1);
    WriteLE16(out, channels);
    WriteLE32(out, sampleRate);
    WriteLE32(out, byteRate);
    WriteLE16(out, blockAlign);
    WriteLE16(out, bitsPerSample);
    out.write("data", 4);
    WriteLE32(out, dataSize);
    return static_cast<bool>(out);
}

bool ReadWholeFile(const sys::path& path, std::vector<unsigned char>& data)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return false;
    const std::streamoff length = in.tellg();
    if (length < 0)
        return false;
    data.resize(static_cast<std::size_t>(length));
    in.seekg(0, std::ios::beg);
    if (!data.empty())
        in.read(reinterpret_cast<char*>(data.data()), length);
    return static_cast<bool>(in) || data.empty();
}

bool ExportBank(const sys::path& bankPath, const sys::path& relativeBank,
    const sys::path& outputRoot, DK2SoundExportStats& stats)
{
    std::vector<unsigned char> data;
    if (!ReadWholeFile(bankPath, data) || data.size() < 4)
    {
        ++stats.mFailed;
        return false;
    }

    const uint32_t count = ReadLE32(data, 0);
    const std::size_t tableEnd = 4u + static_cast<std::size_t>(count) * 4u;
    if (count == 0 || tableEnd > data.size())
    {
        ++stats.mFailed;
        return false;
    }

    ++stats.mBanks;
    const sys::path bankOutput = outputRoot / relativeBank.parent_path() /
        relativeBank.stem();

    for (uint32_t index = 0; index < count; ++index)
    {
        ++stats.mEntries;
        const uint32_t offset = ReadLE32(data, 4u + index * 4u);
        if (static_cast<std::size_t>(offset) + 40u > data.size())
        {
            ++stats.mFailed;
            continue;
        }

        const uint32_t dataSize = ReadLE32(data, offset + 4u);
        const std::string rawName = TrimEntryName(data.data() + offset + 8u);
        const uint16_t sampleRate = ReadLE16(data, offset + 24u);
        const unsigned char bitsPerSample = data[offset + 26u];
        const unsigned char type = data[offset + 27u];
        const std::size_t payloadOffset = static_cast<std::size_t>(offset) + 40u;

        if (dataSize == 0)
        {
            ++stats.mSkipped;
            continue;
        }
        if (payloadOffset + dataSize > data.size())
        {
            ++stats.mFailed;
            continue;
        }

        const std::string extension = ExtensionForType(type);
        if (extension.empty())
        {
            ++stats.mSkipped;
            continue;
        }

        sys::path relativeOutput = SafeRelativePath(rawName, index, extension);
        sys::path outputPath = bankOutput / relativeOutput;
        std::error_code ec;
        sys::create_directories(outputPath.parent_path(), ec);
        if (ec)
        {
            ++stats.mFailed;
            continue;
        }

        if (sys::exists(outputPath))
        {
            outputPath = outputPath.parent_path() /
                (outputPath.stem().string() + "__" + std::to_string(index) +
                 outputPath.extension().string());
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out)
        {
            ++stats.mFailed;
            continue;
        }
        if (type == 2 || type == 3)
        {
            const uint16_t channels = type == 3 ? 2 : 1;
            if (!WriteWaveHeader(out, dataSize, sampleRate, bitsPerSample, channels))
            {
                ++stats.mFailed;
                continue;
            }
        }

        out.write(reinterpret_cast<const char*>(data.data() + payloadOffset), dataSize);
        if (!out)
        {
            ++stats.mFailed;
            continue;
        }
        ++stats.mExported;
    }
    return true;
}

struct DK2SoundRef
{
    uint32_t mIndex = 0;      // 1-based SDT entry
    uint32_t mArchiveId = 0;  // 1-based BANK.map entry
};

bool FindCategoryMap(const sys::path& root, const std::string& category,
    const char* suffix, sys::path& output)
{
    const std::string wanted = cxx::lower_string(category + suffix);
    std::error_code ec;
    for (sys::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file())
            continue;
        if (cxx::lower_string(it->path().filename().string()) == wanted)
        {
            output = it->path();
            return true;
        }
    }
    return false;
}

bool ReadBankNames(const sys::path& path, std::vector<std::string>& names)
{
    std::vector<unsigned char> data;
    if (!ReadWholeFile(path, data) || data.size() < 28)
        return false;
    if (ReadLE32(data, 0) != 0xE9612C01u || ReadLE32(data, 4) != 0x11D231D0u ||
        ReadLE32(data, 8) != 0xA00009B4u || ReadLE32(data, 12) != 0x03F293C9u)
        return false;

    const uint32_t count = ReadLE32(data, 24);
    std::size_t offset = 28u + static_cast<std::size_t>(count) * 11u;
    if (offset > data.size())
        return false;

    names.clear();
    names.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (offset + 4 > data.size())
            return false;
        const uint32_t length = ReadLE32(data, offset);
        offset += 4;
        if (length == 0 || offset + length > data.size())
            return false;
        std::string name(reinterpret_cast<const char*>(data.data() + offset), length);
        offset += length;
        const std::size_t zero = name.find('\0');
        if (zero != std::string::npos)
            name.resize(zero);
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
            name.pop_back();
        std::replace(name.begin(), name.end(), '\\', '/');
        names.push_back(std::move(name));
    }
    return true;
}

bool ReadSfxEventRefs(const sys::path& path, uint32_t eventId,
    std::vector<DK2SoundRef>& refs)
{
    std::vector<unsigned char> data;
    if (!ReadWholeFile(path, data) || data.size() < 28)
        return false;
    if (ReadLE32(data, 0) != 0xE9612C00u || ReadLE32(data, 4) != 0x11D231D0u ||
        ReadLE32(data, 8) != 0xB00009B4u || ReadLE32(data, 12) != 0x03F293C9u)
        return false;

    const uint32_t topCount = ReadLE32(data, 24);
    std::size_t offset = 28;
    std::vector<uint32_t> groupCounts;
    groupCounts.reserve(topCount);
    std::size_t totalGroups = 0;
    for (uint32_t i = 0; i < topCount; ++i)
    {
        if (offset + 24 > data.size())
            return false;
        const uint32_t count = ReadLE32(data, offset);
        groupCounts.push_back(count);
        totalGroups += count;
        offset += 24;
    }

    struct GroupDesc { uint32_t mType = 0; uint32_t mEntries = 0; };
    std::vector<GroupDesc> groups;
    groups.reserve(totalGroups);
    for (uint32_t count : groupCounts)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (offset + 20 > data.size())
                return false;
            groups.push_back({ReadLE32(data, offset), ReadLE32(data, offset + 4)});
            offset += 20;
        }
    }

    refs.clear();
    for (const GroupDesc& group : groups)
    {
        struct EntryCounts { uint32_t mSounds = 0; uint32_t mData = 0; };
        std::vector<EntryCounts> entries;
        entries.reserve(group.mEntries);
        for (uint32_t i = 0; i < group.mEntries; ++i)
        {
            if (offset + 42 > data.size())
                return false;
            entries.push_back({ReadLE32(data, offset), ReadLE32(data, offset + 4)});
            offset += 42;
        }

        for (const EntryCounts& entry : entries)
        {
            for (uint32_t i = 0; i < entry.mSounds; ++i)
            {
                if (offset + 16 > data.size())
                    return false;
                const uint32_t soundIndex = ReadLE32(data, offset);
                const uint32_t archiveId = ReadLE32(data, offset + 12);
                if (group.mType == eventId && soundIndex && archiveId)
                    refs.push_back({soundIndex, archiveId});
                offset += 16;
            }
            const std::size_t dataBytes = static_cast<std::size_t>(entry.mData) * 8u;
            if (offset + dataBytes > data.size())
                return false;
            offset += dataBytes;
        }
    }
    return !refs.empty();
}

bool FindBankFile(const sys::path& root, const std::string& archiveName,
    const char* quality, sys::path& output)
{
    sys::path relative(archiveName);
    sys::path base = root / relative;
    const std::string targetName = base.filename().string() + quality + ".sdt";
    const sys::path direct = base.parent_path() / targetName;
    if (sys::is_regular_file(direct))
    {
        output = direct;
        return true;
    }

    std::error_code ec;
    const std::string wanted = cxx::lower_string(targetName);
    for (sys::directory_iterator it(base.parent_path(), ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file() && cxx::lower_string(it->path().filename().string()) == wanted)
        {
            output = it->path();
            return true;
        }
    }
    return false;
}

bool LoadBankClip(const sys::path& path, uint32_t oneBasedIndex,
    std::vector<unsigned char>& output)
{
    std::vector<unsigned char> data;
    if (!ReadWholeFile(path, data) || data.size() < 4 || oneBasedIndex == 0)
        return false;
    const uint32_t count = ReadLE32(data, 0);
    if (oneBasedIndex > count || 4u + static_cast<std::size_t>(count) * 4u > data.size())
        return false;

    const uint32_t index = oneBasedIndex - 1;
    const uint32_t entryOffset = ReadLE32(data, 4u + static_cast<std::size_t>(index) * 4u);
    if (static_cast<std::size_t>(entryOffset) + 40u > data.size())
        return false;
    const uint32_t dataSize = ReadLE32(data, entryOffset + 4u);
    const uint16_t sampleRate = ReadLE16(data, entryOffset + 24u);
    const unsigned char bitsPerSample = data[entryOffset + 26u];
    const unsigned char type = data[entryOffset + 27u];
    const std::size_t payloadOffset = static_cast<std::size_t>(entryOffset) + 40u;
    if (dataSize == 0 || payloadOffset + dataSize > data.size())
        return false;

    if (type == 2 || type == 3)
    {
        std::ostringstream stream(std::ios::binary | std::ios::out);
        if (!WriteWaveHeader(stream, dataSize, sampleRate, bitsPerSample, type == 3 ? 2 : 1))
            return false;
        stream.write(reinterpret_cast<const char*>(data.data() + payloadOffset), dataSize);
        const std::string bytes = stream.str();
        output.assign(bytes.begin(), bytes.end());
        return !output.empty();
    }
    if (type == 36 || type == 37)
    {
        output.assign(data.begin() + payloadOffset, data.begin() + payloadOffset + dataSize);
        return !output.empty();
    }
    return false;
}

} // namespace

bool DK2ExportSoundBanks(const std::string& inputRoot,
    const std::string& outputRoot, DK2SoundExportStats& stats)
{
    stats = {};
    const sys::path source(inputRoot);
    const sys::path destination(outputRoot);
    if (!sys::is_directory(source))
        return false;

    std::error_code ec;
    sys::create_directories(destination, ec);
    if (ec)
        return false;
    bool allBanksReadable = true;
    for (const sys::directory_entry& entry : sys::recursive_directory_iterator(source))
    {
        if (!entry.is_regular_file())
            continue;
        const std::string extension = cxx::lower_string(entry.path().extension().string());
        if (extension != ".sdt")
            continue;

        const sys::path relativeBank = sys::relative(entry.path(), source, ec);
        if (ec)
        {
            ++stats.mFailed;
            ec.clear();
            allBanksReadable = false;
            continue;
        }
        if (!ExportBank(entry.path(), relativeBank, destination, stats))
            allBanksReadable = false;
    }

    return allBanksReadable && stats.mBanks > 0 && stats.mFailed == 0;
}


bool DK2LoadSoundEvent(const std::string& sfxRoot, const std::string& category,
    unsigned int eventId, bool preferHD, std::vector<unsigned char>& outputData,
    std::string* outputSource)
{
    outputData.clear();
    if (outputSource)
        outputSource->clear();

    const sys::path root(sfxRoot);
    if (!sys::is_directory(root) || category.empty())
        return false;

    sys::path sfxMap;
    sys::path bankMap;
    if (!FindCategoryMap(root, category, "SFX.map", sfxMap) ||
        !FindCategoryMap(root, category, "BANK.map", bankMap))
        return false;

    std::vector<DK2SoundRef> refs;
    std::vector<std::string> archives;
    if (!ReadSfxEventRefs(sfxMap, eventId, refs) || !ReadBankNames(bankMap, archives))
        return false;

    const char* qualities[2] = {preferHD ? "HD" : "HW", preferHD ? "HW" : "HD"};
    for (const DK2SoundRef& ref : refs)
    {
        if (ref.mArchiveId == 0 || ref.mArchiveId > archives.size())
            continue;
        const std::string& archive = archives[ref.mArchiveId - 1];
        for (const char* quality : qualities)
        {
            sys::path bankPath;
            if (!FindBankFile(root, archive, quality, bankPath))
                continue;
            if (LoadBankClip(bankPath, ref.mIndex, outputData))
            {
                if (outputSource)
                    *outputSource = bankPath.string() + "#" + std::to_string(ref.mIndex);
                return true;
            }
        }
    }
    return false;
}
