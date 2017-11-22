#pragma once

#include <QWidget>
#include <random>


#include "MapChagnges.h"

template <typename ValueType>
using Array2 = std::array<ValueType, 2>;

template <template <class...> class Container>
class Map;

using UIMap = Map<Array2>;

class QShortcut;

class Widget : public QWidget, private MapChangable
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);

    ~Widget();

protected:

    bool event(QEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

private:

    void updateImage();

    void updateTooltip();

    void updateShortcuts();

    void changeMap(const MapChangeData& data, bool updateUI, bool addChangeAction);

    void changeMap(const MapChangeData& data) override;

    void changeMap(int k, const QPoint& pos, bool updateUI = true);

    void changeMap(int k);

    void changeWaterLevel(int k);

    void setWorstCase();

    void setHardCase();

    void setRandomCase();

    void onMapReseted();

private:

    std::mt19937 randGen;

    MapChanges mapChanges;

    std::vector<int> waterHeights;

    std::unique_ptr<UIMap> groundMap;

    int brushSize = 10;
    int waterLevel = 0;

    QImage image;

    QShortcut* undoShortcut;
    QShortcut* redoShortcut;
};

