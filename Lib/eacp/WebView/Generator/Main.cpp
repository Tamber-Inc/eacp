#include <ResEmbed/ResEmbed.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
constexpr auto kPlaceholder = std::string_view {"__EACP_BASENAME__"};
constexpr auto kTemplateName = "EacpBackend.ts.template";
constexpr auto kTemplateCategory = "EacpWebViewBackendGen";

std::string substitute(std::string_view source,
                       std::string_view token,
                       std::string_view replacement)
{
    auto out = std::string {};
    out.reserve(source.size());

    auto pos = std::size_t {0};
    while (pos < source.size())
    {
        auto hit = source.find(token, pos);

        if (hit == std::string_view::npos)
        {
            out.append(source.substr(pos));
            break;
        }

        out.append(source.substr(pos, hit - pos));
        out.append(replacement);
        pos = hit + token.size();
    }

    return out;
}

bool writeFileIfChanged(const std::filesystem::path& path,
                        const std::string& contents)
{
    auto in = std::ifstream {path, std::ios::binary};

    if (in)
    {
        auto existing = std::string {
            std::istreambuf_iterator<char> {in},
            std::istreambuf_iterator<char> {}};

        if (existing == contents)
            return false;
    }

    std::filesystem::create_directories(path.parent_path());

    auto out = std::ofstream {path, std::ios::binary};

    if (!out)
        throw std::runtime_error("cannot open output file: " + path.string());

    out << contents;

    if (!out)
        throw std::runtime_error("failed to write output file: " + path.string());

    return true;
}

void usage(const char* exeName)
{
    std::cerr << "Usage: " << exeName
              << " --out <path> --basename <name>\n"
              << "  --out <path>      Path to write the generated TypeScript shim\n"
              << "  --basename <name> Schema basename used in import paths\n";
}
} // namespace

int main(int argc, char** argv)
{
    auto outPath = std::filesystem::path {};
    auto basename = std::string {};

    for (auto i = 1; i < argc; ++i)
    {
        auto arg = std::string_view {argv[i]};

        if (arg == "--out" && i + 1 < argc)
            outPath = argv[++i];
        else if (arg == "--basename" && i + 1 < argc)
            basename = argv[++i];
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (outPath.empty() || basename.empty())
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    try
    {
        auto resource = ResEmbed::get(kTemplateName, kTemplateCategory);

        if (! resource)
        {
            std::cerr << "embedded template not found: "
                      << kTemplateName << "\n";
            return EXIT_FAILURE;
        }

        auto rendered = substitute(resource.toStringView(),
                                   kPlaceholder,
                                   basename);

        writeFileIfChanged(outPath, rendered);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
