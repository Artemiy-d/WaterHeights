#include "Widget.h"

#include "HeightsEngine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QShortcut>
#include <QWheelEvent>

#include <math.h>


Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);

    auto addShortcut = new QShortcut(QKeySequence(Qt::Key_1), this);
    connect(addShortcut, &QShortcut::activated, [this]()
    {
        changeMap(1);
    });

    auto remShortcut = new QShortcut(QKeySequence(Qt::Key_2), this);
    connect(remShortcut, &QShortcut::activated, [this]()
    {
        changeMap(-1);
    });
}

Widget::~Widget()
{
}

void Widget::resizeEvent(QResizeEvent *)
{
    groundMap.reset(new Map({static_cast<unsigned int>(width()), static_cast<unsigned int>(height())}));
    image = QImage(size(), QImage::Format_RGB32);

    updateImage();
}

void Widget::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    p.drawImage(0, 0, image);
}

void Widget::updateImage()
{
    waterHeights = calculateWater(*groundMap);

    QColor blue(Qt::blue);
    QColor gray(Qt::gray);

    auto index = 0;
    for (int i = 0; i < height(); ++i)
        for (int j = 0; j < width(); ++j)
        {
            const auto h = waterHeights.first[index] ? waterHeights.first[index] : (*groundMap)[index];
            const auto& c = waterHeights.first[index] ? blue : gray;

            image.setPixelColor(j, i, c.darker(100 + h));
            ++index;
        }

    setWindowTitle("Water: " + QString::number(waterHeights.second));

    repaint();
}

void Widget::mousePressEvent(QMouseEvent *event)
{
    changeMap(event->button() == Qt::LeftButton ? 1 : -1, event->pos());
}

void Widget::mouseMoveEvent(QMouseEvent *event)
{
    updateTooltip(event->pos());
}

void Widget::updateTooltip(const QPoint& pos)
{
    auto index = width() * pos.y() + pos.x();
    if (rect().contains(pos))
    {
        QString text = "Ground: " + QString::number((*groundMap)[index]);

        if (waterHeights.first[index])
        {
            text += "\nWater: " + QString::number(waterHeights.first[index]);
        }
        setToolTip(text);
    }
}

void Widget::changeMap(int k, const QPoint& pos)
{
    for (int x = std::max(0, pos.x() - brushSize); x <= std::min(width() - 1, pos.x() + brushSize); ++x)
        for (int y = std::max(0, pos.y() - brushSize); y <= std::min(height() - 1, pos.y() + brushSize); ++y)
        {
            auto r = sqrt((x - pos.x()) * (x - pos.x()) + (y - pos.y()) * (y - pos.y()));

            if (r <= 10)
            {
                (*groundMap)[y * width() + x] += k * (brushSize + 2 - r) / 2;
            }
        }

    updateImage();
    updateTooltip(pos);
}

void Widget::changeMap(int k)
{
    changeMap(k, mapFromGlobal(QCursor::pos()));
}

void Widget::wheelEvent(QWheelEvent *event)
{
    brushSize = qBound(4, 20, brushSize + event->delta());
}
