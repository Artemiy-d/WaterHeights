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


using Distribution = std::uniform_int_distribution<>;

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      randGen(time(0)),
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

    auto randomChangeMap = [this](size_t count)
    {
        while (count--)
        {
            const MapChangeData data = {1, QPoint(Distribution(0, width() - 1)(randGen), Distribution(0, height() - 1)(randGen)), Distribution(4, 14)(randGen)};
            changeMap(data, count == 0, true);
        }
    };

    connect(timer, &QTimer::timeout, std::bind(randomChangeMap, 1));

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

    auto changeMapShortcut = new QShortcut(QKeySequence(Qt::Key_A), this);
    connect(changeMapShortcut, &QShortcut::activated, std::bind(randomChangeMap, 10000));

    auto wrongestCaseShortcut = new QShortcut(QKeySequence(Qt::Key_W), this);
    connect(wrongestCaseShortcut, &QShortcut::activated, this, &Widget::setWorstCase);

    auto hardCaseShortcut = new QShortcut(QKeySequence(Qt::Key_H), this);
    connect(hardCaseShortcut, &QShortcut::activated, this, &Widget::setHardCase);

    auto randomCaseShortcut = new QShortcut(QKeySequence(Qt::Key_R), this);
    connect(randomCaseShortcut, &QShortcut::activated, this, &Widget::setRandomCase);

    updateShortcuts();
}

Widget::~Widget()
{
}

void Widget::resizeEvent(QResizeEvent *)
{
    groundMap.reset(new Map({static_cast<unsigned int>(width()), static_cast<unsigned int>(height())}));
    image = QImage(size(), QImage::Format_RGB32);

    onMapReseted();
}

void Widget::paintEvent(QPaintEvent *)
{
    QPainter(this).drawImage(0, 0, image);
}

void Widget::updateImage()
{   
    const auto t0 = std::chrono::steady_clock::now();
    auto waterResult = calculateWater3(*groundMap, waterLevel);
    waterHeights = std::move(waterResult.heights);
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
            line[j] = (waterHeights[index] ? waterColors : groundColors)
                      (waterHeights[index] ? waterHeights[index] : (*groundMap)[index]);
            ++index;
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    setToolTip({});

    setWindowTitle(QString("Volume: %1; Square: %2; CalcTime: %3; ImageTime: %4;").arg(
                       QString::number(waterResult.volume),
                       QString::number(float(waterResult.square) / waterHeights.size()),
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

        if (waterHeights[index])
        {
            text += "\nWater: " + QString::number(waterHeights[index]);
        }
        setToolTip(text);
    }
}

void Widget::updateShortcuts()
{
    undoShortcut->setEnabled(mapChanges.canUndo());
    redoShortcut->setEnabled(mapChanges.canRedo());
}

void Widget::changeMap(const MapChangeData& data, bool updateUI, bool addChangeAction)
{
    const auto rangeX = std::make_pair(std::max(0, data.pos.x() - data.brushSize), std::min(width() - 1, data.pos.x() + data.brushSize));
    const auto rangeY = std::make_pair(std::max(0, data.pos.y() - data.brushSize), std::min(height() - 1, data.pos.y() + data.brushSize));

    auto w = width();

    for (int x = rangeX.first; x <= rangeX.second; ++x)
        for (int y = rangeY.first; y <= rangeY.second; ++y)
        {
            auto r = static_cast<int>(sqrt((x - data.pos.x()) * (x - data.pos.x()) + (y - data.pos.y()) * (y - data.pos.y())));

            if (r <= data.brushSize)
            {
                (*groundMap)[y * w + x] += data.k * (data.brushSize + 2 - r) / 2;
            }
        }


    if (addChangeAction)
    {
        mapChanges.addChange(data);
    }

    if (updateUI)
    {
        updateImage();
        updateShortcuts();
    }
}

void Widget::setHardCase()
{
    auto w = width();
    auto h = height();

    int groundLevel = 0;

    for (int y = 0; y < h; y += 2)
       for (int x = 0; x < w; x += 2)
       {
           auto base = w * y + x;
           (*groundMap)[base + 1 + w] = groundLevel++;
           (*groundMap)[base] = groundLevel;
           (*groundMap)[base + 1] = groundLevel;
           (*groundMap)[base + w] = groundLevel;
       }

    if (w & 1)
    {
        for (int y = 0; y < h; ++y)
            (*groundMap)[w * (y + 1) - 1] = groundLevel;
    }

    if (h & 1)
    {
        for (int x = 0; x < w; ++x)
           (*groundMap)[w * (h - 1) + x] = groundLevel;
    }

    onMapReseted();
}

void Widget::setWorstCase()
{
    auto w = width();
    auto h = height();

    int groundLevel = 0;

    const int delta = w / 2 + 2;

    size_t index = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            (*groundMap)[index++] = (x + y) & 1 ? ++groundLevel : groundLevel - delta;
        }

    onMapReseted();
}

void Widget::setRandomCase()
{
    for (size_t i = 0; i < groundMap->getCellsCount(); ++i)
        (*groundMap)[i] = Distribution(0, 100)(randGen);

    onMapReseted();
}

void Widget::changeMap(const MapChangeData& data)
{
    changeMap(data, true, false);
}

void Widget::changeMap(int k, const QPoint& pos, bool updateUI)
{
    changeMap({k, pos, brushSize}, updateUI, true);
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

void Widget::onMapReseted()
{
    updateImage();
    mapChanges.clear();
    updateShortcuts();
}
