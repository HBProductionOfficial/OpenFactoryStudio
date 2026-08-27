#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const bool ikemen = argc > 0 && fs::path(argv[0]).filename().string().rfind("Ikemen_GO", 0) == 0;
    std::ofstream file;
    if (!ikemen)
        file.open("mugen.log");
    std::ostream& log = ikemen ? std::cout : static_cast<std::ostream&>(file);
    log << "Fake engine status log\n";
    log << "Arguments:";
    for (int i = 1; i < argc; ++i)
        log << ' ' << argv[i];
    log << '\n';
    log << "Working folder: " << fs::current_path().generic_string() << '\n';
    if (argc > 1) {
        const fs::path def = fs::path("chars") / argv[1] / (std::string(argv[1]) + ".def");
        log << "Character definition: " << (fs::exists(def) ? "found" : "missing") << '\n';
    }
    log << "Log file successfully closed.\n";
    return 0;
}
