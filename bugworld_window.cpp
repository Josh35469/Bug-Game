#include "bugworld_window.h"
#include <QToolBar>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QApplication>
#include <vector>
#include <string>

BugWorldWindow::BugWorldWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Bug World");

    // main rendering scene 
    m_scene = new QGraphicsScene(this);
    m_view  = new QGraphicsView(m_scene, this);

    // smoother rendering for circles
    
    m_view->setRenderHint(QPainter::Antialiasing);

    setCentralWidget(m_view);

    // simple toolbar for debugging and also the interaction 
    QToolBar* toolbar = addToolBar("Controls");

    m_cycle_label = new QLabel("Cycle: 0", this);
    toolbar->addWidget(m_cycle_label);
    toolbar->addSeparator();

    toolbar->addWidget(new QLabel("Trace length (N):"));

    // controls how many previous frames are shown in the trace
    m_spin_n = new QSpinBox(this);
    m_spin_n->setRange(1, 100);
    m_spin_n->setValue(10);
    m_spin_n->setToolTip("Number of previous positions shown per bug");

    toolbar->addWidget(m_spin_n);

    // connect UI slider -> backend trace system
    connect(m_spin_n, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BugWorldWindow::trace_length_changed);

    resize(900, 700);
}

int BugWorldWindow::trace_length() const
{


    
    return m_spin_n->value();
}

void BugWorldWindow::update_display(const WorldState& state, const TraceHistory& traces)
{
    // update cycle label 
    m_cycle_label->setText(QString("Cycle: %1").arg(state.cycle));

    // redraw full world every frame 
    draw_world(state, traces);
}

void BugWorldWindow::draw_world(const WorldState& state, const TraceHistory& traces)
{
    m_scene->clear();

    const int rows = (int)state.board.size();

    //Draw world map 
    for (int r = 0; r < rows; r++)
    {
        const string& row = state.board[r];
        double x_offset = (r % 2 == 1) ? CELL_W * 0.5 : 0.0;

        for (int c = 0; c < (int)row.size(); c++)
        {
            char ch = row[c];
            if (ch == ' ') continue;

            double x = c * CELL_W + x_offset;
            double y = r * CELL_H;

            QColor bg = cell_color(ch);

            // draw each tile of the world
            auto* rect = m_scene->addRect(
                x, y,
                CELL_W - 1,
                CELL_H - 1,
                QPen(Qt::NoPen),
                QBrush(bg)
            );

            rect->setZValue(0); 
        }
    }

   //drwa the trace history
    auto draw_trace = [&](const vector<vector<Position>>& frames,
                          QColor base_color)
    {
        int total = (int)frames.size();
        int N = trace_length();

        int start = std::max(0, total - N);

        for (int fi = start; fi < total; fi++)
        {
            // fade effect
            double alpha = (double)(fi - start) / std::max(1, (total - start));

            QColor color = base_color;
            color.setAlphaF(0.2 + alpha * 0.5);

            for (const Position& p : frames[fi])
            {
                double x_offset = (p.row % 2 == 1) ? CELL_W * 0.5 : 0.0;

                double x = p.col * CELL_W + x_offset + 3;
                double y = p.row * CELL_H + 3;

                // small dot representing past bug position
                auto* dot = m_scene->addEllipse(
                    x, y,
                    CELL_W - 7,
                    CELL_H - 7,
                    QPen(Qt::NoPen),
                    QBrush(color)
                );

                dot->setZValue(1); 
            }
        }
    };

    draw_trace(traces.redFrames,   QColor(220, 50, 50));
    draw_trace(traces.blackFrames, QColor(60, 60, 60));

    //it draws cuurent bug positions
    auto draw_current = [&](const vector<Position>& bugs, QColor color)
    {
        for (const Position& p : bugs)
        {
            double x_offset = (p.row % 2 == 1) ? CELL_W * 0.5 : 0.0;

            double x = p.col * CELL_W + x_offset + 2;
            double y = p.row * CELL_H + 2;

            // current live bug 
            auto* dot = m_scene->addEllipse(
                x, y,
                CELL_W - 5,
                CELL_H - 5,
                QPen(Qt::NoPen),
                QBrush(color)
            );

            dot->setZValue(2); 
        }
    };

    draw_current(state.redBugs,   QColor(220, 50, 50));
    draw_current(state.blackBugs, QColor(40, 40, 40));
}

QColor BugWorldWindow::cell_color(char c)
{
    // map tie coloring logic
    switch (c)
    {
        case '#': return QColor(80, 80, 80);   // rock
        case '.': return QColor(210, 200, 180);
        case '+': return QColor(255, 180, 180); // red nest
        case '-': return QColor(160, 160, 160); // black nest

        case 'R': case 'r': return QColor(220, 50, 50);
        case 'B': case 'b': return QColor(60, 60, 60);

        default:
           
            if (c >= '1' && c <= '9')
                return QColor(100, 200, 100);

            return QColor(210, 200, 180);
    }
}
