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


class ExtendedHeights
{
public:
    using value_type = size_t;

    ExtendedHeights(size_t s, size_t h = 0) :
        height(h + 2),
        base(s)
    {}

    void resize(size_t size)
    {
        base.resize(size);
    }

    size_t operator [] (size_t index) const
    {
        assert(height > 2);
        return base[index] ? 100500 : index % height;
    }

    decltype(std::declval<std::vector<bool>>()[0]) operator [] (size_t index)
    {
        return base[index];
    }

    size_t size() const
    {
        return base.size();
    }

private:
    size_t height;
    std::vector<bool> base;
};

template <typename T>
using Array3 = std::array<T, 3>;
using Map3 = Map<Array3, ExtendedHeights>;


using Distribution = std::uniform_int_distribution<>;

Map3 createMap3(const UIMap& m)
{
    int mx = std::numeric_limits<int>::min();
    int mn = std::numeric_limits<int>::max();
    for (size_t x = 0; x < m.getSize(0); ++x)
        for (size_t y = 0; y < m.getSize(1); ++y)
        {
            const auto h = m.getHeight(x, y);
            if (h < mn)
                mn = h;
            if (h > mx)
                mx = h;
        }

    Map3 result({{size_t(mx - mn + 1), m.getSize(0), m.getSize(1)}}, 0, size_t(mx - mn + 1));

    for (size_t x = 0; x < result.getSize(1); ++x)
        for (size_t y = 0; y < result.getSize(2); ++y)
        {
            const auto h = size_t(m.getHeight(x, y) - mn);
            for (size_t z = 0; z < h; ++z)
                result.getHeight(z, x, y) = true;

            for (size_t z = 0; z < result.getSize(0); ++z)
            {
                if (z < h)
                {
                    assert(result.getHeights()[ result.getHeightIndex(z, x, y) ] == 100500);
                }
                else
                {
                    auto i = result.getHeightIndex(z, x, y);
                    auto t = result.getHeights()[i];
                    assert(t == z + 1);
                }
            }
        }
    return result;
}

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      randGen(0),
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
    groundMap.reset(new UIMap({static_cast<size_t>(width()), static_cast<size_t>(height())}));
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

    const auto& groundHeights = groundMap->getHeights();

    const auto w = width();
    const auto h = height();

    auto index = w + 3;
    for (int i = 0; i < h; ++i)
    {
        auto line = (QRgb*)image.scanLine(i);

        for (int j = 0; j < w; ++j)
        {
            line[j] = (waterHeights[index] ? waterColors : groundColors)
                      (waterHeights[index] ? waterHeights[index] : groundHeights[index]);
            index = ++index;
        }

        index += 2;
    }

    const auto t2 = std::chrono::steady_clock::now();

    setToolTip({});

  //  auto m3 = createMap3(*groundMap);

  //  auto m3Res = calculateWater3(m3);

    //assert(m3Res.square == waterResult.volume);
    setWindowTitle(QString("Volume: %1; Square: %2; CalcTime: %3; ImageTime: %4;").arg(
                       QString::number(waterResult.volume),
                       QString::number(float(waterResult.square) / (w * h)),
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
    const auto index = groundMap->getHeightIndex(pos.x(), pos.y());
    if (rect().contains(pos))
    {
        QString text = "Ground: " + QString::number(groundMap->getHeights()[index]);

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

    for (int x = rangeX.first; x <= rangeX.second; ++x)
        for (int y = rangeY.first; y <= rangeY.second; ++y)
        {
            auto r = static_cast<int>(sqrt((x - data.pos.x()) * (x - data.pos.x()) + (y - data.pos.y()) * (y - data.pos.y())));

            if (r <= data.brushSize)
            {
               groundMap->getHeight(x, y) += data.k * (data.brushSize + 2 - r) / 2;
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
           groundMap->getHeight(x + 1, y + 1) = groundLevel++;
           groundMap->getHeight(x, y) = groundLevel;
           groundMap->getHeight(x + 1, y) = groundLevel;
           groundMap->getHeight(x, y + 1) = groundLevel;
       }

    if (w & 1)
    {
        for (int y = 0; y < h; ++y)
            groundMap->getHeight(w - 1, y) = groundLevel;
    }

    if (h & 1)
    {
        for (int x = 0; x < w; ++x)
           groundMap->getHeight(x, h - 1) = groundLevel;
    }

    onMapReseted();
}

void Widget::setWorstCase()
{
    auto w = width();
    auto h = height();

    int groundLevel = 0;

    const int delta = w / 2 + 2;

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            groundMap->getHeight(x, y) = (x + y) & 1 ? ++groundLevel : groundLevel - delta;
        }

    onMapReseted();
}

void Widget::setRandomCase()
{
    auto w = width();
    auto h = height();


    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            groundMap->getHeight(x, y) = Distribution(0, 100)(randGen);

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
