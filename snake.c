#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <ncurses.h>

#define ALLOC_CHUNK 64

#define FAILURE  0
#define SUCCESS  1
#define GAMEOVER 2

#define DEFAULT_X 0
#define DEFAULT_Y 0

#define DEFAULT_SPEED 8

#define MIN_WIDTH  16
#define MIN_HEIGHT 16

#define TIMEOUT 15

#define CELL_WIDTH 2

#define SNAKE_LOOK "SS"
#define FRUIT_LOOK "FF"

// DATA TYPES

enum COLORS {
    NEUTRAL_COLOR = 1,
    SNAKE_COLOR,
    FRUIT_COLOR
};

struct point {
    int x, y;
};

enum DIRECTION {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

struct state {
    int points;

    struct point *segments;
    int direction, max;

    struct point fruit;
};

// GLOBALS

int colors = 0;
int term_width, term_height;

unsigned long ticks = 0, speed = DEFAULT_SPEED;

// LOGIC

int init_state(struct state *s) {
    if ((s->segments = malloc(ALLOC_CHUNK * sizeof(struct point))) == NULL)
        return FAILURE;
    s->segments->x = DEFAULT_X, s->segments->y = DEFAULT_Y;

    s->points = 0;
    s->direction = EAST;

    s->max = ALLOC_CHUNK;

    return SUCCESS;
}

void free_state(struct state *s) {
    free(s->segments);
    s->segments = NULL;
}

int dir_value(int dir) {
    switch (dir) {
    case EAST: case SOUTH:
        return 1;
    case WEST: case NORTH:
        return -1;
    }

    return 0;
}

struct point *head(struct state *s) {
    return s->segments;
}

void move_head(struct state *s) {
    if (s->direction == EAST || s->direction == WEST)
        head(s)->x += dir_value(s->direction);
    else
        head(s)->y += dir_value(s->direction);

    if (head(s)->x < 0)
        head(s)->x = (term_width / CELL_WIDTH) - 1;
    else if (head(s)->x >= term_width / CELL_WIDTH)
        head(s)->x = 0;

    if (head(s)->y < 0)
        head(s)->y = term_height - 2;
    else if (head(s)->y >= term_height - 1)
        head(s)->y = 0;
}

void move_segments(struct state *s) {
    int i, px, py, tx, ty;
    struct point *p = s->segments;

    px = p->x, py = p->y;

    move_head(s);

    for (i = 1; i <= s->points; i++) {
        tx = s->segments[i].x, ty = s->segments[i].y;
        s->segments[i].x = px, s->segments[i].y = py;
        px = tx, py = ty;
    }
}

int extend(struct state *s) {
    struct point *tmp;
    if (s->points >= s->max - 1) {
        s->max += ALLOC_CHUNK;
        tmp = realloc(s->segments, s->max * sizeof(struct point));
        if (tmp == NULL)
            return FAILURE;
        else
            s->segments = tmp;
    }

    s->segments[s->points + 1].x = s->segments[s->points].x;
    s->segments[s->points + 1].y = s->segments[s->points].y;

    s->points++;

    return SUCCESS;
}

int contains_point(struct state *s, struct point *p) {
    int i;
    for (i = 1; i <= s->points; i++)
        if (p->x == s->segments[i].x && p->y == s->segments[i].y)
            return 1;
    return 0;
}

int eating_self(struct state *s) {
    return contains_point(s, head(s));
}

void randomize_fruit(struct state *s) {
    s->fruit.x = rand() % (term_width / 2);
    s->fruit.y = rand() % (term_height - 1);

    if (contains_point(s, &s->fruit))
        randomize_fruit(s);
}

// TERMINAL

void init_ncurses(void) {
    initscr();

    raw();
    noecho();
    timeout(TIMEOUT);
    keypad(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors() == TRUE) {
        start_color();

        colors = 1;

        init_pair(NEUTRAL_COLOR, COLOR_WHITE, COLOR_BLACK);
        init_pair(SNAKE_COLOR, COLOR_GREEN, COLOR_BLACK);
        init_pair(FRUIT_COLOR, COLOR_BLACK, COLOR_RED);
    }
    
    getmaxyx(stdscr, term_height, term_width);
}

void render_info(struct state *s) {
    if (colors)
        attron(COLOR_PAIR(NEUTRAL_COLOR));

    move(term_height - 1, 0);
    printw("POINTS: %d - MAX: %d",
           s->points, s->max);

    if (colors)
        attroff(COLOR_PAIR(NEUTRAL_COLOR));
}

void render_snake(struct state *s) {
    int i;

    if (colors)
        attron(COLOR_PAIR(SNAKE_COLOR));

    for (i = 0; i <= s->points; i++)
        mvprintw(s->segments[i].y, s->segments[i].x * CELL_WIDTH, SNAKE_LOOK);

    if (colors)
        attroff(COLOR_PAIR(SNAKE_COLOR));
}

void render_fruit(struct state *s) {
    if (colors)
        attron(COLOR_PAIR(FRUIT_COLOR));

    mvprintw(s->fruit.y, s->fruit.x * CELL_WIDTH, FRUIT_LOOK);

    if (colors)
        attroff(COLOR_PAIR(SNAKE_COLOR));
}

void render(struct state *s) {
    erase();

    getmaxyx(stdscr, term_height, term_width);

    if (term_width < MIN_WIDTH || term_height < MIN_HEIGHT)
        printw("TERM TOO SMALL");
    else {
        render_info(s);
        render_snake(s);
        render_fruit(s);
    }

    refresh();
}

void end_ncurses(void) {
    endwin();
}

// RUNNING

int is_integral(char *s) {
    for (; *s != '\0'; s++)
        if (!isdigit(*s) && *s != '-')
            return 0;
    return 1;
}

void fail(char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

void usage(char *prog_name) {
    printf("USAGE: %s [-h] [-s <SPEED>]\n"
           "\t-s <SPEED> :: Must be integral. 1 is fastest, 8 is default.\n"
           "CONTROLS:\n"
           "\tArrow keys to move.\n"
           "\tQ to quit.\n", prog_name);
}

void handle_args(int argc, char *argv[]) {
    int ns;
    char *end, *prog_name = *argv++;
    
    argc--;

    for (; argc > 0; argc--, argv++)
        if (!strcmp(*argv, "-h")) {
            usage(prog_name);
            exit(0);
        } else if (!strcmp(*argv, "-s"))
            if (argc-- > 1) {
                ns = strtol(*(++argv), &end, 10);
                if (end == *argv || !is_integral(*argv))
                    fail("Invalid number provided!");
                else if (ns < 1)
                    fail("Speed cannot be below 1!");
                else
                    speed = ns;
            } else
                fail("No speed value provided!");
        else
            fail("Invalid argument!");
}

void handle_key(struct state *game, int c) {
    switch (c) {
    case KEY_UP:
        if (game->direction != NORTH && game->direction != SOUTH)
            game->direction = NORTH;
        break;
    case KEY_RIGHT:
        if (game->direction != EAST && game->direction != WEST)
            game->direction = EAST;
        break;
    case KEY_DOWN:
        if (game->direction != SOUTH && game->direction != NORTH)
            game->direction = SOUTH;
        break;
    case KEY_LEFT:
        if (game->direction != WEST && game->direction != EAST)
            game->direction = WEST;
        break;
    }
}

int update(struct state *game, int c) {
    handle_key(game, c);
    
    move_segments(game);

    if (eating_self(game))
        return GAMEOVER;

    if (game->fruit.x >= term_width / CELL_WIDTH || game->fruit.y >= term_height)
        randomize_fruit(game);

    if (head(game)->x == game->fruit.x && head(game)->y == game->fruit.y) {
        if (extend(game) == FAILURE)
            return FAILURE;

        randomize_fruit(game);
    }

    render(game);

    return SUCCESS;
}

int main(int argc, char *argv[]) {
    int cb, c = '\0', last_state = SUCCESS;
    struct state game;

    handle_args(argc, argv);

    srand(time(NULL));

    if (init_state(&game) == FAILURE)
        fail("No memory available!");

    init_ncurses();

    randomize_fruit(&game);
    while ((cb = getch()) != 'q' && last_state == SUCCESS) {
        if (cb != -1)
            c = cb;

        if (ticks++ % speed == 0) {
            last_state = update(&game, c);
            c = '\0';
        }
    }

    end_ncurses();
    
    if (last_state == FAILURE)
        fprintf(stderr, "Ran out of memory!\n");
    else if (last_state == GAMEOVER)
        printf("GAME OVER! Finished with %d points!\n", game.points);

    free_state(&game);

    return 0;
}
