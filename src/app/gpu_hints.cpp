// Hybrid-graphics drivers look up these symbols and then run the editor on the discrete GPU.
#if defined(_WIN32)
#  include <cstdint>

extern "C" {
__declspec(dllexport) std::uint32_t NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
