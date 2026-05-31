## Task 2 - Qt Frontend

### What the Frontend Does

Task 2 extends the client with a graphical window that displays the simulation in real time. Instead of only printing the world state to the terminal, the client now opens a Qt window that renders the board visually and draws a trace line behind each bug showing its last N positions.

The current position of each bug is drawn as a solid colored dot - red for the red team, dark for the black team. Each previous position is drawn as a smaller, more transparent dot, fading out the further back in time it is. This gives a clear visual trail showing where bugs have been moving.

The number of trace steps N is configurable by the user through a spin box in the toolbar. Changing N takes effect immediately - if N is reduced, older frames are dropped right away.

The terminal output from Task 1 is still kept alongside the window, so both representations run at the same time.

### New Files

- `bugworld_window.h` - declares the `BugWorldWindow` class, a `QMainWindow` subclass that holds the graphics scene, the grid view, the cycle label, and the trace-length spin box
- `bugworld_window.cpp` - implements the window: draws the board cells with colors matching each cell type, draws fading trace dots for past bug positions, and draws the current bug positions on top

### Changes to client.cpp

Three Qt includes were added at the top. In `main()`, a `QApplication` is created before anything else, a `BugWorldWindow` is opened after the pipes are set up, and the original `while(true)` simulation loop is replaced with a `QTimer` that fires at the configured fps rate. This keeps the Qt event loop responsive while the simulation continues running. The cleanup and pipe closing at the end remain unchanged.
