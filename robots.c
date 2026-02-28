#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <curses.h>
#include <time.h>
#include <math.h>
#include "highscores.h"

#define MAXROB 400
#define MAXX 80
#define MAXY 24
#define MAX_SAFE 15
#define MAX_ANTI 15

#define EMPTY 0
#define ROCK 1
#define ROBOT 2
#define PLAYER 3

int MyX, MyY;
int NumRobots;
int RobotsAlive;
int RobX[MAXROB], RobY[MAXROB];
int Dead[MAXROB];
typedef struct { int x, y; } Cell;
int contents[MAXX][MAXY];
int robot_index[MAXX][MAXY];
int isRock[MAXX][MAXY];
typedef struct { int x, y; } RockPos;
RockPos Rocks[MAXROB];
int NumRocks = 0;

int Running = 1;
int Score = 0;
int Level = 1;

int SafeTP = 0;     // carry-over safe teleports
int AntiMatter = 0; // carry-over anti-matter
int PushRocks = 0;  // game option: player can push rocks?
int RandomStart = 0;


HighScore scores[MAX_SCORES];
int score_count = 0;

void curses_on();
void init_game();
void init_level();
void draw_border();
void draw_screen();
void draw_cell(int x, int y, int type);
void kill_robot(int i, int addscore);
void handle_input();
int  move_player(int dx, int dy);
void do_wait();
void move_robots();
void check_collisions();
void rebuild_world();
int  is_safe_spot(int x, int y);
void teleport_random();
void teleport_safe();
int  do_am();
void end_game();

/*--------------------------------------------------------------------------------------------
 * Main loop
 *--------------------------------------------------------------------------------------------*/
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--pushrocks"))
            PushRocks = 1;
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--randomstart"))
            RandomStart = 1;
    }

    int ch;

    score_count = load_scores(scores);

    srand(time(NULL));

    curses_on();

    if (has_colors()) {
        start_color();
        init_pair(EMPTY, COLOR_BLACK, COLOR_BLACK); // empty cells
        init_pair(ROCK,  COLOR_WHITE, COLOR_BLACK); // rocks
        if (can_change_color()) {
            init_color(10, 1000, 0, 0); // bright red
            init_color(11, 0, 1000, 0); // bright green
            init_pair(PLAYER, 11, COLOR_BLACK);          // player
            init_pair(ROBOT,  COLOR_BLACK, 10);          // robots
            
        } else {
            init_pair(PLAYER, COLOR_GREEN, COLOR_BLACK); // player
            init_pair(ROBOT,  COLOR_BLACK, COLOR_RED);   // robots
        }
    }

    while(1) {
        init_game();
        draw_border();

        init_level();
        draw_screen();

        while (Running) {
            handle_input();
            check_collisions();
            rebuild_world();

            if (!Running) {
                break;
            }

            if (RobotsAlive == 0) {
                Level++;
                //SafeTP += 2;
                //AntiMatter += 3;
                SafeTP += (int)(log(Level) * 2);
                AntiMatter += (int)(log(Level) * 1.5);
                if (SafeTP > MAX_SAFE) SafeTP = MAX_SAFE;
                if (AntiMatter > MAX_ANTI) AntiMatter = MAX_ANTI;
                init_level();
                napms(300);
            }

            draw_screen();
        }
        draw_screen();

        mvprintw(MAXY-1, 0, "Game over! Final score: %d. Level reached: %d. Play again? (y/n)", Score, Level);
        refresh();
        napms(2000);

        if (highscore_rank(scores, score_count, Score) <= MAX_SCORES) {
            char name[32];

            endwin();
            show_scores(scores, score_count);

            printf("New high score! Final score: %d. Level reached: %d. Enter your name: ", Score, Level);
            fflush(stdout);

            if (fgets(name, sizeof(name), stdin)) {
                // remove newline
                name[strcspn(name, "\n")] = '\0';
            } else {
                strcpy(name, "Unknown");
            }

            curses_on();

            add_score(scores, &score_count, name, Score, Level);
            save_scores(scores, score_count);
        }

        mvprintw(MAXY-1, 0, "Game over! Final score: %d. Level reached: %d. Play again? (y/n)", Score, Level);

        do {
            ch = getch();
        } while (ch != 'y' && ch != 'n');

        if (ch == 'n')
            break; // exit outer loop

        Running = 1;
    }

    end_game();
    return 0;
}

/*--------------------------------------------------------------------------------------------
 * Initialize curses
 *--------------------------------------------------------------------------------------------*/
void curses_on() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
}

/*--------------------------------------------------------------------------------------------
 * Initialize game
 *--------------------------------------------------------------------------------------------*/
void init_game() {
    Running = 1;
    Score = 0;
    Level = 1;
    // initial balance
    SafeTP = 0;
    AntiMatter = 2;
    clear();
}

/*--------------------------------------------------------------------------------------------
 * Initialize new level: increase #robots and place randomly
 *--------------------------------------------------------------------------------------------*/
void init_level() {
    // Generate new random coordinates for player
    if (RandomStart) {
        MyX = 1 + rand() % (MAXX - 2);
        MyY = 1 + rand() % (MAXY - 2);
    } else {
        MyX = MAXX / 2;
        MyY = MAXY / 2;
    }

    NumRobots = 10 + Level * 5;
    if (NumRobots > MAXROB)
        NumRobots = MAXROB;
    RobotsAlive = NumRobots;

    // Generate new random coordinates for all robots
    for (int i = 0; i < NumRobots; i++) {
        do {
            RobX[i] = 1 + rand() % (MAXX - 2);
            RobY[i] = 1 + rand() % (MAXY - 2);
        } while (RobX[i] == MyX && RobY[i] == MyY);
        Dead[i] = 0;
    }

    // Clear rocks
    NumRocks = 0;
    for (int x = 0; x < MAXX; x++) {
        for (int y = 0; y < MAXY; y++) {
            isRock[x][y] = 0;
        }
    }

    rebuild_world();
}

/*--------------------------------------------------------------------------------------------
 * Draw border
 *--------------------------------------------------------------------------------------------*/
void draw_border() {
    // draw horizontal border
    for (int x = 0; x < MAXX; x++) {
      mvaddch(0, x, '-');
      mvaddch(MAXY-1, x, '-');
    }

    // draw vertical borders
    for (int y = 0; y < MAXY; y++) {
      mvaddch(y, 0, '|');
      mvaddch(y, MAXX-1, '|');
    }

    // draw corners
    mvaddch(0, 0, '+');
    mvaddch(0, MAXX-1, '+');
    mvaddch(MAXY-1, 0, '+');
    mvaddch(MAXY-1, MAXX-1, '+');
}

/*--------------------------------------------------------------------------------------------
 * Draw screen: draw status line, borders, rocks, robots and player
 *--------------------------------------------------------------------------------------------*/
void draw_screen() {
    int x = MAXX + 5;
    mvprintw(2, x,  "Score:  %d     ", Score);
    mvprintw(4, x,  "Level:  %d     ", Level);
    mvprintw(6, x,  "Safe:   %d     ", SafeTP);
    mvprintw(8, x,  "AM:     %d     ", AntiMatter);
    mvprintw(10, x, "Robots: %d     ", RobotsAlive);
    mvprintw(13, x, "Move:   7 8 9");
    mvprintw(14, x, "        4 5 6");
    mvprintw(15, x, "        1 2 3");
    mvprintw(17, x, "t=teleport");
    mvprintw(18, x, "s=safe teleport");
    mvprintw(19, x, "a=anti-matter");

    for (int y = 1; y < MAXY-1; y++) {
        for (int x = 1; x < MAXX-1; x++) {
            draw_cell(x, y, contents[x][y]);
        }
    } 
    refresh();
}

/*--------------------------------------------------------------------------------------------
 * Helpers
 *--------------------------------------------------------------------------------------------*/
int min(int a, int b) {
    return (a < b ? a : b);
}

void draw_cell(int x, int y, int type) {
    char c = ' ';
    int  color = type;
    switch (type) {
        case EMPTY:  c = ' '; break;
        case ROCK:   c = '*'; break;
        case ROBOT:  c = 'R'; break;
        case PLAYER: c = '@'; break;
    }
    if (has_colors() && color != 0) attron(COLOR_PAIR(color));
    mvaddch(y, x, c);
    if (has_colors() && color != 0) attroff(COLOR_PAIR(color));
}

void kill_robot(int i, int addscore) {
    if (Dead[i]) return;
    
    Dead[i] = 1;
    RobotsAlive--;
    Score += addscore;
}

/*--------------------------------------------------------------------------------------------
 * Handle input and move accordingly
 *--------------------------------------------------------------------------------------------*/
void handle_input() {
    int ch = getch();
    int moved = 0;
    switch (ch) {

        // keypad navigation
        case '4': moved = move_player(-1, 0); break;
        case '6': moved = move_player(1, 0); break;
        case '8': moved = move_player(0, -1); break;
        case '2': moved = move_player(0, 1); break;
        case '7': moved = move_player(-1, -1); break;
        case '9': moved = move_player(1, -1); break;
        case '1': moved = move_player(-1, 1); break;
        case '3': moved = move_player(1, 1); break;

        // classic hjklyubn
        case 'h': moved = move_player(-1, 0); break;
        case 'l': moved = move_player(1, 0); break;
        case 'k': moved = move_player(0, -1); break;
        case 'j': moved = move_player(0, 1); break;
        case 'y': moved = move_player(-1, -1); break;
        case 'u': moved = move_player(1, -1); break;
        case 'b': moved = move_player(-1, 1); break;
        case 'n': moved = move_player(1, 1); break;

        case '.': moved = 1; break;
        case '5': moved = 1; break;

        case 't': teleport_random(); moved = 1; break;
        case 's': teleport_safe();   moved = 1; break;

        case 'a': moved = do_am(); break;

        case 'w': do_wait(); moved = 1; break;

        case 'q': Running = 0; break;
    }

    if (moved)
        move_robots();
}

/*--------------------------------------------------------------------------------------------
 * Handle player move (dx, dy)
 *--------------------------------------------------------------------------------------------*/
int move_player(int dx, int dy) {
    int nx = MyX + dx;
    int ny = MyY + dy;

    if (nx <= 0 || nx >= MAXX-1 || ny <= 0 || ny >= MAXY-1)
      return 0;

    // Player tries to push a rock
    if (isRock[nx][ny]) {
        if (!PushRocks)
            return 0;

        // Diagonal push not allowed
        if (dx != 0 && dy != 0)
            return 0;

        int rx = nx + dx;
        int ry = ny + dy;

        if (rx <= 0 || rx >= MAXX-1 || ry <= 0 || ry >= MAXY-1)
            return 0;

        // cannot push more than 1 rock
        if (isRock[rx][ry])
            return 0;

        // robot (if any) crushed by pushed rock
        for (int i = 0; i < NumRobots; i++) {
            if (!Dead[i] && RobX[i] == rx && RobY[i] == ry) {
                kill_robot(i, 10);
            }
        }

        // move rock
        isRock[rx][ry] = 1;
        isRock[nx][ny] = 0;

        for (int r = 0; r < NumRocks; r++) {
            if (Rocks[r].x == nx && Rocks[r].y == ny) {
                Rocks[r].x = rx;
                Rocks[r].y = ry;
                break;
            }
        }
    }

    // valid move
    MyX = nx;
    MyY = ny;
    return 1;
}

/*--------------------------------------------------------------------------------------------
 * Handle wait: player waits while robots keep moving
 *--------------------------------------------------------------------------------------------*/
void do_wait() {
    int ScoreBefore = Score;
    while (1) {
        move_robots();
        check_collisions();

        if (!Running)
            break;

        if (RobotsAlive == 0)
            break;

        rebuild_world();
        draw_screen();
        napms(30);
    }
    if (Running)
        Score += (int)((Score - ScoreBefore) * 0.1);
    rebuild_world();
    draw_screen();
    flushinp();
    napms(1000);
}

/*--------------------------------------------------------------------------------------------
 * Move all live robots towards player
 *--------------------------------------------------------------------------------------------*/
void move_robots() {
    for (int i = 0; i < NumRobots; i++) {
        if (Dead[i]) continue;

        int oldx = RobX[i];
        int oldy = RobY[i];

        int nx = oldx;
        int ny = oldy;

        if (nx < MyX) nx++;
        else if (nx > MyX) nx--;

        if (ny < MyY) ny++;
        else if (ny > MyY) ny--;

        if (isRock[nx][ny]) {
            kill_robot(i, 10);
            continue;
        }

        RobX[i] = nx;
        RobY[i] = ny;
    }
}

/*--------------------------------------------------------------------------------------------
 * Check for collisions between player and robots, or between robots
 *--------------------------------------------------------------------------------------------*/
void check_collisions() {
    int i;

    // 1. check: robot hits player
    for (i = 0; i < NumRobots; i++) {
        if (!Dead[i] && RobX[i] == MyX && RobY[i] == MyY) {
            isRock[MyX][MyY] = 1;
            Running = 0; // Game over
            return;
        }
    }

    // 2. count robots per cell
    static int count[MAXX][MAXY];
    memset(count, 0, sizeof(count));

    for (i = 0; i < NumRobots; i++) {
        if (!Dead[i])
            count[RobX[i]][RobY[i]]++;
    }

    // 3. any cell with 2+ robots becomes a rock
    for (i = 0; i < NumRobots; i++) {
        if (Dead[i]) continue;
        int x = RobX[i];
        int y = RobY[i];

        if (count[x][y] >= 2) {
            for (int j = 0; j < NumRobots; j++) {
                if (!Dead[j] && RobX[j] == x && RobY[j] == y) {
                    kill_robot(j, 10);
                }
            }
            if (!isRock[x][y]) {
                isRock[x][y] = 1;
                Rocks[NumRocks++] = (RockPos){x, y};
            }
        }
    }

    // 4. robots standing on rocks must die
    for (i = 0; i < NumRobots; i++){
        if (!Dead[i] && isRock[RobX[i]][RobY[i]]) {
            kill_robot(i, 10);
        }
    }
}

/*--------------------------------------------------------------------------------------------
 * Sync contents and robot_index with the new coordinates
 *--------------------------------------------------------------------------------------------*/
void rebuild_world() {
    // Clear contents
    for (int y = 0; y < MAXY; y++) {
        for (int x = 0; x < MAXX; x++) {
            contents[x][y] = EMPTY;
            robot_index[x][y] = -1;
        }
    }

    // Rebuild rocks
    for (int r = 0; r < NumRocks; r++) {
        int x = Rocks[r].x;
        int y = Rocks[r].y;
        contents[x][y] = ROCK;
    }

    // Rebuild robots
    for (int i = 0; i < NumRobots; i++) {
        if (!Dead[i]) {
            int x = RobX[i];
            int y = RobY[i];
            contents[x][y] = ROBOT;
            robot_index[x][y] = i;
        }
    }

    // Rebuild player
    contents[MyX][MyY] = PLAYER;
}

/*--------------------------------------------------------------------------------------------
 * Is location (x, y) safe to teleport to?
 *--------------------------------------------------------------------------------------------*/
int is_safe_spot(int tx, int ty) {
    if (contents[tx][ty] != EMPTY)
        return 0;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
                continue;

            int x = tx + dx;
            int y = ty + dy;

            if (x < 1 || x >= MAXX-1 || y < 1 || y >= MAXY-1)
                continue;

            if (contents[x][y] == ROBOT)
                return 0;

            if (isRock[x][y])
                return 0;
        }
    }
    return 1;
}

/*--------------------------------------------------------------------------------------------
 * Perform random (unsafe) teleport
 *--------------------------------------------------------------------------------------------*/
void teleport_random() {
    MyX = 1 + rand() % (MAXX - 2);
    MyY = 1 + rand() % (MAXY - 2);
}

/*--------------------------------------------------------------------------------------------
 * Perform safe teleport
 *--------------------------------------------------------------------------------------------*/
void teleport_safe() {
    if (SafeTP <= 0)
        return;

    int x, y, tries = 0;

    do {
        x = 1 + rand() % (MAXX - 2);
        y = 1 + rand() % (MAXY - 2);
        tries++;
        if (tries > 2000)
            break;
    } while (!is_safe_spot(x, y));

    if (is_safe_spot(x, y)) SafeTP--;

    MyX = x;
    MyY = y;
}

/*--------------------------------------------------------------------------------------------
 * Perform anti-matter
 *--------------------------------------------------------------------------------------------*/
int do_am() {
    if (AntiMatter <= 0)
        return 0;

    int i;
    int used = 0;
    for (i = 0; i < NumRobots; i++) {
        int x = RobX[i];
        int y = RobY[i];

        if (!Dead[i] &&
            abs(x - MyX) <= 1 &&
            abs(y - MyY) <= 1) {
            kill_robot(i, 5);
            used = 1;
        }
    }

    AntiMatter -= used;
    return 1;
}

void end_game() {
    endwin();
    show_scores(scores, score_count);
}
