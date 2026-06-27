#pragma once
#include <QWidget>
#include <QPoint>

// CanvasWidget renders the tile grid, ground blocks, decorations and
// game-object gizmos (player, coins, enemy) similar to the editor's
// room view, and reports mouse/tile coordinates to the status bar.
class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget *parent = nullptr);

signals:
    void mouseInfoChanged(const QString &mouseText, const QString &tileText);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawBackground(class QPainter &p);
    void drawGroundLayer(class QPainter &p);
    void drawHazards(class QPainter &p);
    void drawDecoration(class QPainter &p);
    void drawGameObjects(class QPainter &p);
    void drawGrid(class QPainter &p);

    static constexpr int kTile = 32; // logical tile size in px at 100%
};
