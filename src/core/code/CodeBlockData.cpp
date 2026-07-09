#include "core/code/CodeBlockData.h"

namespace OFS {

CodeBlockData* CodeBlockData::ensure(QTextBlock block)
{
    if (auto* data = get(block))
        return data;
    auto* data = new CodeBlockData;
    block.setUserData(data);
    return data;
}

} // namespace OFS
