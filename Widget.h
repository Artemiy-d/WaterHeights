#pragma once

#include <QWidget>

class Map;

class Widget : public QWidget
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

    void changeMap(int k, const QPoint& pos);

    void changeMap(int k);

private:

    std::pair<std::vector<int>, int> waterHeights;

    std::unique_ptr<Map> groundMap;

    int brushSize = 10;

    QImage image;
};

