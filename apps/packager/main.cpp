#include "package/PackageFooter.h"
#include "package/PayloadManifest.h"
#include "package/Sha256.h"
#include "package/ZipPayload.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>

namespace {

using tweakopedia::package::PackageFooter;
using tweakopedia::package::PayloadManifest;

std::optional<std::map<std::wstring, std::wstring>> parseOptions(
    int argc, wchar_t* argv[], int first)
{
    std::map<std::wstring, std::wstring> result;
    for (int index = first; index < argc; ++index) {
        const std::wstring key(argv[index]);
        if (key == L"--json") {
            if (!result.emplace(key, L"").second) return std::nullopt;
            continue;
        }
        if (!key.starts_with(L"--") || index + 1 >= argc
            || !result.emplace(key, argv[++index]).second) {
            return std::nullopt;
        }
    }
    return result;
}

bool isInside(const std::filesystem::path& child, const std::filesystem::path& parent)
{
    std::error_code error;
    const auto relative = std::filesystem::relative(
        std::filesystem::absolute(child), std::filesystem::absolute(parent), error);
    if (error || relative.empty()) return !error;
    return *relative.begin() != ".." && !relative.is_absolute();
}

int fail(std::string_view message)
{
    std::cerr << message << '\n';
    return 2;
}

int createContainer(const std::map<std::wstring, std::wstring>& options)
{
    const auto bootstrapIt = options.find(L"--bootstrap");
    const auto payloadIt = options.find(L"--payload-root");
    const auto outputIt = options.find(L"--output");
    if (options.size() != 3 || bootstrapIt == options.end()
        || payloadIt == options.end() || outputIt == options.end()) {
        return fail("create requires --bootstrap, --payload-root and --output");
    }
    const std::filesystem::path bootstrap(bootstrapIt->second);
    const std::filesystem::path payloadRoot(payloadIt->second);
    const std::filesystem::path output(outputIt->second);
    std::error_code error;
    const auto sameAsBootstrap = std::filesystem::exists(output, error)
        && std::filesystem::equivalent(bootstrap, output, error);
    if (!std::filesystem::is_regular_file(bootstrap, error) || error
        || !std::filesystem::is_directory(payloadRoot, error) || error
        || output.filename() != L"Tweakopedia.exe"
        || !std::filesystem::is_directory(output.parent_path(), error) || error
        || std::filesystem::exists(output, error) || error
        || sameAsBootstrap || isInside(output, payloadRoot)) {
        return fail("Invalid bootstrap, payload root or output path");
    }

    const auto manifest = PayloadManifest::fromDirectory(payloadRoot);
    if (!manifest.ok()) return fail(manifest.error.message);
    for (const auto required : {
             "licenses/Qt-LGPLv3.txt",
             "licenses/THIRD-PARTY-NOTICES.txt"}) {
        if (!manifest.value->find(required)) return fail("Required license payload file is missing");
    }

    const auto zip = output.parent_path()
        / (L".tweakopedia-payload-" + std::to_wstring(GetCurrentProcessId()) + L".zip");
    if (std::filesystem::exists(zip, error)) return fail("Temporary payload path already exists");
    const auto zipped = tweakopedia::package::ZipPayload::create(payloadRoot, zip, *manifest.value);
    if (!zipped.ok()) return fail(zipped.error.message);
    auto cleanup = [&] {
        std::filesystem::remove(zip, error);
        std::filesystem::remove(output, error);
    };

    std::filesystem::copy_file(bootstrap, output, error);
    if (error) {
        cleanup();
        return fail("Cannot copy bootstrap executable");
    }
    const auto payloadOffset = std::filesystem::file_size(output, error);
    const auto payloadSize = std::filesystem::file_size(zip, error);
    std::ifstream input(zip, std::ios::binary);
    std::ofstream destination(output, std::ios::binary | std::ios::app);
    destination << input.rdbuf();
    input.close();
    destination.close();
    if (!input || !destination) {
        cleanup();
        return fail("Cannot append payload archive");
    }
    const auto digest = tweakopedia::package::sha256File(output, payloadOffset, payloadSize);
    if (!digest.ok()) {
        cleanup();
        return fail(digest.error.message);
    }
    const PackageFooter footer{
        .payloadOffset = payloadOffset,
        .payloadSize = payloadSize,
        .payloadDigest = *digest.value,
    };
    const auto encoded = footer.encode();
    std::ofstream footerOutput(output, std::ios::binary | std::ios::app);
    footerOutput.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    const auto footerWritten = static_cast<bool>(footerOutput);
    footerOutput.close();
    std::error_code removeError;
    std::filesystem::remove(zip, removeError);
    if (!footerWritten || removeError) {
        std::filesystem::remove(output, error);
        return fail("Cannot finalize container");
    }
    return 0;
}

int inspect(const std::map<std::wstring, std::wstring>& options)
{
    const auto containerIt = options.find(L"--container");
    if (containerIt == options.end() || options.size() > 2
        || (options.size() == 2 && !options.contains(L"--json"))) {
        return fail("inspect requires --container and optional --json");
    }
    const std::filesystem::path container(containerIt->second);
    const auto info = tweakopedia::package::inspectContainer(container);
    if (!info.ok()) return fail(info.error.message);
    const auto manifest = tweakopedia::package::ZipPayload::readManifest(container, *info.value);
    if (!manifest.ok()) return fail(manifest.error.message);

    nlohmann::json files = nlohmann::json::array();
    for (const auto& entry : manifest.value->entries()) files.push_back(entry.path);
    const nlohmann::json result{
        {"schema_version", info.value->footer.schemaVersion},
        {"payload_sha256", tweakopedia::package::digestToHex(info.value->footer.payloadDigest)},
        {"payload_size", info.value->footer.payloadSize},
        {"files", std::move(files)},
    };
    std::cout << result.dump() << '\n';
    return 0;
}

}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) return fail("Expected create or inspect command");
    const auto options = parseOptions(argc, argv, 2);
    if (!options) return fail("Invalid command line");
    const std::wstring command(argv[1]);
    if (command == L"create") return createContainer(*options);
    if (command == L"inspect") return inspect(*options);
    return fail("Unknown command");
}
