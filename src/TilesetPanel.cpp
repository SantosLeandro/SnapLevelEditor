#include "TilesetPanel.h"
#include <QPainter>
#include <QMouseEvent>

TilesetPanel::TilesetPanel(QWidget *parent) : QWidget(parent) {
}

void TilesetPanel::setSpritesheet(const QImage &image, int tileSize, int cols) {
    m_sheet = image;
    m_tileSize = tileSize;
    m_cols = cols;
    int rows = m_sheet.height() / m_tileSize;
    m_tileCount = rows * m_cols;
    int panelCols = qMin(m_cols, 14);
    int panelRows = (m_tileCount + panelCols - 1) / panelCols;
    setMinimumHeight(panelRows * (kCell + kGap) + kGap);
    update();
}

void TilesetPanel::mousePressEvent(QMouseEvent *event) {
    int col = event->pos().x() / (kCell + kGap);
    int row = event->pos().y() / (kCell + kGap);
    int idx = row * m_cols + col;
    if (idx >= 0 && idx < m_tileCount) {
        m_selected = idx;
        emit tileSelected(idx);
        update();
    }
}

void TilesetPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor("#14171c"));

    if (m_sheet.isNull()) return;

    for (int i = 0; i < m_tileCount; ++i) {
        int row = i / m_cols;
        int col = i % m_cols;
        QRect r(kGap + col * (kCell + kGap), kGap + row * (kCell + kGap), kCell, kCell);

        QRect src(col * m_tileSize, row * m_tileSize, m_tileSize, m_tileSize);
        p.drawImage(r, m_sheet, src);

        if (i == m_selected) {
            p.setPen(QPen(QColor("#5fb0ff"), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), 5, 5);
        }
    }
}
