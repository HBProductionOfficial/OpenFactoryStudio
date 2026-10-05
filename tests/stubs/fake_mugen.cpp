// Fake mugen.exe for the Run in engine tests. It logs what it was given.

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

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
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "-r") {
            const fs::path system = fs::path("data") / argv[i + 1] / "system.def";
            log << "Motif definition: " << (fs::exists(system) ? "found" : "missing") << '\n';
        }
    }
    // Flush now so an engine stopped while it waits (Debug > Stop) has still written this much.
    log.flush();
#if defined(_MSC_VER)
    char* wait = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&wait, &length, "OFS_FAKE_ENGINE_WAIT_MS") == 0 && wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(wait)));
        std::free(wait);
    }
#else
    if (const char* wait = std::getenv("OFS_FAKE_ENGINE_WAIT_MS"))
        std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(wait)));
#endif
    log << "Log file successfully closed.\n";
    return 0;
}
