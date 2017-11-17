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
    Ground
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
                const auto orig = indices[j];
                forEachNearest(orig, [&](Index nearest)
                {
                    handler(orig, nearest);
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

std::pair<Heights, Height> calculateWater(const Map& m, int waterLevel = 0)
{
    Indices groundBorders;
    Indices waterBorders;

    size_t prevGroundBordersCount = 0;
    std::vector<CellType> cells(m.getCellsCount());
    auto result = std::make_pair(Heights(m.getCellsCount()), 0);

    auto setWaterCell = [&](Index index)
    {
        assert(cells[index] == CellType::Unknown);
        assert(m[index] < waterLevel);

        cells[index] = CellType::Water;
        result.first[index] = waterLevel - m[index];
        result.second += result.first[index];
    };

    auto setWaterOrGroundCell = [&](Index index)
    {
        if (m[index] < waterLevel)
        {
            setWaterCell(index);
            waterBorders.push_back(index);
        }
        else
        {
            cells[index] = CellType::Ground;
            groundBorders.push_back(index);
        }
    };

    m.forEachBorderIndex(setWaterOrGroundCell);

    while (!groundBorders.empty() || !waterBorders.empty())
    {
        m.bfs(waterBorders, 0, [&](Index orig, Index index)
        {
            assert(cells[index] != CellType::Ground || m[orig] < m[index]);
            if (cells[index] == CellType::Unknown)
            {
                setWaterOrGroundCell(index);
            }
        });

        waterBorders.clear();

        m.bfs(groundBorders, prevGroundBordersCount, [&](const Index& orig, const Index& index)
        {
            assert(!(cells[index] == CellType::Water && m[index] > m[orig]));
            if (cells[index] == CellType::Unknown && m[index] >= m[orig])
            {
                cells[index] = CellType::Ground;
                groundBorders.push_back(index);
            }
        });

        waterLevel = std::numeric_limits<int>::max();

        groundBorders.erase(std::remove_if(groundBorders.begin(), groundBorders.end(), [&](Index index)
        {
            auto isBorder = m.findNearest(index, [&](Index nearest)
            {
                return cells[nearest] == CellType::Unknown;
            });

            if (!isBorder)
                return true;

            waterLevel = std::min(waterLevel, m[index]);
            return false;
        }), groundBorders.end());

        prevGroundBordersCount = groundBorders.size();

        for (auto groundBorderIndex : groundBorders)
        {
            if (m[groundBorderIndex] == waterLevel)
            {
                m.forEachNearest(groundBorderIndex, [&](Index nearest)
                {
                    if (cells[nearest] == CellType::Unknown)
                    {
                        assert(m[nearest] < waterLevel);
                        setWaterCell(nearest);

                        waterBorders.push_back(nearest);
                    }
                });
            }
        }
    };


   /* auto h = result.first;

    for (size_t index = 0; index < h.size(); ++index)
    {
        assert(cells[index] == CellType::Ground || cells[index] == CellType::Water);
        if (h[index] > 0)
        {
            int m1 = -1;
            int m2 = 100500;
            int m3 = 100500;
            Indices indices = { index };
            bool isBorder = false;
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

                    if (m.isBorder(ind))
                    {
                        isBorder = true;
                        assert(m3 == 100500 || m3 == h[ind]);
                        m3 = h[ind];
                    }

                    h[ind] = -1;

                    indices.push_back(ind);
                }
                else if (h[ind] == 0)
                {
                    assert(cells[ind] == CellType::Ground);
                    assert(cells[orig] == CellType::Water);

                    m2 = std::min(m2, m[ind]);
                    assert(m[ind] > m[orig]);

                }
            }, true);

            assert(m3 != 100500 || m1 == m2);
        }
    }
*/
    return result;
}

