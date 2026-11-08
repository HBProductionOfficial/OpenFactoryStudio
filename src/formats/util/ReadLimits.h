#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace OFS::Formats {

// Only the first problems found in a damaged file are kept, so a file that is damaged all over
// doesn't fill the Issues panel.
inline constexpr std::size_t kMaxReadProblems = 100;

inline void addReadProblem(std::vector<std::string>& problems, std::string text)
{
    if (problems.size() < kMaxReadProblems)
        problems.push_back(std::move(text));
    else if (problems.size() == kMaxReadProblems)
        problems.push_back("There are more problems than the ones listed here.");
}

// Many parts of a file can point to the same bytes, and readers copy them for each part. They
// keep count, so a small file can't make the program run out of memory.
class CopyBudget
{
public:
    explicit CopyBudget(std::size_t fileSize)
        : m_left(std::uint64_t{fileSize} * 2 + kSlack)
    {}

    bool take(std::uint64_t bytes)
    {
        if (bytes > m_left)
            return false;
        m_left -= bytes;
        return true;
    }

private:
    static constexpr std::uint64_t kSlack = std::uint64_t{64} << 20;
    std::uint64_t m_left;
};

} // namespace OFS::Formats
