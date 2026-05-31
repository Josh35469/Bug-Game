#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <sstream>
#include <errno.h>
#include <cstring>

#include <QApplication>
#include <QTimer>
#include "bugworld_window.h"
using namespace std;

// structs - give the code structured data instead of keeping everything as raw text
// stores one bug location on the board
struct Position { 
    int row;
    int col;
};

// stores the simulator numbers
struct Stats {
    int redAlive = 0;
    int blackAlive = 0;
    int redFood = 0;
    int blackFood = 0;
};

// stores one full snapshot of the simulation at a given moment
struct WorldState {
    int cycle = 0;
    vector<string> board;
    Stats stats;
    vector<Position> redBugs;
    vector<Position> blackBugs;
};

// stores the last N frames of a bug's positions
struct TraceHistory {
    int maxHistory = 10;
    vector<vector<Position>> redFrames;
    vector<vector<Position>> blackFrames;
};

//colors for printing
#define RESET "\033[0m"
#define RED "\033[1;31m"
#define BLACK "\033[1;90m"
#define FOOD_COLOR "\033[1;95m"
#define ROCK_COLOR "\033[1;30m"
#define NEST_RED  "\033[41m"
#define NEST_BLACK "\033[100m"

// DEBUG CHECKS -> REMOVE AT END
static const bool DEBUG_TRACE = true;

//write whole string to fd (pipe)
// Write the full command to the pipe, even if one write() call sends only part of the data
// Pipe writes are not guaranteed to transfer the whole string at once, so this makes communication with the simulator more robust
static bool write_everything(int fd, const string& text)
{
    const char* pos = text.c_str();
    size_t remaining = text.size();
    while(remaining > 0)
    {
        ssize_t sent = write(fd, pos, remaining);
        if(sent < 0)
            return false;
        pos += (size_t)sent;
        remaining -= (size_t)sent;
    }
    return true;
}

//read until END marker
// The simulator may send its response in several smaller chunks, so a single read() call is not enough
static bool read_til_END(int fd, string& output)
{
    int counter = 0;
    output.clear();
    char buf[256];
    while(true)
    {
        //read 256 characters at a time
        ssize_t r = read(fd, buf, sizeof(buf));
        //cout << "retrieved " << r << " bytes: " << buf << endl;
        if(r < 0)
        {
            // If no data is available on the non-blocking pipe, wait for a bit
            // Try again instead of treating it as a fatal error
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(1000);
                continue;
            }
            return false;
        }
        // 0 means no more data was read at this moment, allow a few retries before timing out
        if(r == 0)
        {
            usleep(500000); // changed from sleep(0.5) because it did no real wait
            counter += 1;
        }
        if (counter > 5) 
        {
            cerr << "[read_til_END] response from sim timed out.\n";
            return false;
        }
        //return false;
     if (r > 0)
        output.append(buf, r);
          // Stop at protocol terminator marking one complete simulator response
        // This prevents us from accidentally reading into the next one
        if (output.find("\nEND\n") != string::npos)
            return true;
        if (output.size() >= 4 && output.substr(output.size() - 4) == "\nEND") 
            return true;
    }
}

// Replaced the last show_pretty function to make it easier for Task 2 while keeping the functionality consistent
// same simulator communication idea, same overall client, same input/output workflow
static void show_pretty(const WorldState& state)
{
    // Print from the parsed WorldState instead of raw simulator text
    // This keeps rendering separate from parsing, which makes the program easier to maintain
    // The frontend can reuse the same structured data
    cout << endl << "CYCLE: " << state.cycle << endl << endl;

    // The board is stored inside state.board, so rendering only needs to walk through the characters and -
    // choose how each symbol should be displayed
    for (size_t i = 0; i < state.board.size(); i++)
    {
        for (char c : state.board[i])
        {
            // Bug symbols are colored by team to make the current state of the world easier to read in the terminal
            if (c == 'R')
                cout << RED << 'R' << RESET;
            else if (c == 'r')
                cout << RED << 'r' << RESET;
            else if (c == 'B')
                cout << BLACK << 'B' << RESET;
            else if (c == 'b')
                cout << BLACK << 'b' << RESET;

            // Nest cells use background colors so they stand out from ordinary board characters
            else if (c == '+')
                cout << NEST_RED << '+' << RESET;
            else if (c == '-')
                cout << NEST_BLACK << '-' << RESET;

            // Rocks and food are also highlighted to make the map structure easier to understand
            else if (c == '#')
                cout << ROCK_COLOR << '#' << RESET;
            else if (isdigit((unsigned char)c))
                cout << FOOD_COLOR << c << RESET;

            // Any other character is printed unchanged
            else
                cout << c;
        }
        cout << endl;
    }

    // Stats are now read from named fields instead of being reparsed from raw text
    // This is clearer and avoids duplicating parsing
    cout << "\nSTATS: "
         << RED << "Red alive: " << state.stats.redAlive << RESET << " "
         << BLACK << "Black alive: " << state.stats.blackAlive << RESET << " "
         << RED << "Red food: " << state.stats.redFood << RESET << " "
         << BLACK << "Black food: " << state.stats.blackFood << RESET << endl;

    // These counts are useful for Task 2 for extracting and storing bug positions for trace visualization
    cout << "Red bugs on board: " << state.redBugs.size() << " | "
         << "Black bugs on board: " << state.blackBugs.size() << endl;

    cout << flush;
}


// Added for Task 2
static vector<Position> find_bug_positions(const vector<string>& board, char bug1, char bug2)
{
    vector<Position> positions;

    // The simulator gives the world as text rows, so to find bugs we scan each cell of each row and record matching coordinates
    for (int r = 0; r < (int)board.size(); r++)
    {
        for (int c = 0; c < (int)board[r].size(); c++)
        {
            // Treat uppercase and lowercase symbols as belonging to the same team
            // (since for task 2 we only need team traces, not for each individual bug)
            if (board[r][c] == bug1 || board[r][c] == bug2)
                positions.push_back({r, c});
        }
    }

    return positions;
}

static bool parse_response(const string& response, WorldState& state)
{
    // Each simulator message describes a fresh state of the world
    // We reset the state first, so no old data survives into the newly parsed frame
    state = WorldState{};

    string stats_line;
    istringstream stream(response);
    string line;

    while (getline(stream, line))
    {
        // Some environments may send lines ending in "\r\n" instead of "\n"
        // We remove the trailing '\r' so text comparisons don't fail.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("CYCLE ", 0) == 0)
        {
            // Store the simulation step number for this frame
            state.cycle = stoi(line.substr(6));
        }
        else if (line.rfind("STATS ", 0) == 0)
        {
            // We store the stats text first and parse it once after the loop (keeps the line-handling logic simple and localized)
            stats_line = line.substr(6);
        }
        else if (line.rfind("ROW ", 0) == 0)
        {
            // Each ROW line is a visible row of the world board
            // save only the actual contents, not the "ROW " prefix
            state.board.push_back(line.substr(4));
        }
        else if (line == "END")
        {
            break;
        }
    }

    // A usable frame must contain both board data and stats
    // If either is missing, the caller should treat this response as invalid
    if (state.board.empty() || stats_line.empty())
        return false;

    istringstream ss(stats_line);

    // We store the simulator statistics in named fields so later code is clearer
    ss >> state.stats.redAlive
       >> state.stats.blackAlive
       >> state.stats.redFood
       >> state.stats.blackFood;

    // The board is still a text representation -> scan it once and extract bug coordinates into structured vectors
    // This avoids reparsing the board in every later feature
    state.redBugs = find_bug_positions(state.board, 'R', 'r');
    state.blackBugs = find_bug_positions(state.board, 'B', 'b');

    return true;
}


static void set_trace_length(TraceHistory& traces, int n)
{
    // A trace length smaller than 1 would mean we store no visible history at all
    if (n < 1)
        n = 1;

    traces.maxHistory = n;

    // If a user decreases N while the program is running, we may already have more stored frames than the new limit allows
    // So, we remove the oldest frames first and keep only the most recent history (frontend needs this)
    while ((int)traces.redFrames.size() > traces.maxHistory)
        traces.redFrames.erase(traces.redFrames.begin());

    while ((int)traces.blackFrames.size() > traces.maxHistory)
        traces.blackFrames.erase(traces.blackFrames.begin());
}

static void add_frame_to_trace(TraceHistory& traces, const WorldState& state)
{
    // Each simulator response gives one new frame of bug positions
    // Store the full set of red/black team positions for that frame so the frontend can draw a movement trail
    traces.redFrames.push_back(state.redBugs);
    traces.blackFrames.push_back(state.blackBugs);

    // The trace history should only be the last N frames
    // Once stored history becomes too long, discard the oldest frame to keep memory bounded (and to match the task requirement)
    while ((int)traces.redFrames.size() > traces.maxHistory)
        traces.redFrames.erase(traces.redFrames.begin());

    while ((int)traces.blackFrames.size() > traces.maxHistory)
        traces.blackFrames.erase(traces.blackFrames.begin());
}

// DEBUG CHECKS -> REMOVE AT END
// current cycle, how many red/black bugs were found in this frame, how many trace frames are currently stored, the current max trace length N
// basically checks if the trace backend logic works fine before starting to visualize it 
static void debug_trace_status(const TraceHistory& traces, const WorldState& state)
{
    if (!DEBUG_TRACE)
        return;

    cout << "[TRACE DEBUG] cycle=" << state.cycle
         << " red_now=" << state.redBugs.size()
         << " black_now=" << state.blackBugs.size()
         << " red_frames=" << traces.redFrames.size()
         << " black_frames=" << traces.blackFrames.size()
         << " maxHistory=" << traces.maxHistory
         << endl;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    //client <world_map> <bug1> <bug2> [ticks_per_frame] [fps]
    if(argc < 4)
    {
        // DEBUG CHECKS -> REMOVE AT END
        // Replace cerr << "usage: " << argv[0] << " <world_map> <red.bug> <black.bug> [ticks_per_frame] [fps]\n";
        // added N here to make sure backend logic works as intended before implementing changeable N for frontend
        cerr << "usage: " << argv[0] << " <world_map> <red.bug> <black.bug> [ticks_per_frame] [fps] [trace_length]\n";
        return 2;
    }
    int ticks_per_frame = 50;
    int fps = 10;
    int trace_length = 10; // DEBUG CHECKS -> REMOVE AT END

    if(argc >= 5)
        ticks_per_frame = stoi(argv[4]);

    if(argc >= 6)
        fps = stoi(argv[5]);

    // DEBUG CHECKS -> REMOVE AT END
    if(argc >= 7)
        trace_length = stoi(argv[6]);

    if(ticks_per_frame < 1)
        ticks_per_frame = 1;

    if(fps < 1)
        fps = 1;

    // DEBUG CHECKS -> REMOVE AT END
    if(trace_length < 1)
        trace_length = 1;

    const string world_path = argv[1];
    const string red_bug_path = argv[2];
    const string black_bug_path = argv[3];

    cout << "\033[?25l" << flush;

    //created temp dir for pipes (I hope this is what you mean by temporary)
    // This prevents collisions with pipe names from earlier executions
    char temp_dir[] = "/tmp/bugworld_XXXXXXX";
    if(!mkdtemp(temp_dir))
    {
        cerr << "[main] Error : mkdtemp failed" << endl;
        cout << "\033[?25h" << flush;
        return 1;
    }

    string cmd_path = string(temp_dir) + "/cmd.pipe";
    string data_path = string(temp_dir) + "/data.pipe";

    //create 2 temporary FIFO files for communication with sim
    // one for commands sent to the simulator and one for simulation data received back from it
    if(mkfifo(cmd_path.c_str(), 0600) != 0)
    {
        cerr << "[main] Error: failed to create FIFO " << cmd_path << endl;
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }
    if (mkfifo(data_path.c_str(), 0600) != 0)
    {
        cerr << "[main] Error: failed to create FIFO " << data_path << endl;
        unlink(cmd_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }

    //launch sim as subproc with --cmd-pipe and --data-pipe
    pid_t pid = fork();
    if(pid < 0)
    {
        cerr << "[main] Error: fork() failed\n";
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        cout << "\033[?25h" << flush;
        rmdir(temp_dir);
        return 1;
    }
    if(pid == 0)
    {
        //child: replace process with sim
        execl("./sim", "./sim", 
            "--cmd-pipe", cmd_path.c_str(), 
            "--data-pipe", data_path.c_str(), 
            world_path.c_str(), red_bug_path.c_str(), black_bug_path.c_str(), 
            (char*)nullptr);
        //if execl return, it failed
        cerr <<  "[main (fork)] Error: failed to exec ./sim\n";
        _exit(127);
    }
    //give sim time to reach its blocking open()
    usleep(100 * 1000);

    //open pipes in the correct order to avoid deadlock
    //this one for reading
    int cmd_fd = open(cmd_path.c_str(), O_WRONLY | O_NONBLOCK);
    if(cmd_fd < 0)
    {
        cerr << "[main] Error: failed to open" << cmd_path << "for writing\n";
        //try to stop sim
        kill(pid, SIGTERM);
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }    

    //this one for writing
    int data_fd = open(data_path.c_str(), O_RDONLY | O_NONBLOCK);
    if(data_fd < 0)
    {
        cerr << "[main] Error failed to open " << data_path << "for readidng\n";
        close(cmd_fd);
        kill(pid, SIGTERM);
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }
    fcntl(data_fd, F_SETFL, O_NONBLOCK);
    string response;
    // For the program to remember more information between frames
    WorldState current_state;
    TraceHistory traces;
    // DEBUG CHECKS -> REMOVE AT END
    // Replaced set_trace_length(traces, 10);
    set_trace_length(traces, trace_length);
    BugWorldWindow window;
    QObject::connect(&window, &BugWorldWindow::trace_length_changed, [&](int n) { set_trace_length(traces, n); });
    window.show();

    //send FETCH instruction
    // Request the initial world state before entering STEP loop
    // This gives the client a valid first frame to display
    if(!write_everything(cmd_fd, "FETCH\n"))
    {
        cerr << "[main] Error: fetch sending failed cycle 0" << endl;
        close(cmd_fd);
        close(data_fd);
        kill(pid, SIGTERM);
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }

    //read and store response
    if(!read_til_END(data_fd, response))
    {
        cerr << "[main] Error: fetch response retrieval failed cycle 0" << endl;
        close(cmd_fd);
        close(data_fd);
        kill(pid, SIGTERM);
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }

    //use response to display game status
    // Extended: for Task 1 reading the response and printing immediately was enough
    // For Task 2: after each FETCH and STEP, parsing, extracting bug positions and storing frames are needed for trace visualisation
    if (!parse_response(response, current_state))
    {
        cerr << "[main] Error: failed to parse simulator response\n";
        // cleanup
        close(cmd_fd);
        close(data_fd);
        kill(pid, SIGTERM);
        unlink(cmd_path.c_str());
        unlink(data_path.c_str());
        rmdir(temp_dir);
        cout << "\033[?25h" << flush;
        return 1;
    }
    add_frame_to_trace(traces, current_state);
    
    show_pretty(current_state);
    window.update_display(current_state, traces);

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        string step = "STEP " + to_string(ticks_per_frame) + "\n";
        if(!write_everything(cmd_fd, step)) { timer.stop(); return; }
        if(!read_til_END(data_fd, response)) { timer.stop(); return; }
        if(!parse_response(response, current_state)) { timer.stop(); return; }
        add_frame_to_trace(traces, current_state);
        show_pretty(current_state);
        window.update_display(current_state, traces);
    });
    timer.start(1000 / fps);
    
    int exit_code = app.exec();

    //wrap up
    write_everything(cmd_fd, "QUIT\n");
    close(cmd_fd);
    close(data_fd);
    int status = 0;
    waitpid(pid, &status, 0);
    unlink(cmd_path.c_str());
    unlink(data_path.c_str());
    rmdir(temp_dir);
    cout << "\033[?25h" << flush;
    return exit_code;
};
