#pragma once
#include <QWidget>
#include <QImage>

class TilesetPanel : public QWidget {
    Q_OBJECT
public:
    explicit TilesetPanel(QWidget *parent = nullptr);
    void setSpritesheet(const QImage &image, int tileSize, int cols);
    int selectedIndex() const { return m_selected; }

signals:
    void tileSelected(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QImage m_sheet;
    int m_tileSize = 32;
    int m_cols = 16;
    int m_selected = 4;
    int m_tileCount = 0;
    static constexpr int kCell = 48;
    static constexpr int kGap = 6;
};
