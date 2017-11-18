#include "Widget.h"

#include "HeightsEngine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QShortcut>
#include <QWheelEvent>
#include <QTimer>

#include <math.h>
#include <chrono>
#include <unordered_map>
#include <random>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      mapChanges(*this)
{
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

    auto increaseWaterLevelShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(increaseWaterLevelShortcut, &QShortcut::activated, std::bind(&Widget::changeWaterLevel, this, 1));

    auto decreaseWaterLevelShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(decreaseWaterLevelShortcut, &QShortcut::activated, std::bind(&Widget::changeWaterLevel, this, -1));

    auto timer = new QTimer(this);

    auto randomChangeMap = [this](std::mt19937& gen)
    {
        using Distribution = std::uniform_int_distribution<>;
        changeMap(1, QPoint(Distribution(0, width() - 1)(gen), Distribution(0, height() - 1)(gen)));
    };

    connect(timer, &QTimer::timeout, std::bind(randomChangeMap, std::mt19937(time(0))));

    auto timerShortcut = new QShortcut(QKeySequence(Qt::Key_T), this);
    connect(timerShortcut, &QShortcut::activated, [=]()
    {
        if (timer->isActive())
        {
            timer->stop();
        }
        else
        {
            timer->start(0);
        }
    });

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
    QPainter(this).drawImage(0, 0, image);
}

void Widget::updateImage()
{
    const auto t0 = std::chrono::steady_clock::now();
    waterHeights = calculateWater(*groundMap, waterLevel);
    const auto t1 = std::chrono::steady_clock::now();

    class ColorCache
    {
    public:
        ColorCache(const QColor& c) : color(c)
        {}

        QRgb operator () (int h)
        {
            auto found = cache.find(h);
            return (found == cache.end() ? cache.emplace(h, color.darker(100 + h).rgb()).first : found)->second;
        }
    private:
        QColor color;
        std::unordered_map<int, QRgb> cache;
    };

    ColorCache waterColors(QColor(Qt::blue).lighter(120));
    ColorCache groundColors(Qt::gray);

    auto index = 0;
    const auto w = width();
    for (int i = 0; i < height(); ++i)
    {
        auto line = (QRgb*)image.scanLine(i);
        for (int j = 0; j < w; ++j)
        {
            line[j] = (waterHeights.first[index] ? waterColors : groundColors)
                      (waterHeights.first[index] ? waterHeights.first[index] : (*groundMap)[index]);
            ++index;
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    setWindowTitle(QString("Water: %1; CalcTime: %2; ImageTime: %3;").arg(QString::number(waterHeights.second),
                                                                          QString::number(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()),
                                                                          QString::number(std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count())));

    repaint();
}

void Widget::mousePressEvent(QMouseEvent *event)
{
    changeMap(event->button() == Qt::LeftButton ? 1 : -1, event->pos());
}

void Widget::mouseMoveEvent(QMouseEvent *event)
{
    changeMap(event->buttons().testFlag(Qt::LeftButton) ? 1 : -1, event->pos());
}

void Widget::updateTooltip()
{
    const auto pos = mapFromGlobal(QCursor::pos());
    const auto index = width() * pos.y() + pos.x();
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
    const auto rangeX = std::make_pair(std::max(0, data.pos.x() - data.brushSize), std::min(width() - 1, data.pos.x() + data.brushSize));
    const auto rangeY = std::make_pair(std::max(0, data.pos.y() - data.brushSize), std::min(height() - 1, data.pos.y() + data.brushSize));

    for (int x = rangeX.first; x <= rangeX.second; ++x)
        for (int y = rangeY.first; y <= rangeY.second; ++y)
        {
            auto r = static_cast<int>(sqrt((x - data.pos.x()) * (x - data.pos.x()) + (y - data.pos.y()) * (y - data.pos.y())));

            if (r <= data.brushSize)
            {
                (*groundMap)[y * width() + x] += data.k * (data.brushSize + 2 - r) / 2;
            }
        }

    updateImage();
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

void Widget::changeWaterLevel(int k)
{
    waterLevel += k;

    updateImage();
    updateTooltip();
}

void Widget::wheelEvent(QWheelEvent *event)
{
    brushSize = qBound(4, 20, brushSize + event->delta());
}

bool Widget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        updateTooltip();
    }
    return QWidget::event(event);
}
