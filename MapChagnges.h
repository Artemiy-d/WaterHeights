#pragma once

#include <QPoint>
#include <deque>
#include <cassert>


struct MapChangeData
{
    int k;
    QPoint pos;
    int brushSize;
};

class MapChangable
{
public:
    virtual ~MapChangable() = default;

    virtual void changeMap(const MapChangeData& change) = 0;
};

class MapChanges
{
public:

    MapChanges(MapChangable& c) : changable(c)
    {

    }

    void addChange(const MapChangeData& data)
    {
        changes.resize(position);
        if (position >= 1000)
            changes.pop_front();

        changes.push_back(data);
        ++position;
    }

    void undo()
    {
        assert(canUndo());
        const auto& data = changes[--position];
        changable.changeMap({-data.k, data.pos, data.brushSize});
    }

    void redo()
    {
        assert(canRedo());
        changable.changeMap(changes[position++]);
    }

    bool canUndo() const
    {
        return position != 0;
    }

    bool canRedo() const
    {
        return position < changes.size();
    }

private:

    MapChangable& changable;
    std::deque<MapChangeData> changes;
    size_t position = 0;
};
