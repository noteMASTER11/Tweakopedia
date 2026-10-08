#include "package/PackageFooter.h"
#include "package/Sha256.h"

#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

using namespace tweakopedia;

namespace {

package::Digest digestFromHex(std::string_view hex)
{
    package::Digest digest{};
    auto nibble = [](char value) -> unsigned char {
        if (value >= '0' && value <= '9') return static_cast<unsigned char>(value - '0');
        if (value >= 'a' && value <= 'f') return static_cast<unsigned char>(10 + value - 'a');
        return static_cast<unsigned char>(10 + value - 'A');
    };
    for (std::size_t index = 0; index < digest.size(); ++index) {
        digest[index] = static_cast<std::byte>(
            (nibble(hex[index * 2]) << 4) | nibble(hex[index * 2 + 1]));
    }
    return digest;
}

void writeBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void appendBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::app);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

std::filesystem::path testPath(const QTemporaryDir& directory, const char* name)
{
    return std::filesystem::path(directory.path().toStdWString()) / name;
}

package::PackageFooter validFooter(const std::vector<std::byte>& prefix,
                                   const std::vector<std::byte>& payload,
                                   const std::filesystem::path& scratch)
{
    std::vector<std::byte> bytes = prefix;
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    writeBytes(scratch, bytes);
    const auto hash = package::sha256File(scratch, prefix.size(), payload.size());
    Q_ASSERT(hash.ok());
    return {
        .schemaVersion = package::PackageFooter::schemaVersionCurrent,
        .flags = 0,
        .payloadOffset = prefix.size(),
        .payloadSize = payload.size(),
        .payloadDigest = *hash.value,
    };
}

}

class PackageFooterTest final : public QObject
{
    Q_OBJECT

private slots:
    void hashesKnownVectorAndSlice()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto file = testPath(directory, "hash.bin");
        const std::array bytes{
            std::byte{'x'}, std::byte{'a'}, std::byte{'b'}, std::byte{'c'}, std::byte{'y'}};
        writeBytes(file, bytes);

        const auto result = package::sha256File(file, 1, 3);
        QVERIFY(result.ok());
        QCOMPARE(*result.value, digestFromHex(
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }

    void encodesExactMagicVersionAndRoundTrips()
    {
        package::PackageFooter footer{
            .schemaVersion = package::PackageFooter::schemaVersionCurrent,
            .flags = 7,
            .payloadOffset = 0x0102030405060708ULL,
            .payloadSize = 0x1112131415161718ULL,
            .payloadDigest = digestFromHex(
                "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"),
        };

        const auto encoded = footer.encode();
        QCOMPARE(encoded.size(), package::PackageFooter::fixedSize);
        QCOMPARE(std::string_view(reinterpret_cast<const char*>(encoded.data()), 8),
                 std::string_view("TWPKG001"));
        QCOMPARE(std::to_integer<unsigned char>(encoded[8]), 1);
        QCOMPARE(std::to_integer<unsigned char>(encoded[9]), 0);
        QCOMPARE(std::to_integer<unsigned char>(encoded[10]), 0);
        QCOMPARE(std::to_integer<unsigned char>(encoded[11]), 0);

        const auto decoded = package::PackageFooter::decode(encoded);
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.value->schemaVersion, footer.schemaVersion);
        QCOMPARE(decoded.value->flags, footer.flags);
        QCOMPARE(decoded.value->payloadOffset, footer.payloadOffset);
        QCOMPARE(decoded.value->payloadSize, footer.payloadSize);
        QCOMPARE(decoded.value->payloadDigest, footer.payloadDigest);
    }

    void rejectsTooSmallFileBadMagicAndUnknownVersion()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto file = testPath(directory, "container.exe");
        std::vector<std::byte> shortFile(package::PackageFooter::fixedSize - 1);
        writeBytes(file, shortFile);
        QVERIFY(!package::inspectContainer(file).ok());

        package::PackageFooter footer{};
        auto bytes = footer.encode();
        bytes[0] = std::byte{'X'};
        writeBytes(file, bytes);
        QVERIFY(!package::inspectContainer(file).ok());

        footer.schemaVersion = package::PackageFooter::schemaVersionCurrent + 1;
        bytes = footer.encode();
        writeBytes(file, bytes);
        QVERIFY(!package::inspectContainer(file).ok());
    }

    void rejectsOverflowOverlapTruncationAndDigestMismatch()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto file = testPath(directory, "container.exe");
        const std::vector<std::byte> prefix(16, std::byte{0x41});
        const std::vector<std::byte> payload(12, std::byte{0x42});
        auto footer = validFooter(prefix, payload, testPath(directory, "payload.bin"));

        std::vector<std::byte> container = prefix;
        container.insert(container.end(), payload.begin(), payload.end());
        writeBytes(file, container);

        auto invalid = footer;
        invalid.payloadOffset = std::numeric_limits<std::uint64_t>::max() - 2;
        invalid.payloadSize = 8;
        auto encoded = invalid.encode();
        appendBytes(file, encoded);
        QVERIFY(!package::inspectContainer(file).ok());

        writeBytes(file, container);
        invalid = footer;
        invalid.payloadSize += package::PackageFooter::fixedSize;
        encoded = invalid.encode();
        appendBytes(file, encoded);
        QVERIFY(!package::inspectContainer(file).ok());

        writeBytes(file, container);
        invalid = footer;
        invalid.payloadSize += 1;
        encoded = invalid.encode();
        appendBytes(file, encoded);
        QVERIFY(!package::inspectContainer(file).ok());

        writeBytes(file, container);
        invalid = footer;
        invalid.payloadDigest[0] ^= std::byte{0xff};
        encoded = invalid.encode();
        appendBytes(file, encoded);
        QVERIFY(!package::inspectContainer(file).ok());
    }

    void inspectsValidContainer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto file = testPath(directory, "Tweakopedia.exe");
        const std::vector<std::byte> prefix(23, std::byte{0x31});
        const std::vector<std::byte> payload(41, std::byte{0x52});
        const auto footer = validFooter(prefix, payload, testPath(directory, "payload.bin"));

        writeBytes(file, prefix);
        appendBytes(file, payload);
        const auto encoded = footer.encode();
        appendBytes(file, encoded);

        const auto result = package::inspectContainer(file);
        QVERIFY(result.ok());
        QCOMPARE(result.value->containerPath, file);
        QCOMPARE(result.value->footer.payloadOffset, prefix.size());
        QCOMPARE(result.value->footer.payloadSize, payload.size());
        QCOMPARE(result.value->footer.payloadDigest, footer.payloadDigest);
    }
};

QTEST_MAIN(PackageFooterTest)
#include "PackageFooterTest.moc"
