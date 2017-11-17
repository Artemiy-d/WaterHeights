#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <map>
#include <iostream>
#include <cassert>

using Index = size_t;
using Indices = std::vector<Index>;
using Height = int;
using Heights = std::vector<Height>;

enum class CellType
{
    Unknown,
    Water,
    Ground,
    WaterBorder
};

class Map
{
public:
    Map(std::vector<unsigned int> s, Heights h = {}) :
        heights(std::move(h)),
        sizes(std::move(s)),
        dimensions(sizes.size())
    {
        if (!dimensions.empty())
        {
            dimensions[0] = 1;

            for (size_t i = 1; i < dimensions.size(); ++i)
            {
                dimensions[i] = dimensions[i - 1] * sizes[i - 1];
            }
        }

        const auto neededSize = !dimensions.empty() ? dimensions.back() * sizes.back() : 0u;

        if (heights.empty())
        {
            heights.resize(neededSize);
        }
        else if (neededSize != heights.size())
        {
            throw std::logic_error("Bad input");
        }
    }

    size_t getSubIndex(Index index, size_t number) const
    {
        return (index / getDim(number)) % sizes[number];
    }

    Height operator [] (Index index) const
    {
        return heights[index];
    }

    Height& operator [] (Index index)
    {
        return heights[index];
    }

    size_t getDim(size_t number) const
    {
        return dimensions[number];
    }

    bool isBorder(const Index& index) const
    {
        for (size_t i = 0; i < sizes.size(); ++i)
        {
            auto subIndex = getSubIndex(index, i);
            if (subIndex == 0 || subIndex + 1 == sizes[i])
                return true;
        }

        return false;
    }


    template <typename Handler>
    bool findNearest(Index index, Handler&& handler) const
    {
        for (size_t i = 0; i < sizes.size(); ++i)
        {
            auto subIndex = getSubIndex(index, i);

            if (subIndex)
            {
                if (handler(index - dimensions[i]))
                    return true;
            }

            if (subIndex + 1 < sizes[i])
            {
                if (handler(index + dimensions[i]))
                    return true;
            }
        }

        return false;
    }

    template <typename Handler>
    void forEachNearest(Index index, Handler&& handler) const
    {
        findNearest(index, [&](Index nearest)
        {
            handler(nearest);
            return false;
        });
    }

    template <typename Handler>
    void forEachBorderIndex(Handler&& handler) const
    {
        for (size_t i = 1; i < heights.size(); ++i)
        {
            if (isBorder(i))
                handler(i);
        }
    }

    template <typename Handler>
    void bfs(Indices& indices, size_t prevSize, Handler&& handler, bool handleBase = false) const
    {
        if (handleBase)
        {
            for (auto index : indices)
            {
                handler(index, index);
            }
        }

        do
        {
            size_t j = prevSize;
            prevSize = indices.size();
            for (; j < prevSize; ++j)
            {
                forEachNearest(indices[j], [&](Index nearest)
                {
                    if (handler(indices[j], nearest))
                    {
                        indices.push_back(nearest);
                    }
                });
            }
        } while (prevSize != indices.size());
    }

    size_t getCellsCount() const
    {
        return heights.size();
    }

private:

    std::vector<int> heights;
    std::vector<unsigned int> sizes;
    std::vector<unsigned int> dimensions;
};

std::pair<Heights, Height> calculateWater(const Map& m)
{
    Indices groundBorders;
    size_t prevGroundBordersCount = 0;
    std::vector<CellType> cells(m.getCellsCount());
    auto result = std::make_pair(Heights(m.getCellsCount()), 0);

    m.forEachBorderIndex([&](Index index)
    {
        cells[index] = CellType::Ground;
        groundBorders.push_back(index);
    });

    while (!groundBorders.empty())
    {
        m.bfs(groundBorders, prevGroundBordersCount, [&](const Index& orig, const Index& index)
        {
            assert(!(cells[index] == CellType::Water && m[index] > m[orig]));
            if (cells[index] == CellType::Unknown && m[index] >= m[orig])
            {
                cells[index] = CellType::Ground;
                return true;
            }

            return false;
        });

        auto minHeight = std::numeric_limits<int>::max();

        groundBorders.erase(std::remove_if(groundBorders.begin(), groundBorders.end(), [&](Index index)
        {
            auto isBorder = m.findNearest(index, [&](Index nearest)
            {
                return cells[nearest] == CellType::Unknown;
            });

            if (!isBorder)
                return true;

            minHeight = std::min(minHeight, m[index]);
            return false;
        }), groundBorders.end());

        prevGroundBordersCount = groundBorders.size();

        Indices waterIndices;
        for (auto groundBorderIndex : groundBorders)
        {
            if (m[groundBorderIndex] == minHeight)
            {
                m.forEachNearest(groundBorderIndex, [&](Index nearest)
                {
                    if (cells[nearest] == CellType::Unknown)
                    {
                        assert(m[nearest] < minHeight);
                        cells[nearest] = CellType::WaterBorder;
                        waterIndices.push_back(nearest);
                    }
                });
            }
        }

        m.bfs(waterIndices, 0, [&](Index orig, Index index)
        {
            if (cells[index] == CellType::Unknown || cells[index] == CellType::WaterBorder)
            {
                if (m[index] < minHeight)
                {
                    cells[index] = CellType::Water;
                    result.first[index] = minHeight - m[index];
                    result.second += result.first[index];
                    return true;
                }
                else
                {
                    cells[index] = CellType::Ground;
                    groundBorders.push_back(index);
                }
            }
            else if (cells[index] == CellType::Ground)
            {
                assert(m[orig] < m[index]);
            }

            return false;
        }, true);
    };

/*
    auto h = result.first;

    for (size_t index = 0; index < h.size(); ++index)
    {
        assert(cells[index] == CellType::Ground || cells[index] == CellType::Water);
        if (h[index] > 0)
        {
            int m1 = -1;
            int m2 = 100500;
            Indices indices = { index };
            m.bfs(indices, 0, [&](Index orig, Index ind)
            {

                if (h[ind] > 0)
                {
                    assert(cells[ind] == CellType::Water);
                    if (m1 > 0)
                    {
                        assert(m1 == h[ind] + m[ind]);
                    }

                    m1 = h[ind] + m[ind];
                    h[ind] = -1;

                    return true;
                }
                else if (h[ind] == 0)
                {
                    assert(cells[ind] == CellType::Ground);
                    assert(cells[orig] == CellType::Water);

                    m2 = std::min(m2, m[ind]);
                    assert(m[ind] > m[orig]);

                }

                return false;
            }, true);

            assert(m1 == m2);
        }
    }*/

    return result;
}
