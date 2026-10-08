#include "package/PayloadManifest.h"
#include "package/Sha256.h"
#include "package/ZipPayload.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <miniz.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace tweakopedia;

namespace {

std::string narrowPath(const std::filesystem::path& path)
{
    const auto utf8 = path.generic_u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

void writeFile(const std::filesystem::path& path, std::string_view bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::byte> readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const auto size = static_cast<std::size_t>(stream.tellg());
    std::vector<std::byte> bytes(size);
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    return bytes;
}

package::PayloadManifest makeSource(const std::filesystem::path& root)
{
    writeFile(root / "Tweakopedia.App.exe", "app");
    writeFile(root / "Tweakopedia.Executor.exe", "executor");
    writeFile(root / "content/categories.yaml", "categories: []\n");
    writeFile(root / "plugins/example.dll", "plugin");
    const auto manifest = package::PayloadManifest::fromDirectory(root);
    Q_ASSERT(manifest.ok());
    return *manifest.value;
}

void writeArchive(
    const std::filesystem::path& path,
    const package::PayloadManifest& manifest,
    const std::vector<std::pair<std::string, std::string>>& files)
{
    mz_zip_archive archive{};
    const auto archivePath = narrowPath(path);
    QVERIFY(mz_zip_writer_init_file(&archive, archivePath.c_str(), 0));
    const auto manifestJson = manifest.toCanonicalJson();
    MZ_TIME_T fixedTime = 315532800;
    QVERIFY(mz_zip_writer_add_mem_ex_v2(
        &archive, "payload-manifest.json", manifestJson.data(), manifestJson.size(),
        nullptr, 0, MZ_BEST_COMPRESSION, 0, 0, &fixedTime, nullptr, 0, nullptr, 0));
    for (const auto& [name, contents] : files) {
        QVERIFY(mz_zip_writer_add_mem_ex_v2(
            &archive, name.c_str(), contents.data(), contents.size(), nullptr, 0,
            MZ_BEST_COMPRESSION, 0, 0, &fixedTime, nullptr, 0, nullptr, 0));
    }
    QVERIFY(mz_zip_writer_finalize_archive(&archive));
    QVERIFY(mz_zip_writer_end(&archive));
}

std::vector<std::pair<std::string, std::string>> validFiles()
{
    return {
        {"Tweakopedia.App.exe", "app"},
        {"Tweakopedia.Executor.exe", "executor"},
        {"content/categories.yaml", "categories: []\n"},
        {"plugins/example.dll", "plugin"},
    };
}

package::PackageInfo packageInfo(const std::filesystem::path& archive)
{
    package::PackageInfo info;
    info.containerPath = archive;
    info.containerSize = std::filesystem::file_size(archive);
    info.footer.payloadOffset = 0;
    info.footer.payloadSize = info.containerSize;
    const auto digest = package::sha256File(archive);
    Q_ASSERT(digest.ok());
    info.footer.payloadDigest = *digest.value;
    return info;
}

std::size_t findCentralDirectory(std::vector<std::byte>& bytes, std::size_t occurrence = 0)
{
    const std::array signature{std::byte{0x50}, std::byte{0x4b}, std::byte{0x01}, std::byte{0x02}};
    auto iterator = bytes.begin();
    for (std::size_t index = 0; index <= occurrence; ++index) {
        iterator = std::search(iterator, bytes.end(), signature.begin(), signature.end());
        if (iterator == bytes.end()) return bytes.size();
        if (index != occurrence) ++iterator;
    }
    return static_cast<std::size_t>(iterator - bytes.begin());
}

void writeUint32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8)) & 0xffU);
}

void overwriteFile(const std::filesystem::path& path, const std::vector<std::byte>& bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

}

class ZipPayloadTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsDeterministicOrderedArchiveWithFixedTime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString()) / "source";
        const auto manifest = makeSource(root);
        const auto first = root.parent_path() / "first.zip";
        const auto second = root.parent_path() / "second.zip";

        QVERIFY(package::ZipPayload::create(root, first, manifest).ok());
        QVERIFY(package::ZipPayload::create(root, second, manifest).ok());
        QCOMPARE(readFile(first), readFile(second));

        mz_zip_archive archive{};
        const auto path = narrowPath(first);
        QVERIFY(mz_zip_reader_init_file(&archive, path.c_str(), 0));
        QCOMPARE(mz_zip_reader_get_num_files(&archive), manifest.entries().size() + 1);
        mz_zip_archive_file_stat stat{};
        QVERIFY(mz_zip_reader_file_stat(&archive, 0, &stat));
        QCOMPARE(std::string(stat.m_filename), std::string("payload-manifest.json"));
        QCOMPARE(static_cast<std::int64_t>(stat.m_time), static_cast<std::int64_t>(315532800));
        for (std::size_t index = 0; index < manifest.entries().size(); ++index) {
            QVERIFY(mz_zip_reader_file_stat(&archive, static_cast<mz_uint>(index + 1), &stat));
            QCOMPARE(std::string(stat.m_filename), manifest.entries()[index].path);
            QCOMPARE(static_cast<std::int64_t>(stat.m_time), static_cast<std::int64_t>(315532800));
        }
        QVERIFY(mz_zip_reader_end(&archive));
    }

    void extractsVerifiedArchive()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto root = base / "source";
        const auto output = base / "payload.zip";
        const auto destination = base / "extracted";
        const auto manifest = makeSource(root);
        QVERIFY(package::ZipPayload::create(root, output, manifest).ok());

        QVERIFY(package::ZipPayload::extract(output, packageInfo(output), destination).ok());
        QCOMPARE(readFile(destination / "Tweakopedia.App.exe"),
                 readFile(root / "Tweakopedia.App.exe"));
        QVERIFY(std::filesystem::exists(destination / "payload-manifest.json"));
    }

    void rejectsUntrustedArchiveShapes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto manifest = makeSource(base / "source");
        auto assertRejected = [&](std::string_view name,
                                  std::vector<std::pair<std::string, std::string>> files) {
            const auto archive = base / (std::string(name) + ".zip");
            const auto destination = base / (std::string(name) + "-out");
            writeArchive(archive, manifest, files);
            QVERIFY(!package::ZipPayload::extract(archive, packageInfo(archive), destination).ok());
            QVERIFY(!std::filesystem::exists(destination));
        };

        auto files = validFiles();
        files.push_back({"undeclared.dll", "x"});
        assertRejected("undeclared", files);

        files = validFiles();
        files.erase(files.begin() + 1);
        assertRejected("missing", files);

        files = validFiles();
        files.push_back(files.front());
        assertRejected("duplicate", files);

        files = validFiles();
        files.push_back({"../outside.dll", "x"});
        assertRejected("traversal", files);

        files = validFiles();
        files[0].second = "longer";
        assertRejected("size", files);

        files = validFiles();
        files[0].second = "bad";
        assertRejected("digest", files);
    }

    void rejectsSymlinkHugeExpansionAndCorruptCentralDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto manifest = makeSource(base / "source");

        auto rejectPatched = [&](const char* name, auto patch) {
            const auto archive = base / (std::string(name) + ".zip");
            const auto destination = base / (std::string(name) + "-out");
            writeArchive(archive, manifest, validFiles());
            auto bytes = readFile(archive);
            patch(bytes);
            overwriteFile(archive, bytes);
            QVERIFY(!package::ZipPayload::extract(archive, packageInfo(archive), destination).ok());
            QVERIFY(!std::filesystem::exists(destination));
        };

        rejectPatched("symlink", [](auto& bytes) {
            const auto central = findCentralDirectory(bytes, 1);
            QVERIFY(central < bytes.size());
            bytes[central + 5] = std::byte{3};
            writeUint32(bytes, central + 38, static_cast<std::uint32_t>(0120777U << 16));
        });

        rejectPatched("huge", [](auto& bytes) {
            const auto central = findCentralDirectory(bytes, 1);
            QVERIFY(central < bytes.size());
            writeUint32(bytes, central + 24, 0xffffffffU);
        });

        rejectPatched("corrupt", [](auto& bytes) {
            const auto central = findCentralDirectory(bytes);
            QVERIFY(central < bytes.size());
            bytes[central] = std::byte{0};
        });
    }
};

QTEST_MAIN(ZipPayloadTest)
#include "ZipPayloadTest.moc"
