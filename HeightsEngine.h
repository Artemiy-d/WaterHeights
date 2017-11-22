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

template <typename Container, typename... Args>
auto getDefaultContainer(Args&&... args) -> decltype(Container(args...))
{
    return Container(std::forward<Args>(args)...);
}

template <typename Container>
Container getDefaultContainer(...)
{
    return {};
}


template <template <class...> class Container>
class Map
{
public:
    using Sizes = Container<unsigned int>;

    Map(Sizes s, Heights h = {}) :
        heights(std::move(h)),
        sizes(std::move(s)),
        dimensions(getDefaultContainer<Sizes>(sizes.size()))
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
    void forEachInRange(Container<size_t>& body, const Container<std::pair<size_t, size_t>>& ranges, Handler&& handler) const
    {
        Index index = 0;
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            assert(ranges[i].first <= ranges[i].second);
            body[i] = ranges[i].first;
            index += dimensions[i] * body[i];
        }

        for (size_t i = 0; i < ranges.size(); )
        {
            handler(index);

            for (i = 0; i < ranges.size(); )
            {
                if (body[i] == ranges[i].second)
                {
                    body[i] = ranges[i].first;
                    index -= (ranges[i].second - body[i]) * dimensions[i];
                    ++i;
                }
                else
                {
                    ++body[i];
                    index += dimensions[i];
                    break;
                }
            }
        }
    }

    template <typename Handler>
    void forEachBorderIndex(Handler&& handler) const
    {
        if (std::any_of(sizes.begin(), sizes.end(), std::bind(std::less<size_t>(), std::placeholders::_1, size_t(3))))
        {
            for (size_t i = 1; i < heights.size(); ++i)
                handler(i);
        }
        else
        {
            auto body = getDefaultContainer<Container<size_t>>(dimensions.size());
            auto ranges = getDefaultContainer<Container<std::pair<size_t, size_t>>>(dimensions.size());

            for (size_t i = 0; i < sizes.size(); ++i)
            {
                ranges[i].first = 1;
                ranges[i].second = sizes[i] - 2;
            }

            for (size_t i = 0; i < sizes.size(); ++i)
            {
                const auto last = sizes[i] - 1;
                if (last)
                {
                    ranges[i].first = ranges[i].second = last;
                    forEachInRange(body, ranges, handler);
                }

                ranges[i].first = ranges[i].second = 0;
                forEachInRange(body, ranges, handler);

                ranges[i].second = last;
            }
        }
    }

    template <typename IndicesArray, typename Handler>
    void bfs(IndicesArray& indices, size_t prevSize, Handler&& handler, bool handleBase = false) const
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

private:

    std::vector<int> heights;
    Sizes sizes;
    Sizes dimensions;
};


struct HeightsResult
{
    HeightsResult(size_t mapSize) : heights(mapSize) {}

    Heights heights;
    Height volume = 0;
    size_t square = 0;
};

template <typename HeightsMap>
HeightsResult calculateWater(const HeightsMap& m, int waterLevel = 0)
{
    Indices groundBorders;
    Indices waterBorders;

    size_t prevGroundBordersCount = 0;
    std::vector<CellType> cells(m.getCellsCount());
    HeightsResult result(m.getCellsCount());

    auto isUnknowCell = [&](Index nearest)
    {
        return cells[nearest] == CellType::Unknown;
    };

    auto setWaterCell = [&](Index index)
    {
        assert(cells[index] == CellType::Unknown);
        assert(m[index] < waterLevel);

        cells[index] = CellType::Water;
        result.heights[index] = waterLevel - m[index];
        result.volume += result.heights[index];
        ++result.square;

        waterBorders.push_back(index);
    };

    auto setGroundCell = [&](Index index)
    {
        cells[index] = CellType::Ground;
        groundBorders.push_back(index);
    };

    auto setWaterOrGroundCell = [&](Index index)
    {
        if (m[index] < waterLevel)
            setWaterCell(index);
        else
            setGroundCell(index);
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
                setGroundCell(index);
            }
        });

        waterLevel = std::numeric_limits<int>::max();

        groundBorders.erase(std::remove_if(groundBorders.begin(), groundBorders.end(), [&](Index index)
        {
            if (!m.findNearest(index, isUnknowCell))
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

template <typename HeightsMap>
HeightsResult calculateWater2(const HeightsMap& m, int waterLevel = 0)
{
    Indices groundBorders;
    Indices waterBorders;

    size_t prevGroundBordersCount = 0;
    std::vector<CellType> cells(m.getCellsCount());
    HeightsResult result(m.getCellsCount());

    auto isUnknowCell = [&](Index nearest)
    {
        return cells[nearest] == CellType::Unknown;
    };

    auto setWaterCell = [&](Index index)
    {
        assert(cells[index] == CellType::Unknown);
        assert(m[index] < waterLevel);

        cells[index] = CellType::Water;
        result.heights[index] = waterLevel - m[index];
        result.volume += result.heights[index];
        ++result.square;

        waterBorders.push_back(index);
    };

    auto setGroundCell = [&](Index index)
    {
        cells[index] = CellType::Ground;
        groundBorders.push_back(index);
    };

    auto setWaterOrGroundCell = [&](Index index)
    {
        if (m[index] < waterLevel)
            setWaterCell(index);
        else
            setGroundCell(index);
    };

    m.forEachBorderIndex(setWaterOrGroundCell);

    std::map<int, Indices> heightToIndices;

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

        m.bfs(groundBorders, 0, [&](const Index& orig, const Index& index)
        {
            assert(!(cells[index] == CellType::Water && m[index] > m[orig]));
            if (cells[index] == CellType::Unknown && m[index] >= m[orig])
            {
                setGroundCell(index);
            }
        });

        for (size_t i = prevGroundBordersCount; i < groundBorders.size(); ++i)
        {
            const auto index = groundBorders[i];

            if (m.findNearest(index, isUnknowCell))
            {
                heightToIndices[m[index]].push_back(index);
            }
        }

        groundBorders.clear();

        if (!heightToIndices.empty())
        {
            auto it = heightToIndices.begin();
            waterLevel = it->first;
            groundBorders = std::move(it->second);
            heightToIndices.erase(it);

            for (auto groundBorderIndex : groundBorders)
            {
                assert(m[groundBorderIndex] == waterLevel);
                m.forEachNearest(groundBorderIndex, [&](Index nearest)
                {
                    if (cells[nearest] == CellType::Unknown)
                    {
                        assert(m[nearest] < waterLevel);
                        setWaterCell(nearest);
                    }
                });
            }
        }

        prevGroundBordersCount = groundBorders.size();
    };

    assert(heightToIndices.empty());

    return result;
}

template <typename HeightsMap>
HeightsResult calculateWater3(const HeightsMap& m, int waterLevel = 0)
{
    Indices groundBorders;
    Indices waterBorders;

    size_t prevGroundBordersCount = 0;
    std::vector<CellType> cells(m.getCellsCount());
    HeightsResult result(m.getCellsCount());

    std::multimap<Height, std::pair<size_t, size_t>> heightsToRanges;

    auto isUnknowCell = [&](Index nearest)
    {
        return cells[nearest] == CellType::Unknown;
    };

    auto setWaterCell = [&](Index index)
    {
        assert(cells[index] == CellType::Unknown);
        assert(m[index] < waterLevel);

        cells[index] = CellType::Water;
        result.heights[index] = waterLevel - m[index];
        result.volume += result.heights[index];
        ++result.square;

        waterBorders.push_back(index);
    };

    auto setGroundCell = [&](Index index)
    {
        cells[index] = CellType::Ground;
        groundBorders.push_back(index);
    };

    auto setWaterOrGroundCell = [&](Index index)
    {
        if (m[index] < waterLevel)
            setWaterCell(index);
        else
            setGroundCell(index);
    };

    m.forEachBorderIndex(setWaterOrGroundCell);

    while (prevGroundBordersCount < groundBorders.size() || !waterBorders.empty())
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
                setGroundCell(index);
            }
        });

        groundBorders.erase(std::remove_if(groundBorders.begin() + prevGroundBordersCount, groundBorders.end(), [&](Index index)
        {
            return !m.findNearest(index, isUnknowCell);
        }), groundBorders.end());

        if (prevGroundBordersCount < groundBorders.size())
        {
            std::sort(groundBorders.begin() + prevGroundBordersCount, groundBorders.end(), [&](Index a, Index b)
            {
                return m[a] < m[b];
            });

            heightsToRanges.emplace(m[groundBorders[prevGroundBordersCount]], std::make_pair(prevGroundBordersCount, groundBorders.size()));
        }

        while (!heightsToRanges.empty() && waterBorders.empty())
        {
            auto it = heightsToRanges.begin();
            waterLevel = it->first;
            do
            {
                auto range = it->second;
                it = heightsToRanges.erase(it);

                for (; range.first < range.second && m[groundBorders[range.first]] == waterLevel; ++range.first)
                {
                    m.forEachNearest(groundBorders[range.first], [&](Index nearest)
                    {
                        if (cells[nearest] == CellType::Unknown)
                        {
                            assert(m[nearest] < waterLevel);
                            setWaterCell(nearest);
                        }
                    });
                }

                if (range.first < range.second)
                {
                    assert(m[groundBorders[range.first]] > waterLevel);

                    heightsToRanges.emplace(m[groundBorders[range.first]], range);
                    it = heightsToRanges.begin();
                }

            } while (!heightsToRanges.empty() && it->first == waterLevel);
        }

        prevGroundBordersCount = groundBorders.size();
    };

    assert(heightsToRanges.empty());

    return result;
}

