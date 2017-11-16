#pragma once

#include <QWidget>

#include "MapChagnges.h"

class Map;
class QShortcut;

class Widget : public QWidget, private MapChangable
{
    Q_OBJECT

public:
    Widget(QWidget *parent = 0);

    ~Widget();

protected:

    void resizeEvent(QResizeEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

private:

    void updateImage();

    void updateTooltip(const QPoint& pos);

    void updateShortcuts();

    void changeMap(const MapChangeData& data) override;

    void changeMap(int k, const QPoint& pos);

    void changeMap(int k);

private:

    MapChanges mapChanges;

    std::pair<std::vector<int>, int> waterHeights;

    std::unique_ptr<Map> groundMap;

    int brushSize = 10;

    QImage image;

    QShortcut* undoShortcut;
    QShortcut* redoShortcut;
};

