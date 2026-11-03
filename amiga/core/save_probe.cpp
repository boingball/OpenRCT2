/* SPDX-License-Identifier: GPL-3.0-only */
#include "diagnostic_output.h"
#include <openrct2/core/Endianness.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/rct2/S6Header.h>
#include <openrct2/sawyer_coding/SawyerChunk.h>
#include <openrct2/sawyer_coding/SawyerChunkReader.h>
#include <openrct2/sawyer_coding/SawyerChunkWriter.h>
#include <openrct2/sawyer_coding/SawyerCoding.h>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef __amigaos__
extern "C" {
    unsigned long __stack = 262144;
    void __stkinit(void);
    void (*keep_stack_init)(void) = __stkinit;
}
#endif

using namespace OpenRCT2;
using namespace OpenRCT2::SawyerCoding;

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static uint32_t hashBytes(uint32_t hash, const void* data, size_t length)
{
    auto bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619U;
    return hash;
}

// Validate the external envelope before invoking the engine's decoder.
// In particular, never let malformed lengths reach huge allocations or the
// toolchain's FPU-using stream-error formatter.
static std::shared_ptr<SawyerChunk> readChecked(MemoryStream& stream)
{
    const auto position = stream.GetPosition();
    const auto remaining = stream.GetLength() - position;
    require(remaining >= sizeof(ChunkHeader), "Truncated chunk header");
    auto bytes = static_cast<const uint8_t*>(stream.GetData()) + position;
    const auto length = ReadLittleEndian<uint32_t>(bytes + 1);
    require(length > 0 && length < 16 * 1024 * 1024, "Invalid encoded chunk length");
    require(length <= remaining - sizeof(ChunkHeader), "Truncated chunk payload");
    return SawyerChunkReader(&stream).ReadChunk();
}

static void selfTest()
{
    print("Test: engine memory stream\n");
    MemoryStream strings;
    print("Test: stream string write\n");
    strings.WriteString(std::string_view("a\0b", 3));
    require(strings.GetLength() == 2 && std::memcmp(strings.GetData(), "a\0", 2) == 0,
        "Stream string terminator changed");
    // Fixed external bytes, not merely same-machine round trips.
    print("Test: fixed little-endian chunk\n");
    const uint8_t literal[] = { 0, 4, 0, 0, 0, 0x12, 0x34, 0x56, 0x78 };
    MemoryStream raw(literal, sizeof literal);
    auto chunk = readChecked(raw);
    require(chunk->GetLength() == 4 && std::memcmp(chunk->GetData(), literal + 5, 4) == 0,
        "Fixed little-endian chunk failed");
    raw.SetPosition(0);
    SawyerChunkReader(&raw).SkipChunk();
    require(raw.GetPosition() == sizeof literal, "Little-endian SkipChunk failed");
    print("Test: fixed RLE chunk\n");
    const uint8_t rle[] = { 1, 4, 0, 0, 0, 0, 'A', 0xfe, 'B' };
    MemoryStream compressed(rle, sizeof rle);
    chunk = readChecked(compressed);
    require(chunk->GetLength() == 4 && std::memcmp(chunk->GetData(), "ABBB", 4) == 0,
        "Fixed RLE decode failed");

    // Exercise all production Sawyer encoders and chunk decoders. Compare
    // this wire fingerprint between little-endian host and big-endian m68k.
    std::array<uint8_t, 257> payload{};
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = (i / 7) ^ (i % 11);
    std::vector<uint8_t> encoded(1024);
    uint32_t hash = 2166136261U;
    for (unsigned mode = 0; mode < 4; ++mode)
    {
        print("Test: encoding %u\n", mode);
        size_t length = WriteChunkBuffer(encoded.data(), payload.data(),
            { static_cast<ChunkEncoding>(mode), static_cast<uint32_t>(payload.size()) });
        require(length <= encoded.size(), "Encoded fixture too large");
        require(ReadLittleEndian<uint32_t>(encoded.data() + 1) == length - 5,
            "Writer produced a non-little-endian length");
        hash = hashBytes(hash, encoded.data(), length);
        MemoryStream stream(encoded.data(), length, MemoryAccess::read);
        chunk = readChecked(stream);
        require(chunk->GetLength() == payload.size()
            && std::memcmp(chunk->GetData(), payload.data(), payload.size()) == 0,
            "Sawyer encoding round trip failed");
    }
    require(hash == 0xb9cdc2a8U, "Wire bytes differ from the golden fixture");
    const uint8_t scalars[] = { 0, 0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90 };
    require(ReadLittleEndian<uint16_t>(scalars + 1) == 0x5678
        && ReadLittleEndian<uint32_t>(scalars + 1) == 0x12345678U
        && ReadLittleEndian<uint64_t>(scalars + 1) == 0x90abcdef12345678ULL,
        "Unaligned scalar decoding failed");
    std::array<uint8_t, sizeof scalars> written{};
    WriteLittleEndian<uint64_t>(written.data() + 1, 0x90abcdef12345678ULL);
    require(std::memcmp(written.data(), scalars, sizeof scalars) == 0, "Scalar encoding failed");
    // Checksum writes may be unaligned. TD6 checksums are also little-endian.
    print("Test: track checksum\n");
    std::vector<uint8_t> track(2048);
    const size_t trackLength = EncodeTD6(payload.data(), track.data() + 1, payload.size());
    require(ValidateTrackChecksum(track.data() + 1, trackLength) == 1, "TD6 checksum failed");
    MemoryStream trackStream;
    SawyerChunkWriter(&trackStream).WriteChunkTrack(payload.data(), payload.size());
    require(trackStream.GetLength() == trackLength
        && std::memcmp(trackStream.GetData(), track.data() + 1, trackLength) == 0,
        "Stream writer track bytes differ");
    track[2] ^= 1;
    require(ValidateTrackChecksum(track.data() + 1, trackLength) == 0, "Corrupt TD6 accepted");
    require(ValidateTrackChecksum(track.data(), 3) == 0, "Short TD6 accepted");

    const std::vector<std::vector<uint8_t>> malformed = {
        { 0, 4 }, { 0, 4, 0, 0, 0, 1 }, { 0, 0, 0, 0, 0 },
        { 99, 1, 0, 0, 0, 42 }, { 1, 1, 0, 0, 0, 0xff },
        { 1, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 4, 1 }
    };
    print("Test: corrupt input\n");
    for (const auto& bytes : malformed)
    {
        MemoryStream stream(bytes.data(), bytes.size());
        bool rejected = false;
        try { (void)readChecked(stream); }
        catch (const std::exception&) { rejected = true; }
        require(rejected && stream.GetPosition() == 0, "Malformed chunk accepted or not rewound");
    }
    print("Wire fingerprint (all four encodings): %08lx\n", static_cast<unsigned long>(hash));
    print("Fixed bytes / encoders / decoder / skip / unaligned checksum / corrupt input: PASS\n");
}

struct CloseFile { void operator()(FILE* file) const { std::fclose(file); } };

static void inspectSave(const char* path)
{
    std::unique_ptr<FILE, CloseFile> file(std::fopen(path, "rb"));
    require(file != nullptr, "Cannot open save file");
    require(std::fseek(file.get(), 0, SEEK_END) == 0, "Cannot seek file");
    const long size = std::ftell(file.get());
    require(size >= 9 && size <= 16 * 1024 * 1024, "File size outside diagnostic limit (9 bytes to 16 MiB)");
    require(std::fseek(file.get(), 0, SEEK_SET) == 0, "Cannot rewind file");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    require(std::fread(bytes.data(), 1, bytes.size(), file.get()) == bytes.size(), "Cannot read complete file");
    const size_t dataLength = bytes.size() - 4;
    require(CalculateChecksum(bytes.data(), dataLength) == ReadLittleEndian<uint32_t>(bytes.data() + dataLength),
        "SV6/SC6 checksum mismatch");
    print("Save file checksum: PASS\n");
    MemoryStream stream(bytes.data(), dataLength, MemoryAccess::read);
    auto chunk = readChecked(stream);
    require(chunk->GetLength() == sizeof(RCT2::S6Header), "Not an S6 header chunk");
    RCT2::S6Header header;
    std::memcpy(&header, chunk->GetData(), sizeof header);
    header.NumPackedObjects = ToLittleEndian(header.NumPackedObjects);
    header.Version = ToLittleEndian(header.Version);
    header.MagicNumber = ToLittleEndian(header.MagicNumber);
    require(header.Type <= 1, "Unknown S6 file type");
    require(header.ClassicFlag != 0x0f, "Extended RCT Classic saves not supported by this diagnostic");
    print("S6 type: %u; packed objects: %u; version: %lu; magic: %08lx\n",
        static_cast<unsigned>(header.Type), static_cast<unsigned>(header.NumPackedObjects),
        static_cast<unsigned long>(header.Version), static_cast<unsigned long>(header.MagicNumber));
    uint32_t hash = hashBytes(2166136261U, chunk->GetData(), chunk->GetLength());
    unsigned count = 1;
    auto decode = [&] {
        auto next = readChecked(stream);
        hash = hashBytes(hash, next->GetData(), next->GetLength());
        ++count;
    };
    if (header.Type == 1) decode(); // scenario info precedes packed objects
    for (unsigned i = 0; i < header.NumPackedObjects; ++i)
    {
        require(stream.GetLength() - stream.GetPosition() >= 16 /* legacy RCTObjectEntry wire size */, "Truncated packed object entry");
        stream.Seek(16 /* legacy RCTObjectEntry wire size */, STREAM_SEEK_CURRENT);
        decode();
    }
    while (stream.GetPosition() < stream.GetLength()) decode();
    print("Decoded chunks: %u; decoded-byte fingerprint: %08lx\n", count, static_cast<unsigned long>(hash));
    print("S6 chunk decoding: PASS (no park import or simulation)\n");
}

int main(int argc, char** argv)
{
    report = std::fopen("OpenRCT2SaveProbe.txt", "w");
    print("OpenRCT2 native save/byte-order probe 0.6\n");
    print("Actual Sawyer coding and MemoryStream; no park simulation.\n");
    int result = 0;
    try
    {
        require(argc <= 2, "Usage: OpenRCT2SaveProbe [park.sv6 or scenario.sc6]");
        selfTest();
        if (argc == 2) inspectSave(argv[1]);
        print("Save/byte-order checks: PASS\n");
    }
    catch (const std::exception& error)
    {
        print("FAIL: %s\n", error.what());
        result = 20;
    }
    if (report && std::fclose(report) != 0) result = 20;
    return result;
}
