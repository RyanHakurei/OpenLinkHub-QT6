#include "widgets/FanCurveChart.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>

FanCurveChart::FanCurveChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_points = {{0, 30}, {100, 100}};
}

void FanCurveChart::setMaxTemperature(int maxTemperature)
{
    m_maxTemperature = qMax(10, maxTemperature);
    for (QPointF &point : m_points) {
        point.setX(qBound(0.0, point.x(), static_cast<double>(m_maxTemperature)));
    }
    sortPoints();
    update();
}

void FanCurveChart::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    if (m_points.size() < 2) {
        m_points = {{0, 0}, {static_cast<double>(m_maxTemperature), 100}};
    }
    for (QPointF &point : m_points) {
        point.setX(qBound(0.0, point.x(), static_cast<double>(m_maxTemperature)));
        point.setY(qBound(0.0, point.y(), 100.0));
    }
    sortPoints();
    m_dragIndex = -1;
    update();
}

void FanCurveChart::setLiveTemperature(double celsius)
{
    m_liveTemperature = celsius;
    m_hasLive = true;
    update();
}

void FanCurveChart::clearLiveTemperature()
{
    m_hasLive = false;
    update();
}

QVector<QPointF> FanCurveChart::points() const
{
    return m_points;
}

double FanCurveChart::speedAt(double temperature) const
{
    if (m_points.isEmpty()) {
        return 0;
    }
    if (temperature <= m_points.first().x()) {
        return m_points.first().y();
    }
    if (temperature >= m_points.last().x()) {
        return m_points.last().y();
    }
    for (int i = 0; i + 1 < m_points.size(); ++i) {
        const QPointF left = m_points.at(i);
        const QPointF right = m_points.at(i + 1);
        const double span = right.x() - left.x();
        if (span <= 0.0 || temperature < left.x() || temperature > right.x()) {
            continue;
        }
        const double ratio = (temperature - left.x()) / span;
        return left.y() + ratio * (right.y() - left.y());
    }
    return m_points.last().y();
}

QSize FanCurveChart::sizeHint() const
{
    return {420, 280};
}

QSize FanCurveChart::minimumSizeHint() const
{
    return {240, 180};
}

QRectF FanCurveChart::plotRect() const
{
    return QRectF(rect()).adjusted(52, 16, -16, -36);
}

QPointF FanCurveChart::toPixel(const QPointF &point) const
{
    const QRectF plot = plotRect();
    const double x = plot.left() + (point.x() / m_maxTemperature) * plot.width();
    const double y = plot.bottom() - (point.y() / 100.0) * plot.height();
    return {x, y};
}

QPointF FanCurveChart::fromPixel(const QPointF &pixel) const
{
    const QRectF plot = plotRect();
    const double temperature = ((pixel.x() - plot.left()) / plot.width()) * m_maxTemperature;
    const double speed = ((plot.bottom() - pixel.y()) / plot.height()) * 100.0;
    return {qBound(0.0, temperature, static_cast<double>(m_maxTemperature)), qBound(0.0, speed, 100.0)};
}

int FanCurveChart::pointAt(const QPointF &pixel) const
{
    for (int i = 0; i < m_points.size(); ++i) {
        if (QLineF(toPixel(m_points.at(i)), pixel).length() <= 10.0) {
            return i;
        }
    }
    return -1;
}

void FanCurveChart::sortPoints()
{
    std::sort(m_points.begin(), m_points.end(), [](const QPointF &left, const QPointF &right) {
        return left.x() < right.x();
    });
}

void FanCurveChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = plotRect();
    const QPalette palette = this->palette();

    painter.fillRect(plot, palette.color(QPalette::Base));
    painter.setPen(QPen(palette.color(QPalette::Mid), 1));

    painter.setFont(QFont(font().family(), font().pointSize() - 1));
    for (int step = 0; step <= 10; ++step) {
        const double speed = step * 10.0;
        const double y = plot.bottom() - (speed / 100.0) * plot.height();
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.drawText(QRectF(0, y - 8, plot.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1%").arg(step * 10));
    }

    const int divisions = 10;
    for (int step = 0; step <= divisions; ++step) {
        const int temperature = qRound((m_maxTemperature * step) / static_cast<double>(divisions));
        const double x = plot.left() + (temperature / static_cast<double>(m_maxTemperature)) * plot.width();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.drawText(QRectF(x - 18, plot.bottom() + 4, 36, 16), Qt::AlignHCenter | Qt::AlignTop, QString::number(temperature));
    }

    painter.setPen(QPen(palette.color(QPalette::WindowText), 1));
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawText(QRectF(plot.left(), plot.bottom() + 16, plot.width(), 18), Qt::AlignHCenter | Qt::AlignTop, tr("Temperature (°C)"));

    if (m_points.size() < 2) {
        return;
    }

    QPainterPath curve;
    curve.moveTo(toPixel(m_points.first()));
    for (int i = 1; i < m_points.size(); ++i) {
        curve.lineTo(toPixel(m_points.at(i)));
    }
    const QColor accent = palette.color(QPalette::Highlight);
    painter.setPen(QPen(accent, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(curve);
    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    for (const QPointF &point : m_points) {
        painter.drawEllipse(toPixel(point), 5, 5);
    }

    if (!m_hasLive) {
        return;
    }
    const double shown = qBound(0.0, m_liveTemperature, static_cast<double>(m_maxTemperature));
    const double speed = speedAt(m_liveTemperature);
    const QPointF at = toPixel(QPointF(shown, speed));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, 1, Qt::DashLine));
    painter.drawLine(QPointF(at.x(), plot.top()), QPointF(at.x(), plot.bottom()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette.color(QPalette::WindowText));
    painter.drawEllipse(at, 4, 4);

    const QString label = tr("%1 °C · %2%").arg(QString::number(m_liveTemperature, 'f', 1), QString::number(qRound(speed)));
    const QFontMetrics metrics(painter.font());
    const int textWidth = metrics.horizontalAdvance(label);
    const int textHeight = metrics.height();
    qreal textX = at.x() + 8;
    const qreal textY = plot.top() + 4;
    if (textX + textWidth > plot.right()) {
        textX = at.x() - textWidth - 8;
    }
    painter.setPen(palette.color(QPalette::WindowText));
    painter.drawText(QRectF(textX, textY, textWidth + 2, textHeight), Qt::AlignLeft | Qt::AlignVCenter, label);
}

void FanCurveChart::mousePressEvent(QMouseEvent *event)
{
    const QPointF pixel = event->position();
    const int index = pointAt(pixel);
    if (event->button() == Qt::RightButton) {
        if (index >= 0 && m_points.size() > 2) {
            m_points.removeAt(index);
            m_dragIndex = -1;
            update();
        }
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (index >= 0) {
        m_dragIndex = index;
        return;
    }
    if (!plotRect().contains(pixel)) {
        return;
    }
    const QPointF value = fromPixel(pixel);
    const QPointF rounded(qRound(value.x()), qRound(value.y()));
    m_points.append(rounded);
    sortPoints();
    m_dragIndex = pointAt(toPixel(rounded));
    update();
}

void FanCurveChart::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex < 0 || !(event->buttons() & Qt::LeftButton)) {
        setCursor(pointAt(event->position()) >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        return;
    }
    const QPointF value = fromPixel(event->position());
    m_points[m_dragIndex] = QPointF(qRound(value.x()), qRound(value.y()));
    update();
}

void FanCurveChart::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragIndex >= 0) {
        m_dragIndex = -1;
        sortPoints();
        update();
    }
}
