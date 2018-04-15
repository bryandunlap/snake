#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#define __USE_POSIX199309 = true;   // use nanosleep and struct timespec w/ c99
#include <time.h>
#include <curses.h>

#define DEFAULT_SNAKE_NODES 4 
#define GAME_LOOP_DELAY 200         // ms

#define MATRIX_X 32 
#define MATRIX_Y 32

#define STEP 1

#define RING_MASK(index) index & (MATRIX_X * MATRIX_Y - 1)
#define RAND_TO(max) rand() / (RAND_MAX / (max + 1) + 1)

typedef enum direction
{
	UP, DOWN, LEFT, RIGHT
} direction;

typedef struct snake_node
{
	int x, y;
} snake_node;

typedef struct snake
{
	snake_node nodes[MATRIX_X * MATRIX_Y];
	size_t head, tail;
	direction direction;
} snake;

typedef enum cell
{
	EMPTY, SNAKE, FOOD, WALL, COLLISION
} cell;

typedef struct matrix
{
	cell cells[MATRIX_X * MATRIX_Y];
} matrix;

typedef struct game_state
{
	matrix matrix;
	snake snake;
	bool collision;
} game_state;

static void cleanup(WINDOW *window)
{
	delwin(window);     // clean up the game window
	endwin();           // exit curses mode
}

static void panic(char *message)
{
	endwin();
	puts(message);
	exit(EXIT_FAILURE);
}

static void chop_tail(snake *snake, matrix *matrix)
{
	snake_node const * const old_tail = &snake->nodes[RING_MASK(snake->tail)];
	matrix->cells[old_tail->x * MATRIX_Y + old_tail->y] = EMPTY;
	(snake->tail)++;
}

static void place_food(matrix *matrix)
{
	// TODO: need to randomize on a bucket of only EMPTY cells.
	static int const max_rand = (MATRIX_X * MATRIX_Y) - 1;
	cell * const food_cell = &matrix->cells[RAND_TO(max_rand)];
	if (*food_cell != SNAKE && *food_cell != WALL && *food_cell != COLLISION)
		*food_cell = FOOD;
	else
		place_food(matrix);
}

#define WALL_PAIR 1
#define SNAKE_PAIR 2
#define FOOD_PAIR 3
#define COLLISION_PAIR 4

int main(void)
{
	// TODO: move to ui module
	initscr();               // initialize curses mode 
	curs_set(false);         // hide the cursor
	cbreak();                // read a character at a time
	keypad(stdscr, true);    // listen for arrow keys
	nodelay(stdscr, true);   // non blocking input
	noecho();                // don't echo input

	if (has_colors())
		start_color();
	else
		panic("terminal does not support colors");

	// define color pairs for game entities
	init_pair(WALL_PAIR, COLOR_WHITE, COLOR_WHITE);
	init_pair(SNAKE_PAIR, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(FOOD_PAIR, COLOR_GREEN, COLOR_GREEN);
	init_pair(COLLISION_PAIR, COLOR_RED, COLOR_RED);

	// prepare the game window
	WINDOW * const window = newwin(MATRIX_Y, 
		MATRIX_X * 2, 
		(LINES - MATRIX_Y) / 2,
		(COLS - MATRIX_X * 2) / 2);

	// allocate game memory
	game_state * const game_state = malloc(sizeof(*game_state));
	if (!game_state)
		panic("game state malloc failed");

	// initialize game state
	game_state->collision = false;

	// initialize memory for the matrix
	matrix * const matrix = &game_state->matrix;
	memset(matrix->cells, EMPTY, MATRIX_X * MATRIX_Y * sizeof(cell));

	// initialize memory for the snake 
	snake * const snake = &game_state->snake;
	snake->head = DEFAULT_SNAKE_NODES - 1;
	snake->tail = 0;
	snake->direction = RIGHT;
	memset(snake->nodes, 0, MATRIX_X * MATRIX_Y * sizeof(snake_node)); 

	// place the walls on the game matrix
	for (size_t wall_x = 0; wall_x < MATRIX_X; ++wall_x)
	{
		for (size_t wall_y = 0; wall_y < MATRIX_Y; ++wall_y)
		{
			if ((wall_x == 0 || wall_x == MATRIX_X - 1)
				|| (wall_y == 0 || wall_y == MATRIX_Y - 1))
			{
				matrix->cells[wall_x * MATRIX_Y + wall_y] = WALL;
			}
		}
	}
	
	// queue the default snake nodes and set the direction
	size_t const x_offset = 5;
	for (size_t node_index = 0; node_index < DEFAULT_SNAKE_NODES; ++node_index)
	{
		snake->nodes[node_index].x = node_index + x_offset;
		snake->nodes[node_index].y = 10;
		matrix->cells[(node_index + x_offset) * MATRIX_Y + 10] = SNAKE;
	}
   
	srand(time(NULL));       // seed the prng

	place_food(matrix);     // place the first food item on the game matrix 

	// game loop
	while (!game_state->collision)
	{
		switch (getch())    // process arrow key input 
		{
			case KEY_UP:
				snake->direction = snake->direction == DOWN ? DOWN : UP;
				break;
			case KEY_DOWN:
				snake->direction = snake->direction == UP ? UP : DOWN;
				break;
			case KEY_LEFT:
				snake->direction = snake->direction == RIGHT ? RIGHT : LEFT;
				break;
			case KEY_RIGHT:
				snake->direction = snake->direction == LEFT ? LEFT : RIGHT;
				break;
			default:        // unknown keypress - noop
				break;
		}

		// add a new snake head based on the current direction 
		size_t const new_head_index = snake->head + STEP;
		snake_node * const new_head = &snake->nodes[RING_MASK(new_head_index)];
		snake_node * const old_head = &snake->nodes[RING_MASK(snake->head)];
		switch (snake->direction)
		{
			case UP:
				new_head->x = old_head->x;
				new_head->y = old_head->y - STEP;
				break;
			case DOWN:
				new_head->x = old_head->x;
				new_head->y = old_head->y + STEP;
				break;
			case LEFT:
				new_head->x = old_head->x - STEP;
				new_head->y = old_head->y;
				break;
			case RIGHT:
				new_head->x = old_head->x + STEP;
				new_head->y = old_head->y;
				break;
			default:
				panic("invalid direction");
		}
		snake->head = new_head_index;

		// resolve movement, check for collisions and update the game matrix 
		cell * const new_head_cell = &matrix->cells[new_head->x * MATRIX_Y + new_head->y];
		switch (*new_head_cell)
		{
			case EMPTY:     // snake moves into empty space
				chop_tail(snake, matrix);
				*new_head_cell = SNAKE;
				break;
			case FOOD:      // snake eats food
				*new_head_cell = SNAKE;
				place_food(matrix);
				break;
			case WALL:      // snake collides with itself
				chop_tail(snake, matrix);
			case SNAKE:     // snake collides with wall 
				*new_head_cell = COLLISION;
				game_state->collision = true;
				break;
			default:
				panic("invalid cell type");
		}

		// draw the game matrix to the window buffer
		// TODO: move to ui module
		for (size_t x = 0; x < MATRIX_X; ++x)
		{
			for (size_t y = 0; y < MATRIX_Y; ++y)
			{
				switch (matrix->cells[x * MATRIX_Y + y])
				{
					case WALL:
						wattron(window, COLOR_PAIR(WALL_PAIR));
						mvwprintw(window, y, x * 2, "##");
						wattroff(window, COLOR_PAIR(WALL_PAIR));
						break;
					case SNAKE:
						wattron(window, COLOR_PAIR(SNAKE_PAIR));
						mvwprintw(window, y, x * 2, "OO");
						wattroff(window, COLOR_PAIR(SNAKE_PAIR));
						break;
					case FOOD:
						wattron(window, COLOR_PAIR(FOOD_PAIR));
						mvwprintw(window, y, x * 2, "++");
						wattroff(window, COLOR_PAIR(FOOD_PAIR));
						break; 
					case EMPTY:
						mvwprintw(window, y, x * 2, "  ");
						break;
					case COLLISION:
						wattron(window, COLOR_PAIR(COLLISION_PAIR));
						mvwprintw(window, y, x * 2, "XX");
						wattroff(window, COLOR_PAIR(COLLISION_PAIR));
						break;
					default:
						panic("unknown matrix cell type");
				}
			}
		}

		// flush the window buffer to the screen
		wrefresh(window);

		static struct timespec const ts = { 0, GAME_LOOP_DELAY * 1000000 };
		nanosleep(&ts, NULL);
	}

	free(game_state);
	cleanup(window);

	return EXIT_SUCCESS;
}

