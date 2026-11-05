#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace OpenRCT2::Drawing
{
    class InvalidationGrid
    {
        uint16_t _blockWidth{};
        uint16_t _blockHeight{};
        uint32_t _columnCount{};
        uint32_t _rowCount{};

        std::vector<uint8_t> _blocks;
        uint32_t _screenWidth{};
        uint32_t _screenHeight{};

        uint32_t _lowestRow{ std::numeric_limits<uint32_t>::max() };
        uint32_t _highestRow{ 0 };

        uint32_t _lowestColumn{ std::numeric_limits<uint32_t>::max() };
        uint32_t _highestColumn{};

        std::vector<uint8_t> _coalesceSeen;
        std::vector<uint32_t> _coalesceStack;

    public:
        uint32_t getRowCount() const noexcept;

        uint32_t getColumnCount() const noexcept;

        uint32_t getBlockWidth() const noexcept;

        uint32_t getBlockHeight() const noexcept;

        void reset(int32_t width, int32_t height, uint32_t blockWidth, uint32_t blockHeight) noexcept;

        void invalidate(int32_t left, int32_t top, int32_t right, int32_t bottom) noexcept;

        /**
         * Grows each 8-connected patch of dirty cells to its bounding box when the patch fills at least
         * minFillPercent of it. Moving sprites leave staircase-shaped patches that traverseDirtyCells splits into
         * several rectangles; every rectangle costs a whole viewport paint (tile walk included), so painting a few
         * clean cells inside one box is cheaper. Marking more cells dirty is always correct.
         */
        void coalesce(uint32_t minFillPercent)
        {
            if (_lowestRow > _highestRow || _lowestColumn > _highestColumn || _columnCount == 0 || _rowCount == 0)
                return;
            const uint32_t r0 = _lowestRow, r1 = std::min(_highestRow, _rowCount - 1);
            const uint32_t c0 = _lowestColumn, c1 = std::min(_highestColumn, _columnCount - 1);
            _coalesceSeen.assign(_blocks.size(), 0);
            for (uint32_t row = r0; row <= r1; ++row)
            {
                for (uint32_t col = c0; col <= c1; ++col)
                {
                    const uint32_t start = row * _columnCount + col;
                    if (_blocks[start] == 0 || _coalesceSeen[start] != 0)
                        continue;
                    uint32_t minR = row, maxR = row, minC = col, maxC = col, count = 0;
                    _coalesceStack.clear();
                    _coalesceStack.push_back(start);
                    _coalesceSeen[start] = 1;
                    while (!_coalesceStack.empty())
                    {
                        const uint32_t index = _coalesceStack.back();
                        _coalesceStack.pop_back();
                        ++count;
                        const uint32_t r = index / _columnCount, c = index % _columnCount;
                        minR = std::min(minR, r);
                        maxR = std::max(maxR, r);
                        minC = std::min(minC, c);
                        maxC = std::max(maxC, c);
                        for (int32_t dr = -1; dr <= 1; ++dr)
                        {
                            for (int32_t dc = -1; dc <= 1; ++dc)
                            {
                                const int32_t nr = static_cast<int32_t>(r) + dr, nc = static_cast<int32_t>(c) + dc;
                                if (nr < static_cast<int32_t>(r0) || nr > static_cast<int32_t>(r1)
                                    || nc < static_cast<int32_t>(c0) || nc > static_cast<int32_t>(c1))
                                    continue;
                                const uint32_t n = static_cast<uint32_t>(nr) * _columnCount + static_cast<uint32_t>(nc);
                                if (_blocks[n] != 0 && _coalesceSeen[n] == 0)
                                {
                                    _coalesceSeen[n] = 1;
                                    _coalesceStack.push_back(n);
                                }
                            }
                        }
                    }
                    const uint32_t area = (maxR - minR + 1) * (maxC - minC + 1);
                    if (count == area || count * 100 < area * minFillPercent)
                        continue;
                    for (uint32_t r = minR; r <= maxR; ++r)
                    {
                        for (uint32_t c = minC; c <= maxC; ++c)
                        {
                            _blocks[r * _columnCount + c] = 1;
                            _coalesceSeen[r * _columnCount + c] = 1;
                        }
                    }
                }
            }
        }

        template<typename F>
        void traverseDirtyCells(F&& func)
        {
            const uint32_t columnCount = _columnCount;
            const uint32_t rowCount = _rowCount;
            const uint32_t blockWidth = _blockWidth;
            const uint32_t blockHeight = _blockHeight;
            auto& blocks = _blocks;

            const uint32_t startRow = rowCount ? std::clamp(_lowestRow, 0u, rowCount - 1) : 0;
            const uint32_t endRow = rowCount ? std::clamp(_highestRow, 0u, rowCount - 1) : 0;
            const uint32_t startCol = columnCount ? std::clamp(_lowestColumn, 0u, columnCount - 1) : 0;
            const uint32_t endCol = columnCount ? std::clamp(_highestColumn, 0u, columnCount - 1) : 0;

            for (uint32_t currentRow = startRow; currentRow <= endRow; ++currentRow)
            {
                for (uint32_t currentCol = startCol; currentCol <= endCol;)
                {
                    const uint32_t blockIndex = currentRow * columnCount + currentCol;

                    if (blocks[blockIndex] == 0)
                    {
                        ++currentCol;
                        continue;
                    }

                    // Horizontal merge.
                    uint32_t horizontalSpan = 1;
                    while (currentCol + horizontalSpan < columnCount
                           && blocks[currentRow * columnCount + currentCol + horizontalSpan] != 0)
                    {
                        ++horizontalSpan;
                    }

                    // Vertical merge.
                    uint32_t mergedWidth = horizontalSpan;
                    uint32_t verticalSpan = 0;
                    for (uint32_t rowOffset = 1; currentRow + rowOffset <= endRow; ++rowOffset)
                    {
                        uint32_t validWidth = 0;
                        while (validWidth < mergedWidth
                               && blocks[(currentRow + rowOffset) * columnCount + currentCol + validWidth] != 0)
                        {
                            ++validWidth;
                        }
                        if (validWidth == 0)
                            break;
                        mergedWidth = validWidth;
                        ++verticalSpan;
                    }

                    const uint32_t totalRows = verticalSpan + 1;

                    // Clear blocks
                    for (uint32_t y = currentRow; y < currentRow + totalRows; ++y)
                    {
                        for (uint32_t x = currentCol; x < currentCol + mergedWidth; ++x)
                        {
                            blocks[y * columnCount + x] = 0;
                        }
                    }

                    const uint32_t coordLeft = currentCol * blockWidth;
                    const uint32_t coordTop = currentRow * blockHeight;
                    const uint32_t coordRight = (currentCol + mergedWidth) * blockWidth;
                    const uint32_t coordBottom = (currentRow + totalRows) * blockHeight;

                    const uint32_t clampedRight = std::min(coordRight, _screenWidth);
                    const uint32_t clampedBottom = std::min(coordBottom, _screenHeight);

                    if (coordLeft < _screenWidth && coordTop < _screenHeight)
                    {
                        func(coordLeft, coordTop, clampedRight, clampedBottom);
                    }

                    // Skip processed columns
                    currentCol += mergedWidth;
                }
            }

            _lowestRow = std::numeric_limits<uint32_t>::max();
            _highestRow = 0;
            _lowestColumn = std::numeric_limits<uint32_t>::max();
            _highestColumn = 0;
        }
    };

} // namespace OpenRCT2::Drawing
