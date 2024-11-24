#define _CRT_SECURE_NO_WARNINGS
#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// window size
#define LINES 25
#define COLS 110
#define BORDER 1

// colours:
#define MAIN_COLOR 1
#define FROG_COLOR 2
#define ROAD_COLOR 3
#define FROG_ROAD_COLOR 4
#define BACKGROUND_COLOR COLOR_WHITE

// key definitions:
#define QUIT		'q'
#define NOKEY		' '

// general definitions:
#define DELAY_OFF	0
#define DELAY_ON	1

#define SINGLE_LANE 4
#define DOUBLE_LANE 7
#define TRIPLE_LANE 10


// structures:
typedef struct {
	WINDOW* window; // ncurses window
	int x, y;
	int lines, cols;
	int colour;
} window_t; // my window

// moving object structure inside win(dow)
typedef struct {
	window_t* win;
	// in case of cars both bckg and rd colours are the same
	int bckg_colour; // normal color (background color as defined)
	int rd_colour; // background color is road color
	int x, y;
	int width, height; // sizes
	char** appearance; // shape of the object (2-dim characters array (box))
} object_t;

typedef struct {
	window_t* win;
	int colour;
	int y; // top row of the road
	int width; // road spreads for this many rows down from and including y
} road_t;


WINDOW* Start()
{
	WINDOW* win;
	if ((win = initscr()) == NULL) // initialize ncurses
	{
		fprintf(stderr, "Error initialising ncurses.\n");
		exit(EXIT_FAILURE);
	}

	start_color(); // initialize colors
	init_pair(MAIN_COLOR, COLOR_BLUE, BACKGROUND_COLOR);
	init_pair(FROG_COLOR, COLOR_GREEN, BACKGROUND_COLOR);
	init_pair(ROAD_COLOR, COLOR_WHITE, COLOR_BLACK);
	init_pair(FROG_ROAD_COLOR, COLOR_GREEN, COLOR_BLACK);

	noecho(); // Switch off echoing, turn off cursor
	curs_set(0);
	return win;
}

// default screen
void CleanWin(window_t* W)
{
	int i, j;
	wattron(W->window, COLOR_PAIR(W->colour));
	// frame
	for (int i = 0; i < LINES; i++)
	{
		for (int j = 0; j < COLS; j++)
		{
			if (i == 0 && j == 0) mvwaddch(W->window, i, j, 201);
			else if (i == 0 && j == COLS - 1) mvwaddch(W->window, i, j, 187);
			else if (i == LINES - 1 && j == 0) mvwaddch(W->window, i, j, 200);
			else if (i == LINES - 1 && j == COLS - 1) mvwaddch(W->window, i, j, 188);
			else if (j == 0 || j == COLS - 1) mvwaddch(W->window, i, j, 186);
			else if (i == 0 || i == LINES - 1) mvwaddch(W->window, i, j, 205);
			else mvwaddch(W->window, i, j, ' ');
		}
	}
}

// window initialization: position, colors, border, etc
window_t* Init(WINDOW* parent, int lines, int cols, int y, int x, int colour, int delay)
{
	window_t* W = (window_t*)malloc(sizeof(window_t));
	W->x = x; W->y = y; W->lines = lines; W->cols = cols; W->colour = colour;
	W->window = subwin(parent, lines, cols, y, x);
	CleanWin(W);
	if (delay == DELAY_OFF) // non-blocking reading of characters (for real-time game)
		nodelay(W->window, TRUE);
	wrefresh(W->window);
	return W;
}

int CheckRoad(int y, int x)
{
	// Get the character and its attributes from the standard screen at (y, x) 
	chtype ch = mvinch(y, x);
	// Extract the color pair from the attributes 
	int color_pair = PAIR_NUMBER(ch & A_COLOR);
	// Return 1 if color pair is 1 (COLOR_WHITE on COLOR_BLACK), otherwise return 0 
	if (color_pair == ROAD_COLOR)
		return 1;
	return 0;
}

// TODO: OBJ+ FUNCTIONS
void Print(object_t* object)
{
	for (int i = 0; i < object->height; i++)
		mvwprintw(object->win->window, object->y + i, object->x, "%s", object->appearance[i]);
}

void PrintRoad(road_t* road)
{
	wattron(road->win->window, COLOR_PAIR(road->colour));

	// lines at the begining and end of road:
	char* rdline = (char*)malloc((COLS - 2 * BORDER + 1) * sizeof(char)); // screen inside the borders + 1 for escape character \0
	memset(rdline, '-', COLS - 2 * BORDER);
	rdline[COLS - 2 * BORDER] = '\0'; // avoiding junk characters
	// empty road:
	char* rdempty = (char*)malloc((COLS - 2 * BORDER + 1) * sizeof(char));
	memset(rdempty, ' ', COLS - 2 * BORDER);
	rdempty[COLS - 2 * BORDER] = '\0';

	for (int i = 0; i < road->width; i++)
	{
		if (i % 3 == 0) mvwprintw(road->win->window, road->y + i, BORDER, "%s", rdline); // every lane is 2 lines wide, bounded by rdline
		else mvwprintw(road->win->window, road->y + i, BORDER, "%s", rdempty);
	}
	wrefresh(road->win->window);
}

void Show(object_t* object, int moveY, int moveX)
{
	char* sw = (char*)malloc((object->width + 1) * sizeof(char));
	memset(sw, ' ', object->width);
	sw[object->width] = '\0';

	CheckRoad(object->y + moveY, object->x + moveX) ? wattron(object->win->window, COLOR_PAIR(object->rd_colour)) : wattron(object->win->window, COLOR_PAIR(object->bckg_colour));

	// movements:
	if ((moveY > 0) && (object->y + object->height < LINES - BORDER))
	{
		object->y += moveY;
		for (int i = 1; i <= moveY; i++)
			mvwprintw(object->win->window, object->y - i, object->x, "%s", sw);
	}
	if ((moveY < 0) && (object->y > BORDER))
	{
		object->y += moveY;
		for (int i = 1; i <= abs(moveY); i++)
			mvwprintw(object->win->window, object->y + object->height, object->x, "%s", sw);
	}
	if ((moveX > 0) && (object->x + object->width < COLS - BORDER))
	{
		object->x += moveX;
		for (int i = 1; i <= moveX; i++)
			for (int j = 0; j < object->height; j++)
				mvwprintw(object->win->window, object->y + j, object->x - i, " ");
	}
	if ((moveX < 0) && (object->x > BORDER))
	{
		object->x += moveX;
		for (int i = 1; i <= abs(moveX); i++)
			for (int j = 0; j < object->height; j++)
				mvwprintw(object->win->window, object->y + j, object->x + object->width + (i - 1), " ");
	}

	Print(object);

	wrefresh(object->win->window);
}

object_t* InitFrog(window_t* w, int col, int roadcol)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->bckg_colour = col;
	object->rd_colour = roadcol;
	object->win = w;
	object->width = 1;
	object->height = 1;
	object->y = LINES - (BORDER + 1);
	object->x = COLS / 2;

	object->appearance = (char**)malloc(sizeof(char*));
	object->appearance[0] = (char*)malloc(sizeof(char));

	strcpy(object->appearance[0], "Q");

	return object;
}

object_t* InitCar(window_t* w, int col, int posY, int posX)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->bckg_colour = col;
	object->rd_colour = col;
	object->win = w;
	object->width = 3;
	object->height = 2;
	object->y = posY;
	object->x = posX;

	object->appearance = (char**)malloc(sizeof(char*) * object->height); // 2D table of char(acter)s
	for (int i = 0; i < object->height; i++)
		object->appearance[i] = (char*)malloc(sizeof(char) * (object->width + 1)); // +1: end-of-string (C): '\0'
	
	strcpy(object->appearance[0], "###");
	strcpy(object->appearance[1], "###");

	return object;
}

road_t* InitRoad(window_t* w, int posY, int lanes)
{
	road_t* road = (road_t*)malloc(sizeof(road_t));
	road->colour = ROAD_COLOR;
	road->win = w;
	road->y = posY;
	road->width = lanes;
	return road;
}

void MoveFrog(object_t* object, int ch)
{
	switch (ch) {
	//case KEY_UP: Show(object, -1, 0); break;
	//case KEY_DOWN: Show(object, 1, 0); break;
	//case KEY_LEFT: Show(object, 0, -1); break;
	//case KEY_RIGHT: Show(object, 0, 1);
	case 'w': Show(object, -1, 0); break;
	case 's': Show(object, 1, 0); break;
	case 'a': Show(object, 0, -1); break;
	case 'd': Show(object, 0, 1); break;
	}
}

// TODO: cars movements (speed related)

// TODO: collision

// TODO: timer functions

int MainLoop(window_t* status, object_t* frog)
{
	int ch;
	int pts = 0;
	while ((ch = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY; // ERR is ncurses predefined
		else MoveFrog(frog, ch);
		// TODO: movecar callout
		flushinp(); // clear input buffer (avoiding multiple key pressed)
		// TODO: update timer & sleep
	}
	return 0;
}

int main()
{
	int n_of_roads = 1;

	WINDOW* mainwin = Start();

	window_t* playwin = Init(mainwin, LINES, COLS, 0, 0, MAIN_COLOR, DELAY_ON);

	object_t* frog = InitFrog(playwin, FROG_COLOR, FROG_ROAD_COLOR);

	road_t** roads = (road_t**)malloc(n_of_roads * sizeof(road_t*));


	// roads initialisation, TODO: random road width
	for (int i = 0; i < n_of_roads; i++)
		roads[i] = InitRoad(playwin, 3, SINGLE_LANE);

	for (int i = 0; i < sizeof(roads) / sizeof(roads[0]); i++)
		PrintRoad(roads[i]);

	Show(frog, 0, 0);

	if (MainLoop(playwin, frog) == 0)
	{
		delwin(playwin->window);
		delwin(mainwin);
		endwin();
		refresh();
	}

	return 0;
}