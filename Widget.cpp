#include "Widget.h"

#include "HeightsEngine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QShortcut>
#include <QWheelEvent>

#include <math.h>


Widget::Widget(QWidget *parent)
    : QWidget(parent),
      mapChanges(*this)
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

    undoShortcut = new QShortcut(QKeySequence(Qt::Key_Z + Qt::CTRL), this);
    connect(undoShortcut, &QShortcut::activated, std::bind(&MapChanges::undo, &mapChanges));

    redoShortcut = new QShortcut(QKeySequence(Qt::Key_U + Qt::CTRL), this);
    connect(redoShortcut, &QShortcut::activated, std::bind(&MapChanges::redo, &mapChanges));

    updateShortcuts();
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
    if (event->buttons() == Qt::NoButton)
    {
        updateTooltip(event->pos());
    }
    else
    {
        changeMap(event->buttons().testFlag(Qt::LeftButton) ? 1 : -1, event->pos());
    }
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

void Widget::updateShortcuts()
{
    undoShortcut->setEnabled(mapChanges.canUndo());
    redoShortcut->setEnabled(mapChanges.canRedo());
}

void Widget::changeMap(const MapChangeData& data)
{
    for (int x = std::max(0, data.pos.x() - data.brushSize); x <= std::min(width() - 1, data.pos.x() + data.brushSize); ++x)
        for (int y = std::max(0, data.pos.y() - data.brushSize); y <= std::min(height() - 1, data.pos.y() + data.brushSize); ++y)
        {
            auto r = static_cast<int>(sqrt((x - data.pos.x()) * (x - data.pos.x()) + (y - data.pos.y()) * (y - data.pos.y())));

            if (r <= 10)
            {
                (*groundMap)[y * width() + x] += data.k * (data.brushSize + 2 - r) / 2;
            }
        }

    updateImage();
    updateTooltip(data.pos);
    updateShortcuts();
}

void Widget::changeMap(int k, const QPoint& pos)
{
    MapChangeData data = {k, pos, brushSize};
    changeMap(data);
    mapChanges.addChange(data);
}

void Widget::changeMap(int k)
{
    changeMap(k, mapFromGlobal(QCursor::pos()));
}

void Widget::wheelEvent(QWheelEvent *event)
{
    brushSize = qBound(4, 20, brushSize + event->delta());
}
