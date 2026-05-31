# Software Engineering – Spring 2026
# README Bug World Client
This is our client implementation for the Bug World simulator.
The client communicates with the given sim program using named pipes. It sends commands to advance the simulation and reads back the world state, which is then displayed in the terminal.
The client communicates with the simulator using the pipe protocol described in the project documentation. The simulator sends responses containing the cycle number, map, and stats, which the client parses and prints.

## Compiling
The client is written in C++ so compile it using g++:
g++ client.cpp -o client

## Running
The program expects the following arguments:
./client <world_map> <bug1> <bug2> [ticks_per_frame] [fps]

Example:
./client Worlds/tiny.world Bugs/beetle.bug Bugs/firefly.bug 1 2

The first three arguments are required:
- world_map – path to the .world file
- bug1 – bug program for the red team
- bug2 – bug program for the black team

The last two arguments are optional:
- ticks_per_frame – how many simulation steps happen before updating the display
- fps – how often the display refreshes
If they are not provided, the simulator defaults are used.

## What the Client Does
The client follows the protocol described in the project guide:
- Creates temporary named pipes
- Starts the simulator as a subprocess
- Opens the pipes in the correct order
- Sends commands such as STEP and FETCH
- Reads the simulator response until the END marker
- Parses the response
- Prints the world state to the terminal
The simulation continues until the program exits.

## Files
Main files in the project:
- client.cpp: main client implementation
- sim: provided simulator binary
- asm: probided bug assembly interpreter
- Worlds/: world map files
- Bugs/: bug programs
- README.md: this file
- CHANGELOG.md: list of project changes


## Task 2 documentation
Deleted cmd.pipe and data.pipe from the project directory, I will make sure the program creates temporary pipes elsewhere instead. 
Replaced sleep(0.5) with usleep(500000), because sleep only takes integers (so 0.5 would become 0 and thus sleep wouldn't wait at all)
Due to debug tests, the new run command looks like: ./client <world_map> <bug1> <bug2> [ticks_per_frame] [fps] [trace_length]

## Structured Data Types
Introduced four structs: 
- Position: stores row and column of a bug on the map
- Stats: stores sim stats in named fields (redAlive, blackAlive, redFood, blackFood)
- WorldState: stores one full simulator frame (includes cycle number, board rows, stats, and bug positions)
- TraceHistory: stores the last N frames of bug positions for both teams
Task 2 requires storing previous bug positions over time, while Task 1 only used raw text for immediate printing, therefore these structures were necessary

## Change to show_pretty function
The new show_pretty receives a WorldState object instead of raw sim text. 
The original show_pretty(...) both parsed and rendered the simulator response. I separated these responsibilities so that parsing happens once in a dedicated function and rendering only displays already-structured data. This makes the code easier to understand, easier to test, and easier to extend.

## Sim Response Parsing
New function parse_response(const string& response, WorldState& state) was added.
This function reads one full simulator message and extracts:
- the current cycle,
- the stats line,
- the board rows,
It then converts the board into bug position vectors.
The parser resets the WorldState at the start of each frame so that data from older frames does not mix with the newly received simulator state.

## Bug Position Extraction
Added find_bug_positions to scan the board text and record coordinates. 
The simulator gives the board as text rows, so bug positions are not available directly. This helper function searches each board cell and stores the coordinates of matching bugs. Uppercase and lowercase symbols are treated as belonging to the same team, since it was clarified that it's not yet necessary to distinguish individual bugs of the same team.

## Trace History Storage
Added two functions for trace management:
- set_trace_length: sets the max size N and removes old stored frames first (if needed)
- add_frame_to_trace: stores the current frame's red and black bug positions and removes the oldest frames in history>N
This is important backend support, because it keeps the last N positions that the frontend can later draw as fading traces.

## Extended main
Added:
- WorldState current_state
- TraceHistory traces
After each FETCH and STEP, the program now:
- reads the simulator response,
- parses it into current_state,
- adds the frame to traces,
- displays the parsed state

## Temporary(?) debugging for backend functionality verification
I'm not sure whether to remove these after all tests pass or not
To verify that trace storage was working correctly, I temporarily added debug output showing:
- current cycle,
- number of red and black bugs found in the current frame,
- number of stored red and black frames,
- current maxHistory.
I confirmed that the trace history grows up to N and then stays capped there, which shows that old frames are being removed as intended.

I also added support for an optional extra command-line argument for trace length.
This allowed me to run the program with different values of N, such as:
./client Worlds/tiny.world Bugs/beetle.bug Bugs/firefly.bug 1 2 5
Currently useful for backend verification, the frontend can later replace/extend this with a proper interactive control.

