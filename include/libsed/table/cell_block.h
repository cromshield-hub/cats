#pragma once
// CellBlock is defined in core/Types.h
#include "../core/types.h"

namespace libsed {

/// CellBlock builder for convenience
struct CellBlockBuilder {
    CellBlock block;

    CellBlockBuilder& startColumn(uint32_t col) { block.startColumn = col; return *this; }
    CellBlockBuilder& endColumn(uint32_t col)   { block.endColumn = col; return *this; }
    CellBlockBuilder& startRow(uint32_t row)    { block.startRow = row; return *this; }
    CellBlockBuilder& endRow(uint32_t row)      { block.endRow = row; return *this; }
    CellBlockBuilder& column(uint32_t col)      { block.startColumn = col; block.endColumn = col; return *this; }

    operator CellBlock() const { return block; }
};

inline CellBlockBuilder cellBlock() { return CellBlockBuilder{}; }

} // namespace libsed
