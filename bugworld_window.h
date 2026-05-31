#pragma once

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSpinBox>
#include <QLabel>

#include "client_types.h"

// Main window of    BugWorld simulator
class BugWorldWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BugWorldWindow(QWidget* parent = nullptr);

    // Updates the full display 
    void update_display(const WorldState& state, const TraceHistory& traces);

    // Returns current trace length selected by the user
    int trace_length() const;

signals:
    
    void trace_length_changed(int n);

private:
    QGraphicsScene* m_scene;
    QGraphicsView* m_view;

    QSpinBox* m_spin_n;
    QLabel* m_cycle_label;

    // Handles drawing of the world grid, bugs, and traces
    void draw_world(const WorldState& state, const TraceHistory& traces);

    // Converts map characters into colors for visualization
    QColor cell_color(char c);

    static constexpr int CELL_W = 20;
    static constexpr int CELL_H = 18;
};
