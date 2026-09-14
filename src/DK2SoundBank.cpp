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
