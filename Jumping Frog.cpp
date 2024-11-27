#define _CRT_SECURE_NO_WARNINGS
#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// window size
#define LINES 25
#define COLS 110
#define BORDER 1
#define FINISH 1

// colours:
#define MAIN_COLOR 1
#define FROG_COLOR 2
#define ROAD_COLOR 3
#define FROG_ROAD_COLOR 4
#define CAR_COLOR1 5 // colour pair for enemy car
#define CAR_COLOR2 6 // unused for now, colour pair for friendly car
#define BACKGROUND_COLOR COLOR_WHITE

// key definitions:
#define QUIT		'q'
#define NOKEY		' '

//time related definitions:
#define FRAME_TIME	25 // 25 ms (base frame time) (time interval between frames)
#define FROG_JUMP_TIME 25

// general definitions:
#define DELAY_OFF	0
#define DELAY_ON	1

#define SINGLE_LANE 1 // 4 rows wide
#define DOUBLE_LANE 2 // 7 rows wide
#define TRIPLE_LANE 3 // 10 rows wide

#define RA(min, max) ( (min) + rand() % ((max) - (min) + 1) )			// random number between min and max (inc)

const int numof_cars = 5;

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
	int speed; // speed per frame (how many frames betweeen moves)
	int interval;
	int width, height; // sizes
	char** appearance; // shape of the object (2-dim characters array (box))
} object_t;

typedef struct {
	window_t* win;
	int colour;
	int y; // top row of the road
	int width; // road spreads for this many rows down from and including y
	int speed; // speed of all cars using this road
	int numof_cars;
	object_t** cars;
} road_t;

typedef struct {
	unsigned int frame_time;
	int frame_no;
} timer_t;

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
	init_pair(CAR_COLOR1, COLOR_RED, COLOR_BLACK);

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
	// name and index number:
	mvwprintw(W->window, LINES - 1, BORDER + 1, "Stanislaw Kardas 203880");

	// finish line:
	wattron(W->window, COLOR_PAIR(COLOR_GREEN, BACKGROUND_COLOR));
	char* fin_line = (char*)malloc((COLS - 2 * BORDER + 1) * sizeof(char));
	memset(fin_line, '/', COLS - 2 * BORDER);
	fin_line[COLS - 2 * BORDER] = '\0';
	mvwprintw(W->window, BORDER, BORDER, "%s", fin_line);
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
	chtype ch = mvinch(y, x);
	int color_pair = PAIR_NUMBER(ch & A_COLOR); // extract color pair from ch

	short foregrnd, bckgrnd;
	pair_content(color_pair, &foregrnd, &bckgrnd); // extract foreground and background color from color_pair
	if (bckgrnd == COLOR_BLACK)
		return 1;
	return 0;
}

// ---------------------------status functions:---------------------------

void ShowTimer(window_t* W, float pass_time)
{
	wattron(W->window, COLOR_PAIR(MAIN_COLOR));
	mvwprintw(W->window, 0, BORDER + 1, "%.2f", pass_time / 1000);
	wrefresh(W->window);
}

// ---------------------------OBJ+ FUNCTIONS:---------------------------

void Print(object_t* object)
{
	for (int i = 0; i < object->height; i++)
		mvwprintw(object->win->window, object->y + i, object->x, "%s", object->appearance[i]);
}

void PrintBlank(object_t* object)
{
	wattron(object->win->window, COLOR_PAIR(ROAD_COLOR));
	for (int i = 0; i < object->height; i++)
		for (int j=0; j<object->width; j++)
			mvwprintw(object->win->window, object->y + i, object->x + j, " ");
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
	// 'rebuilding' lane bounding road or printing empty character after object passes 
	char* sw = (char*)malloc((object->width + 1) * sizeof(char));
	if ((mvinch(object->y, object->x + 1) & A_CHARTEXT) == '-' || (mvinch(object->y, object->x - 1) & A_CHARTEXT) == '-')
		memset(sw, '-', object->width);
	else
		memset(sw, ' ', object->width);
	sw[object->width] = '\0';

	// check whether to change background print colour based on current object position for blank space printing
	CheckRoad(object->y, object->x) ? wattron(object->win->window, COLOR_PAIR(ROAD_COLOR)) : wattron(object->win->window, COLOR_PAIR(object->bckg_colour));

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
		{
			for (int j = 0; j < object->height; j++)
			{
				if ((mvinch(object->y, object->x) & A_CHARTEXT) == '-')
					mvwprintw(object->win->window, object->y + j, object->x - i, "-");
				else
					mvwprintw(object->win->window, object->y + j, object->x - i, " ");
			}
		}
	}
	if ((moveX < 0) && (object->x > BORDER))
	{
		object->x += moveX;
		for (int i = 1; i <= abs(moveX); i++)
		{
			for (int j = 0; j < object->height; j++)
			{
				if ((mvinch(object->y, object->x) & A_CHARTEXT) == '-')
					mvwprintw(object->win->window, object->y + j, object->x + object->width + (i - 1), "-");
				else
					mvwprintw(object->win->window, object->y + j, object->x + object->width + (i - 1), " ");
			}
		}
	}

	// check whether to change background print colour based on where object is abour to move
	CheckRoad(object->y, object->x) ? wattron(object->win->window, COLOR_PAIR(object->rd_colour)) : wattron(object->win->window, COLOR_PAIR(object->bckg_colour));

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
	object->speed = FROG_JUMP_TIME;
	object->interval = 0;

	object->appearance = (char**)malloc(sizeof(char*));
	object->appearance[0] = (char*)malloc(sizeof(char));

	strcpy(object->appearance[0], "Q");

	return object;
}

object_t* InitCar(window_t* w, int col, int posY, int posX, int speed)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->bckg_colour = col;
	object->rd_colour = col;
	object->win = w;
	object->width = 3;
	object->height = 2;
	object->y = posY;
	object->x = posX;
	object->speed = speed;
	object->interval = 1;

	object->appearance = (char**)malloc(sizeof(char*) * object->height); // 2D table of char(acter)s
	for (int i = 0; i < object->height; i++)
		object->appearance[i] = (char*)malloc(sizeof(char) * (object->width + 1)); // +1: end-of-string (C): '\0'
	
	strcpy(object->appearance[0], "###\0");
	strcpy(object->appearance[1], "###\0");

	return object;
}

road_t* InitRoad(window_t* w, int posY, int lanes, int numof_cars)
{
	road_t* road = (road_t*)malloc(sizeof(road_t));
	road->colour = ROAD_COLOR;
	road->win = w;
	road->y = posY;
	road->width = lanes*3 + 1;
	road->speed = RA(FRAME_TIME / 4, FRAME_TIME);
	road->numof_cars = numof_cars;
	road->cars = (object_t**)malloc(road->numof_cars * sizeof(object_t*));
	for (int i = 0; i < road->numof_cars; i++)
	{
		road->cars[i] = InitCar(w, CAR_COLOR1, road->y + (RA(0, lanes - 1)*3 + 1), RA(BORDER + 1, COLS - BORDER - 3), road->speed);
	}
	return road;
}

void MoveFrog(object_t* object, int ch, unsigned int frame)
{
	if (frame - object->interval >= object->speed)
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
		object->interval = frame;
	}
}

void MoveCar(object_t* object, unsigned int frame)
{
	if (frame - object->interval >= object->speed)
	{
		if (object->x == COLS - 2 * BORDER - object->width)
		{
			PrintBlank(object);
			object->x = BORDER;
		}
		Show(object, 0, 1);
		object->interval = frame;
	}
}

// collision of two boxes
int Collision(object_t* f, object_t* c)
{
	if (((f->y >= c->y && f->y < c->y + c->height) || (c->y >= f->y && c->y < f->y + f->height)) &&
		((f->x >= c->x && f->x < c->x + c->width) || (c->x >= f->x && c->x < f->x + f->width)))
		return 1;
	else 	return 0;
}

// ---------------------------timer functions:---------------------------

void Sleep(unsigned int tui) 
{
	clock_t start_time = clock();
	clock_t end_time = start_time + (clock_t)(tui * CLOCKS_PER_SEC / 1000);
	while (clock() < end_time) { /* busy loop */ }
}

timer_t* InitTimer(window_t* status)
{
	timer_t* timer = (timer_t*)malloc(sizeof(timer_t));
	timer->frame_no = 1;
	timer->frame_time = FRAME_TIME;
	return timer;
}

void UpdateTimer(timer_t* T, window_t* status)
{
	T->frame_no++;
	Sleep(T->frame_time);
	ShowTimer(status, T->frame_time*T->frame_no);
}

// ---------------------------main loop:---------------------------

int MainLoop(window_t* status, object_t* frog, timer_t* timer, road_t** roads, int numof_roads)
{
	int ch;
	int pts = 0;
	while ((ch = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY; // ERR is ncurses predefined
		else 
		{
			MoveFrog(frog, ch, timer->frame_no);
			if (frog->y == FINISH) return 0; // TODO: win procedure
			
		}
		// movecar callout with collision checker
		for (int i = 0; i < numof_roads; i++)
		{
			for (int j = 0; j < roads[i]->numof_cars; j++)
			{
				MoveCar(roads[i]->cars[j], timer->frame_no);
				if (Collision(frog, roads[i]->cars[j])) return 0; // TODO: lose procedure
			}
		}
		flushinp(); // clear input buffer (avoiding multiple key pressed)
		UpdateTimer(timer, status);// update timer & sleep
	}
	return 0;
}

int main()
{
	int numof_roads = 2;

	WINDOW* mainwin = Start();

	window_t* playwin = Init(mainwin, LINES, COLS, 0, 0, MAIN_COLOR, DELAY_OFF);

	timer_t* timer = InitTimer(playwin);

	object_t* frog = InitFrog(playwin, FROG_COLOR, FROG_ROAD_COLOR);

	road_t** roads = (road_t**)malloc(numof_roads * sizeof(road_t*));

	roads[0] = InitRoad(playwin, 17, SINGLE_LANE, 2);
	roads[1] = InitRoad(playwin, 5, DOUBLE_LANE, 4);

	for (int i = 0; i < numof_roads; i++)
		PrintRoad(roads[i]);

	Show(frog, 0, 0);

	if (MainLoop(playwin, frog, timer, roads, numof_roads) == 0)
	{
		delwin(playwin->window);
		delwin(mainwin);
		endwin();
		refresh();
	}

	return 0;
}