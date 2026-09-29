#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

class FanCurveChart : public QWidget
{
    Q_OBJECT

public:
    explicit FanCurveChart(QWidget *parent = nullptr);

    void setMaxTemperature(int maxTemperature);
    void setPoints(const QVector<QPointF> &points);
    void setLiveTemperature(double celsius);
    void clearLiveTemperature();
    QVector<QPointF> points() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF plotRect() const;
    QPointF toPixel(const QPointF &point) const;
    QPointF fromPixel(const QPointF &pixel) const;
    int pointAt(const QPointF &pixel) const;
    void sortPoints();
    double speedAt(double temperature) const;

    QVector<QPointF> m_points;
    int m_maxTemperature = 100;
    int m_dragIndex = -1;
    double m_liveTemperature = 0;
    bool m_hasLive = false;
};
