#include "CanvasWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(960, 540);
    setAutoFillBackground(false);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event) {
    const QPoint pos = event->pos();
    const int tx = pos.x() / kTile;
    const int ty = pos.y() / kTile;
    emit mouseInfoChanged(
        QString("Mouse: %1, %2").arg(pos.x()).arg(pos.y()),
        QString("Tile: %1, %2").arg(tx).arg(ty));
    QWidget::mouseMoveEvent(event);
}

void CanvasWidget::drawBackground(QPainter &p) {
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.0, QColor("#173a3f"));
    grad.setColorAt(1.0, QColor("#0f2b30"));
    p.fillRect(rect(), grad);

    // Distant tree silhouette (left side), echoing the big tree in the
    // reference editor.
    QColor trunk("#4a2f22");
    QColor canopy("#1f5247");
    int tx = width() * 0.10;
    int groundY = height() * 0.55;
    p.fillRect(QRect(tx, groundY - 170, 22, 170), trunk);
    p.setBrush(canopy);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(tx + 10, groundY - 190), 95, 70);
    p.drawEllipse(QPoint(tx - 40, groundY - 150), 60, 50);
    p.drawEllipse(QPoint(tx + 60, groundY - 150), 60, 50);
}

void CanvasWidget::drawGroundLayer(QPainter &p) {
    int groundY = height() * 0.55;
    QColor dirt("#5b3a23");
    QColor grass("#4caf50");

    auto block = [&](int x, int y, int w, int h) {
        p.fillRect(QRect(x, y, w, h), dirt);
        p.fillRect(QRect(x, y, w, 10), grass);
    };

    // Main ground strip (left)
    block(0, groundY, width() * 0.34, height() - groundY);
    // Floating platform (center)
    block(width() * 0.40, groundY - 70, width() * 0.07, 70 + (height() - groundY));
    // Mid platform
    block(width() * 0.50, groundY - 20, width() * 0.10, 20 + (height() - groundY));
    // Right ground strip
    block(width() * 0.66, groundY - 40, width() * 0.34, 40 + (height() - groundY));
}

void CanvasWidget::drawHazards(QPainter &p) {
    int groundY = height() * 0.55;
    int spikeY = groundY + (height() - groundY) * 0.0;
    p.setBrush(QColor("#c9ced4"));
    p.setPen(Qt::NoPen);
    int startX = width() * 0.475;
    for (int i = 0; i < 8; ++i) {
        QPolygon tri;
        int bx = startX + i * 16;
        tri << QPoint(bx, groundY + 14) << QPoint(bx + 8, groundY - 4) << QPoint(bx + 16, groundY + 14);
        p.drawPolygon(tri);
    }
    // Wooden crate
    QRect crate(width() * 0.60, groundY - 38, 38, 38);
    p.setBrush(QColor("#caa15a"));
    p.setPen(QPen(QColor("#5b3a23"), 2));
    p.drawRect(crate);
    p.drawLine(crate.topLeft(), crate.bottomRight());
    p.drawLine(crate.topRight(), crate.bottomLeft());
}

void CanvasWidget::drawDecoration(QPainter &p) {
    int groundY = height() * 0.55;
    // Small bush near the right platform
    p.setBrush(QColor("#3f8e46"));
    p.setPen(Qt::NoPen);
    int bx = width() * 0.78;
    p.drawEllipse(QPoint(bx, groundY - 38), 26, 18);
    p.drawEllipse(QPoint(bx - 18, groundY - 30), 18, 14);
    p.drawEllipse(QPoint(bx + 18, groundY - 30), 18, 14);
}

void CanvasWidget::drawGameObjects(QPainter &p) {
    int groundY = height() * 0.55;

    // Player
    int px = width() * 0.135;
    int py = groundY - 46;
    p.setBrush(QColor("#caa15a"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(px, py - 28), 9, 9);          // head
    p.setBrush(QColor("#b14b3a"));
    p.drawRoundedRect(QRect(px - 9, py - 20, 18, 16), 3, 3); // shirt
    p.setBrush(QColor("#3a5fa0"));
    p.drawRect(QRect(px - 8, py - 4, 16, 14));          // pants
    p.setBrush(QColor("#1c2026"));
    p.drawRect(QRect(px - 8, py + 10, 6, 6));
    p.drawRect(QRect(px + 2, py + 10, 6, 6));

    // Coins (matches Coin_01, Coin_02 selected, Coin_03-ish)
    auto coin = [&](int cx, int cy, bool selected) {
        if (selected) {
            p.setPen(QPen(QColor("#5fb0ff"), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRect(cx - 16, cy - 16, 32, 32), 4, 4);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#f2c14e"));
        p.drawEllipse(QPoint(cx, cy), 11, 11);
        p.setBrush(QColor("#d99a1e"));
        p.drawEllipse(QPoint(cx, cy), 5, 5);
    };
    int coinY = groundY - 95;
    coin(width() * 0.415, coinY, false);
    coin(width() * 0.450, coinY, true);
    coin(width() * 0.485, coinY, false);

    // Enemy (goomba-like)
    int ex = width() * 0.86;
    int ey = groundY - 18;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#6b3d2e"));
    p.drawEllipse(QPoint(ex, ey - 6), 16, 14);
    p.setBrush(QColor("#3a2218"));
    p.drawRect(QRect(ex - 16, ey + 4, 10, 8));
    p.drawRect(QRect(ex + 6, ey + 4, 10, 8));
    p.setBrush(Qt::white);
    p.drawEllipse(QPoint(ex - 5, ey - 8), 3, 3);
    p.drawEllipse(QPoint(ex + 5, ey - 8), 3, 3);
}

void CanvasWidget::drawGrid(QPainter &p) {
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    for (int x = 0; x < width(); x += kTile)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += kTile)
        p.drawLine(0, y, width(), y);
}

void CanvasWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawBackground(p);
    drawGroundLayer(p);
    drawHazards(p);
    drawDecoration(p);
    drawGameObjects(p);
    drawGrid(p);
}
