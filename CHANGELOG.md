# Changelog

All important changes to this project will be written in this file.

---

## [1.1] - Task 2 backend foundation
### added
- Structured data types: `Position`, `Stats`, `WorldState`, and `TraceHistory`
- Parsing of simulator responses into `WorldState`
- Extraction of red and black bug positions from world rows
- Trace-history storage for the last `N` frames
- Optional command-line trace-length argument for backend testing
- Temporary debug output for verifying trace history behavior

### changed
- Modified `show_pretty(...)` to render from parsed `WorldState` instead of raw simulator text
- Extended the main loop to parse each frame and store trace history after `FETCH` and `STEP`
- FIFO files are now expected to be created as temporary runtime artifacts rather than kept in the project directory

### fixed
- Replaced `sleep(0.5)` with `usleep(500000)` in `read_til_END(...)`
- Improved handling of simulator frame storage

---

## [1.0] - Rendering and final changes
### added
- Terminal rendering of the world map and simulation state
- Cycle counter is now displayed 
- Stats are also displayed now
- The client implementation is finally complete

### changed
- Cleaner output formatting for the map
- More robust handling of partial reads and writes when communicating through pipes

### fixed
- There where a lot of issues where incomplete responses from sim caused the client to stop reading too early
- Terminal output formatting issues

---

## [0.3] - sim communication

### added
- Pipes communication btwn client and simulator
- Function write_everything so that we make sure the messages are fully transmited
- Function read_til_END to read the sim output until END marker

### fixed
- When the sim process fails or closes unexpectedly the errors are now handled properly

---

## [0.2] - handling files and parsing arguments

### added
- Command line arguments for parsing the world file, bug files adn simulation paramenters
- Validation for incorrect/missing arguments

### improved
- Error messages when the files can't be open

## [0.1] - initial client setup

### added
- Basic structure of client
- Basic terminal output for debugging
