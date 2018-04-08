#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#define __USE_POSIX199309 = true;   // use nanosleep and struct timespec w/ c99
#include <time.h>
#include <curses.h>

#define DEFAULT_SNAKE_NODES 4 
#define GAME_LOOP_DELAY 50          // ms
#define MATRIX_ROWS 32              // MATRIX_ROWS * MATRIX_COLS must be 2^n 
#define MATRIX_COLS 32
#define STEP 1

#define mask(index) index & (MATRIX_ROWS * MATRIX_COLS - 1)

typedef struct snake_node
{
	int x, y;
} snake_node;

typedef enum direction
{
	UP, DOWN, LEFT, RIGHT
} direction;

typedef struct snake
{
	snake_node nodes[MATRIX_ROWS * MATRIX_COLS];
	size_t head, tail;
	direction direction;
} snake;

typedef enum cell
{
	EMPTY, SNAKE, FOOD, COLLISION
} cell;

typedef struct matrix
{
	cell cells[MATRIX_ROWS * MATRIX_COLS];
} matrix;

static void cleanup_window()
{
	clear();     // clear the window buffer
	refresh();   // flush the window buffer to screen
	endwin();    // clean up the window
}

static void panic()
{
	cleanup_window();
	exit(EXIT_FAILURE);
}

static long rand_to(long max)
{
	return rand() / (RAND_MAX / (max + 1) + 1);
}

static void chop_tail(snake *snake, matrix *matrix)
{
	snake_node const * const old_tail = &snake->nodes[mask(snake->tail)];
	matrix->cells[old_tail->x * MATRIX_COLS + old_tail->y] = EMPTY;
	(snake->tail)++;
}

static void place_food(matrix *matrix)
{
	// TODO: this gets REALLY ugly as the SNAKE/EMPTY cell ratio increases,
	//       randomize on a bucket of only EMPTY cells.
	static int const max_rand = (MATRIX_ROWS * MATRIX_COLS) - 1;
	cell * const food_cell = &matrix->cells[rand_to(max_rand)];
	if (*food_cell != SNAKE && *food_cell != COLLISION)
		*food_cell = FOOD;
	else
		place_food(matrix);
}

// TODO: move to ui module
static bool draw_matrix(matrix *matrix)
{
	// clear the window buffer
	clear();
 
 	// print the game matrix to the window buffer
	// TODO: only draw dirty regions?
	bool running = true;
	for (size_t x = 0; x < MATRIX_ROWS; ++x)
 	{
		for (size_t y = 0; y < MATRIX_COLS; ++y)
		{
 			switch (matrix->cells[x * MATRIX_COLS + y])
			{
				case SNAKE:
					mvprintw(y, x, "O");
					break;
				case FOOD:
					mvprintw(y, x, "+");
					break;
				case EMPTY:
					mvprintw(y, x, " ");
					break;
				case COLLISION:
					mvprintw(y, x, "X");
					running = false;
					break;
				default:      // unknown matrix cell type
					panic();
			}
		}
	}
    
	// flush the window buffer to the screen
	refresh();

	return running;
}

int main(void)
{
	// TODO: move to ui module
	initscr();               // initialize the default window
	curs_set(false);         // hide the cursor
	cbreak();                // read a character at a time
	keypad(stdscr, true);    // listen for arrow keys
	nodelay(stdscr, true);   // non blocking input
	noecho();                // don't echo input

	srand(time(NULL));       // seed the prng

	// allocate and initialize memory for the game matrix
	matrix * const matrix = malloc(sizeof(*matrix));
	if (!matrix)
		panic();
	memset(matrix->cells, EMPTY, MATRIX_ROWS * MATRIX_COLS * sizeof(cell));

	// allocate and initialize memory for the snake 
	snake * const snake = malloc(sizeof(*snake));
	if (!snake)
		panic();
	snake->head = DEFAULT_SNAKE_NODES - 1;
	snake->tail = 0;
	snake->direction = RIGHT;
	memset(snake->nodes, 0, MATRIX_ROWS * MATRIX_COLS * sizeof(snake_node)); 

	// queue the default snake nodes and set the direction
	for (size_t node_index = 0; node_index < DEFAULT_SNAKE_NODES; ++node_index)
	{
		snake->nodes[node_index].x = node_index;
		snake->nodes[node_index].y = 10;
		matrix->cells[node_index * MATRIX_COLS + 10] = SNAKE;
	}
   
	place_food(matrix);     // place the initial food item on the game matrix 

	// game loop
	bool running = true;
	while (running)
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
			default:        // unknown arrow key - noop
				break;
		}

		// add a new snake head based on the current direction 
		size_t const new_head_index = snake->head + STEP;
		snake_node * const new_head = &snake->nodes[mask(new_head_index)];
		snake_node * const old_head = &snake->nodes[mask(snake->head)];
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
			default:        // unknown direction
				panic();
		}
		snake->head = new_head_index;

		// resolve collisions
		cell * const new_head_cell = &matrix->cells[new_head->x * MATRIX_COLS + new_head->y];
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
			case SNAKE:     // snake collides with itself
				chop_tail(snake, matrix);
				*new_head_cell = COLLISION;
				break;
			default:        // unknown cell type
				panic();
		}

		running = draw_matrix(matrix);

		static struct timespec const ts = { 0, GAME_LOOP_DELAY * 1000000 };
		nanosleep(&ts, NULL);
	}

	free(snake);
	free(matrix);

	cleanup_window();
	return EXIT_SUCCESS;
}

