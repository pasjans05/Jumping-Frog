﻿// this game was written using some base functions from demo game CATCH THE BALL

#define _CRT_SECURE_NO_WARNINGS
#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// window size
// #define LINES 30
// #define COLS 120
#define BORDER 1
#define FINISH 1

// colours:
#define MAIN_COLOR 1
#define FROG_COLOR 2
#define ROAD_EU_COLOR 3
#define ROAD_US_COLOR 4
#define FROG_ROAD_COLOR 5
#define CAR_TAXI_COLOR 6
#define CAR_COLOR1 7 // colour pair for enemy car
#define CAR_COLOR2 8 // colour pair for semi-enemy car - it will stop when you approach
#define OBSTACLE_COLOR 9
#define BACKGROUND_COLOR COLOR_WHITE
#define ROAD_BACKGROUND_COLOR COLOR_BLACK

// key definitions:
#define ENGAGE_TAXI 'f' // key used to 'enter the taxi' - demand that the frog is catched by friendly car (taxi); key choice inspired by Grand Theft Auto game franchise
#define QUIT		'q'
#define NOKEY		' '

//time related definitions:
#define FRAME_TIME	10 // ms (base frame time) (time interval between frames)
#define FROG_JUMP_TIME 15

// general definitions:
#define DELAY_OFF	0
#define DELAY_ON	1

#define MIN_CAR_SEPARATION 6
#define MAX_CAR_SEPARATION (3*MIN_CAR_SEPARATION)

#define UNIQE_CARS 3 // number of types of cars: taxi, red enemy car, magenta semi-enemy car

#define CAR_STOP_DISTANCE 2
#define CAR_CHANGE_CHANCE 5 // chance of a car disappearing when reaching the border and reappearing with changed attributes and possible delay are 1 in CAR_CHANGE_CHANCE
#define SPEED_CHANGE_CHANCE 50 // chance of speed of cars to be changed

#define SINGLE_LANE 1 // 4 rows wide
#define DOUBLE_LANE 2 // 7 rows wide
#define TRIPLE_LANE 3 // 10 rows wide

#define MAX_POINTS_TIME 10 // points are calculated as: (MAX_POINTS_TIME * FROG_JUMP_TIME) / (Y_PROGRES_FRAME - LAST_Y_PROGRESS_FRAME) so the faster you progress the more points you get!

#define RA(min, max) ( (min) + rand() % ((max) - (min) + 1) ) // random number between min and max (inc)

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
	int speed; // speed per frame (how many frames betweeen moves); for cars speed is coded so that sign indicates direction that the car is moving: + for right, - for left
	int interval;
	int width, height; // sizes
	char** appearance; // shape of the object (2D characters array (box))
} object_t;

typedef struct {
	window_t* win;
	int colour;
	int y; // top row of the road
	int width; // road spreads for this many rows down from and including y
	int speed; // speed of all cars using this road
	int *stopped; // array of the size of number of lanes with each value either 1|0 - whether whole lane is stopped due to semi-enemy car or not (default: 0)
	int numof_cars;
	object_t** cars;
} road_t;

typedef struct {
	unsigned int frame_time;
	int frame_no;
} timer_t;

typedef struct {
	int last_y_progres_frame;
	int y_progres;
	int points_count;
} point_t;

// basic calculation functions:
int LanesToX(int lanes) { return lanes * 3 + 1; }
int XToLanes(int x) { return (x - 1) / 3; }

// ---------------------------window functions:---------------------------

void PrintFrame(WINDOW* W)
{
	for (int i = 0; i < LINES; i++)
	{
		for (int j = 0; j < COLS; j++)
		{
			if (i == 0 && j == 0) mvwaddch(W, i, j, 201);
			else if (i == 0 && j == COLS - 1) mvwaddch(W, i, j, 187);
			else if (i == LINES - 1 && j == 0) mvwaddch(W, i, j, 200);
			else if (i == LINES - 1 && j == COLS - 1) mvwaddch(W, i, j, 188);
			else if (j == 0 || j == COLS - 1) mvwaddch(W, i, j, 186);
			else if (i == 0 || i == LINES - 1) mvwaddch(W, i, j, 205);
			else mvwaddch(W, i, j, ' ');
		}
	}
	// name and index number:
	mvwprintw(W, LINES - 1, BORDER + 1, "Stanislaw Kardas 203880");
}

void Welcome(WINDOW* win) // Welcome screen: press any key to continue
{
	wattron(win, COLOR_PAIR(MAIN_COLOR));
	PrintFrame(win);
	mvwaddstr(win, 1, 1, "Welcome to Jumping Frog: The Game");
	wattron(win, COLOR_PAIR(FROG_COLOR));
	mvwaddstr(win, 2, 1, "You are The Frog.");
	wattron(win, COLOR_PAIR(CAR_COLOR1));
	mvwaddstr(win, 3, 1, "Avoid cars of this colour.");
	wattron(win, COLOR_PAIR(CAR_COLOR2));
	mvwaddstr(win, 4, 1, "Cars of this colour will stop for you but still are dangerous.");
	wattron(win, COLOR_PAIR(CAR_TAXI_COLOR));
	mvwaddstr(win, 5, 1, "Taxis are friendly and can give you a lift when you press 'f'.");
	wattron(win, COLOR_PAIR(OBSTACLE_COLOR));
	mvwaddstr(win, 6, 1, "You can't pass obstacles.");
	wattron(win, COLOR_PAIR(MAIN_COLOR));
	mvwaddstr(win, 8, 1, "Press any key to start...");
	wgetch(win); // waiting here..
	wclear(win); // clear (after next refresh)
	wrefresh(win);
}

void EndScreen(WINDOW* w, int win, int pts) // End of the game screen: press any key to continue
{
	nodelay(w, FALSE);
	wattron(w, COLOR_PAIR(MAIN_COLOR));
	PrintFrame(w);
	if (win)
		mvwaddstr(w, 1, 1, "You won.");
	else
		mvwaddstr(w, 1, 1, "You lost.");
	mvwaddstr(w, 3, 1, "Your time: "); // TODO: endscreen time
	mvwaddstr(w, 4, 1, "Your points: "); // TODO: endscreen points (if ever implemented)
	mvwprintw(w, 4, 15, "%d", pts);
	mvwaddstr(w, 5, 1, "Some sort of leaderboard perhaps.");
	mvwaddstr(w, 6, 1, "I dont know what else should be on the endscreen.");
	mvwaddstr(w, 8, 1, "Press any key to quit...");
	wgetch(w); // waiting here..
	wclear(w); // clear (after next refresh)
	wrefresh(w);

	delwin(w);
	endwin();
	refresh();
	exit(0);
}

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
	init_pair(ROAD_EU_COLOR, COLOR_WHITE, ROAD_BACKGROUND_COLOR);
	init_pair(ROAD_US_COLOR, COLOR_YELLOW, ROAD_BACKGROUND_COLOR);
	init_pair(FROG_ROAD_COLOR, COLOR_GREEN, ROAD_BACKGROUND_COLOR);
	init_pair(CAR_TAXI_COLOR, COLOR_YELLOW, ROAD_BACKGROUND_COLOR);
	init_pair(CAR_COLOR1, COLOR_RED, ROAD_BACKGROUND_COLOR);
	init_pair(CAR_COLOR2, COLOR_MAGENTA, ROAD_BACKGROUND_COLOR);
	init_pair(OBSTACLE_COLOR, COLOR_CYAN, BACKGROUND_COLOR);

	noecho(); // Switch off echoing, turn off cursor
	curs_set(0);
	return win;
}

// default screen
void CleanWin(window_t* W)
{
	wattron(W->window, COLOR_PAIR(W->colour));
	// frame
	PrintFrame(W->window);

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
	keypad(W->window, TRUE);
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

void ShowStatus(window_t* W, object_t* o, int pts)
{
	wattron(W->window, COLOR_PAIR(MAIN_COLOR));
	mvwprintw(W->window, 0, 45, "x: %d  y: %d  ", o->x, o->y);
	mvwprintw(W->window, 0, 25, "%d", pts);
	wrefresh(W->window);
}

void ShowTimer(window_t* W, float pass_time)
{
	wattron(W->window, COLOR_PAIR(MAIN_COLOR));
	mvwprintw(W->window, 0, BORDER + 7, "%.2f", pass_time / 1000);
	wrefresh(W->window);
}

void ShowNewStatus(window_t* W, timer_t* T, object_t* o, int pts)
{
	wattron(W->window, COLOR_PAIR(MAIN_COLOR));
	mvwprintw(W->window, 0, BORDER+1, "Time: ");
	mvwprintw(W->window, 0, 17, "Points: ");
	ShowTimer(W, T->frame_time*T->frame_no);
	mvwprintw(W->window, 0, 35, "Position: ");
	mvwprintw(W->window, 0, 78, "Jumping-Frog-Game");
	ShowStatus(W, o, pts);
}

// ---------------------------OBJ+ FUNCTIONS:---------------------------

void Print(object_t* object)
{
	for (int i = 0; i < object->height; i++)
		mvwprintw(object->win->window, object->y + i, object->x, "%s", object->appearance[i]);
}

void PrintBlank(object_t* object)
{
	wattron(object->win->window, COLOR_PAIR(ROAD_EU_COLOR));
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

// function changing position of the object and replacing it's previous position with caracter that originaly were there
void Movements(object_t* obj, int moveY, int moveX, char* trail)
{	
	if ((moveY > 0) && (obj->y + obj->height < LINES - BORDER))
	{
		obj->y += moveY;
		for (int i = 1; i <= moveY; i++)
			mvwprintw(obj->win->window, obj->y - i, obj->x, "%s", trail);
	}
	if ((moveY < 0) && (obj->y > BORDER))
	{
		obj->y += moveY;
		for (int i = 1; i <= abs(moveY); i++)
			mvwprintw(obj->win->window, obj->y + obj->height, obj->x, "%s", trail);
	}
	if ((moveX > 0) && (obj->x + obj->width < COLS - BORDER))
	{
		obj->x += moveX;
		if (obj->x - moveX != 0)
			for (int i = 1; i <= moveX; i++)
			{
				for (int j = 0; j < obj->height; j++)
				{
					if ((mvinch(obj->y, obj->x) & A_CHARTEXT) == '-')
						mvwprintw(obj->win->window, obj->y + j, obj->x - i, "-");
					else
						mvwprintw(obj->win->window, obj->y + j, obj->x - i, " ");
				}
			}
	}
	if ((moveX < 0) && (obj->x > BORDER))
	{
		obj->x += moveX;
		if (obj->x - moveX != COLS - obj->width)
			for (int i = 1; i <= abs(moveX); i++)
			{
				for (int j = 0; j < obj->height; j++)
				{
					if ((mvinch(obj->y, obj->x) & A_CHARTEXT) == '-')
						mvwprintw(obj->win->window, obj->y + j, obj->x + obj->width + (i - 1), "-");
					else
						mvwprintw(obj->win->window, obj->y + j, obj->x + obj->width + (i - 1), " ");
				}
			}
	}
}

void Show(object_t* object, int moveY, int moveX, int road_colour)
{
	// 'rebuilding' lane bounding road or printing empty character after object passes 
	char* trail = (char*)malloc((object->width + 1) * sizeof(char));
	if ((mvinch(object->y, object->x + 1) & A_CHARTEXT) == '-' || (mvinch(object->y, object->x - 1) & A_CHARTEXT) == '-')
		memset(trail, '-', object->width);
	else
		memset(trail, ' ', object->width);
	trail[object->width] = '\0';

	// check whether to change background print colour based on current object position for blank space printing
	CheckRoad(object->y, object->x) ? wattron(object->win->window, COLOR_PAIR(road_colour)) : wattron(object->win->window, COLOR_PAIR(object->bckg_colour));

	// adjust y,x data stored in object_t and redraw background behind
	Movements(object, moveY, moveX, trail);

	// check whether to change background print colour based on where object is abour to move
	CheckRoad(object->y, object->x) ? wattron(object->win->window, COLOR_PAIR(object->rd_colour)) : wattron(object->win->window, COLOR_PAIR(object->bckg_colour));

	Print(object);

	wrefresh(object->win->window);
}

object_t* InitFrog(window_t* w, int col, int roadcol) // frog initialisation; note: frog is of a constant size of 1x1 (single character)
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

int CarColour()
{
	switch RA(0, 3)
	{
	case 0: case 1:
		return CAR_COLOR1;
		break;
	case 2:
		return CAR_COLOR2;
		break;
	case 3:
		return CAR_TAXI_COLOR;
		break;
	}
}

object_t* InitCar(window_t* w, int posY, int posX, int speed, int car_length, char car_char)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->rd_colour = CarColour();
	object->win = w;
	object->width = car_length;
	object->height = 2;
	object->y = posY;
	object->x = posX;
	object->speed = speed;
	object->interval = 1;

	object->appearance = (char**)malloc(sizeof(char*) * object->height); // 2D table of char(acter)s
	for (int i = 0; i < object->height; i++)
		object->appearance[i] = (char*)malloc(sizeof(char) * (object->width + 1)); // +1: end-of-string (C): '\0'
	
	char* car_line = (char*)malloc((object->width + 1) * sizeof(char));
	memset(car_line, car_char, object->width);
	car_line[object->width] = '\0';

	for (int i = 0; i < object->height; i++)
		object->appearance[i] = car_line;

	return object;
}

void AllocateCars(road_t* road, int car_lngth, char car_char)
{
	road->cars = (object_t**)malloc(sizeof(object_t*));

	int i, lane, last_lane = 0, posX, lastposX = BORDER + 1;

	for (i = 0; lastposX + car_lngth + MAX_CAR_SEPARATION < COLS - BORDER - car_lngth; i++)
	{
		road->cars = (object_t**)realloc(road->cars, (i+1)*sizeof(object_t*));
		lane = road->y + LanesToX(RA(0, XToLanes(road->width) - 1));
		posX = lastposX + car_lngth + RA(MIN_CAR_SEPARATION, MAX_CAR_SEPARATION);
		road->cars[i] = InitCar(road->win, lane, posX, road->speed, car_lngth, car_char);
		last_lane = lane; lastposX = posX;
	}
	road->numof_cars = i;
}

int CarSpeed(int car_speed) { return (car_speed/abs(car_speed))*RA(FRAME_TIME / abs(car_speed), FRAME_TIME / (abs(car_speed) / 2)); } // cs/abs(cs) indicated direction: 1 for right, -1 for left

road_t* InitRoad(window_t* w, int posY, int lanes, int col, int car_lngth, char car_char, int car_speed)
{
	road_t* road = (road_t*)malloc(sizeof(road_t));
	road->colour = col;
	road->win = w;
	road->y = posY;
	road->width = LanesToX(lanes);
	road->speed = CarSpeed(car_speed); // TODO: (optional) make speed an array so that each lane has a separate speed
	road->stopped = (int*)malloc(lanes * sizeof(int));
	for (int i = 0; i < lanes; i++) road->stopped[i] = 0;
	
	AllocateCars(road, car_lngth, car_char);

	return road;
}

object_t* InitObstacle(window_t* w, int posY, int posX, int col, int width, int height)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->win = w;
	object->y = posY;
	object->x = posX;
	object->bckg_colour = col;
	object->width = width;
	object->height = height;

	char* ob = (char*)malloc((object->width + 1) * sizeof(char));
	memset(ob, '7', object->width);
	ob[object->width] = '\0';

	object->appearance = (char**)malloc(sizeof(char*) * object->height);
	for (int i = 0; i < object->height; i++)
	{
		object->appearance[i] = (char*)malloc(sizeof(char) * (object->width + 1));
		object->appearance[i] = ob + '\0';
	}

	return object;
}

point_t* InitPoints()
{
	point_t* points = (point_t*)malloc(sizeof(point_t));
	points->last_y_progres_frame = 0;
	points->y_progres = LINES - BORDER;
	points->points_count = 0;
	return points;
}

int CalculatePoints(int frame_delta)
{
	int points = (MAX_POINTS_TIME * FROG_JUMP_TIME) / (frame_delta);
	return (points > 0 ? points : 1);
}

// function to check whether object 1 is within (deltaY, deltaX) of object 2 (doesn't work diagonally i think), for direct collisions deltaX,Y = 0
int Collision(object_t* ob1, object_t* ob2, int deltaY, int deltaX)
{
	if (((ob1->y + deltaY >= ob2->y && ob1->y + deltaY < ob2->y + ob2->height) || (ob2->y >= ob1->y + deltaY && ob2->y < ob1->y + ob1->height + deltaY)) &&
		((ob1->x + deltaX >= ob2->x && ob1->x + deltaX < ob2->x + ob2->width) || (ob2->x >= ob1->x + deltaX && ob2->x < ob1->x + ob1->width + deltaX))) return 1;
	return 0;
}

// check for collision with any of the obstacles and if there are none move the object; also calculates points
int ObstacleCheck(object_t** obstacles, int numof_obstacles, object_t* object, int moveY, int moveX, int road_color, point_t* points, int frame)
{
	for (int i = 0; i < numof_obstacles; i++)
	{
		if (Collision(object, obstacles[i], moveY, moveX)) return 1;
	}
	if (object->y + moveY < points->y_progres)
	{
		points->points_count += CalculatePoints(frame - points->last_y_progres_frame);
		points->last_y_progres_frame = frame;
		points->y_progres = object->y + moveY;
	}
	Show(object, moveY, moveX, road_color);

	return 0;
}

void MoveFrog(object_t* object, int ch, unsigned int frame, object_t** obstacle, int numof_obstacles, int road_color, point_t* points)
{
	if (frame - object->interval >= object->speed)
	{
		object->interval = frame;
		switch (ch) {
		case KEY_UP: ObstacleCheck(obstacle, numof_obstacles, object, -1, 0, road_color, points, frame); break;
		case KEY_DOWN: ObstacleCheck(obstacle, numof_obstacles, object, 1, 0, road_color, points, frame); break;
		case KEY_LEFT: ObstacleCheck(obstacle, numof_obstacles, object, 0, -1, road_color, points, frame); break;
		case KEY_RIGHT: ObstacleCheck(obstacle, numof_obstacles, object, 0, 1, road_color, points, frame); break;
		}
	}
}

void CarWrapping(object_t* object, int frame, int taxiing, int x_separation, int direction, int road_colour, int pts)
{
	PrintBlank(object);
	if (direction > 0)
		object->x = BORDER - 1;
	else
		object->x = COLS - BORDER - object->width + 1;

	if (taxiing) EndScreen(object->win->window, 0, pts);
	if (RA(1, CAR_CHANGE_CHANCE) % CAR_CHANGE_CHANCE == 0) // 1 in CAR_CHANGE_CHANCE chance of car changing
	{
		object->rd_colour = CarColour(); // car behaviour is connected to it's colour so changing car colour changes car
		if (x_separation > MIN_CAR_SEPARATION)
			object->interval = frame + RA(1, x_separation - MIN_CAR_SEPARATION); // time interval between previous car disappearing and new one appearing
	}
	else
	{
		object->interval = frame;
		Show(object, 0, direction, road_colour);
	}
}

void MoveCar(object_t* object, object_t* frog, int frame, int road_color, int taxiing, int x_separation, int pts)
{
	int moveX = object->speed / abs(object->speed); // taking the direction data from speed
	if (frame - object->interval >= abs(object->speed))
	{
		if ((moveX > 0 && object->x == COLS - BORDER - object->width) || (moveX < 0 && object->x == BORDER))
		{
			CarWrapping(object, frame, taxiing, x_separation, moveX, road_color, pts);
		}
		else
		{
			if (taxiing) Show(frog, 0, moveX, road_color);
			Show(object, 0, moveX, road_color);
			object->interval = frame;
		}
	}
}

// ---------------------------timer functions:---------------------------

void Sleep(unsigned int tui)
{
	clock_t start_time = clock();
	clock_t end_time = start_time + (clock_t)(tui * CLOCKS_PER_SEC / 1000);
	while (clock() < end_time) { /* while that doesn't end until specified time */ }
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

// ---------------------------levels:---------------------------

void Level1ne(road_t*** roads, object_t*** obstacles, int* numof_roads, int* numof_obstacles, window_t* w, int col, int car_length, char car_char, int car_speed)
{
	*numof_roads = 2;
	*numof_obstacles = 4;

	// initialise roads:

	road_t** roads1 = (road_t**)malloc(*numof_roads * sizeof(road_t*));

	roads1[0] = InitRoad(w, 17, SINGLE_LANE, col, car_length, car_char, -car_speed);
	roads1[1] = InitRoad(w, 5, DOUBLE_LANE, col, car_length, car_char, car_speed);

	*roads = roads1;

	// initialise obstacles:

	object_t** obstacles1 = (object_t**)malloc(*numof_obstacles * sizeof(object_t*));
	
	obstacles1[0] = InitObstacle(w, 15, 20, OBSTACLE_COLOR, 10, 1);
	obstacles1[1] = InitObstacle(w, 13, 100, OBSTACLE_COLOR, 3, 3);
	obstacles1[2] = InitObstacle(w, 2, 40, OBSTACLE_COLOR, 8, 2);
	obstacles1[3] = InitObstacle(w, 25, 50, OBSTACLE_COLOR, 20, 2);

	*obstacles = obstacles1;
}



// find the separation behind car number i from array of numof_cars cars
int CarSeparation(object_t** cars, int numof_cars, int i, int road_y)
{
	int iLane = XToLanes(cars[i]->y - road_y);
	if (cars[i]->speed < 0)
	{
		int j = (i < numof_cars - 1 ? i + 1 : 0);
		while (XToLanes(cars[j]->y - road_y) != iLane)
		{
			j++;
			if (j > numof_cars - 1) j = 0;
			else if (j == i) return COLS - 2 * BORDER - cars[i]->width;
		}
		if (i < j) return cars[j]->x - cars[i]->x - cars[i]->width; // substracted width could have been of any car since it's defined in config file to be the same for each
		else return (cars[j]->x - BORDER + (COLS - BORDER) - cars[i]->x - cars[j]->width); // wrapping separation
	}
	else if (cars[i]->speed > 0)
	{
		int j = (i > 0 ? i - 1 : numof_cars - 1);
		// find neares car behind on the same lane
		while (XToLanes(cars[j]->y - road_y) != iLane)
		{
			j--;
			if (j < 0) j = numof_cars - 1;
			else if (j == i) return COLS - 2 * BORDER - cars[i]->width; // only this car on the lane
		}
		// calculate the separation
		if (i > j) return cars[i]->x - cars[j]->x - cars[i]->width; // substracted width could have been of any car since it's defined in config file to be the same for each
		else return (cars[i]->x - BORDER + (COLS - BORDER) - cars[j]->x - cars[i]->width); // wrapping separation
	}
}

// ---------------------------main loop and related functions:---------------------------

// sub-function of CarsAction to do operations related to collisions
void CollisionAction(WINDOW* w, object_t* frog, road_t** roads, object_t** obstacles, point_t* points, int frame, int numof_obstacles, int ch, int* taxied, int* taxI, int* taxJ, int i, int j)
{
	if (roads[i]->cars[j]->rd_colour == CAR_TAXI_COLOR)
	{
		if (!*taxied && ch == 'f')
		{
			*taxied = 1; // indicate that frog is currently traveling by taxi
			*taxI = i; *taxJ = j; // indicate which taxi is frog currently traveling by
		}
		else if (*taxied)
		{
			if (ch == 'f')
			{
				ObstacleCheck(obstacles, numof_obstacles, frog, -1, 0, roads[0]->colour, points, frame); // moves frog one space up
				*taxied = 0;
			}
		}
	}
	else
		EndScreen(w, 0, points->points_count);
}

// sub-function of CarsAction to do operations related to semi-enemy car (one that stops when near a car)
void SemiEnemyCarAction(object_t* frog, road_t** roads, int currentLane, int* stopI, int* stopJ, int i, int j)
{
	int dir = roads[i]->cars[j]->speed / abs(roads[i]->cars[j]->speed);
	if (!roads[i]->stopped[currentLane] && Collision(frog, roads[i]->cars[j], 0, -CAR_STOP_DISTANCE * dir))
	{
		roads[i]->stopped[currentLane] = 1; // assign to the lane of the car stopped status
		*stopI = i; *stopJ = j;
	}
	else if (roads[i]->stopped[currentLane] && !Collision(frog, roads[*stopI]->cars[*stopJ], 0, -CAR_STOP_DISTANCE * dir))
		roads[i]->stopped[currentLane] = 0; // 'reopen' the lane
}

// function that randomly changes speed of 1 in a SPEED_CHANGE_CHANCE'th road
void RandomRoadSpeedChange(road_t* road, int car_speed)
{
	if (RA(1, SPEED_CHANGE_CHANCE) % SPEED_CHANGE_CHANCE == 0)
	{
		road->speed = CarSpeed(car_speed * (road->speed / abs(road->speed)));
		for (int j = 0; j < road->numof_cars; j++) road->cars[j]->speed = road->speed;
	}
}

void CarsAction(WINDOW* w, object_t* frog, int frame_no, object_t** obstacles, road_t** roads, point_t* points, int ch, int* taxied, int numof_obstacles, int numof_roads, int car_speed, int* stopI, int* stopJ, int* taxI, int* taxJ)
{
	int currentLane; // variable to store lane information of current car
	for (int i = 0; i < numof_roads; i++)
	{
		for (int j = 0; j < roads[i]->numof_cars; j++)
		{
			currentLane = XToLanes(roads[i]->cars[j]->y - roads[i]->y);
			if (roads[i]->stopped[currentLane] == 0) // moves car if current lane isn't stopped
				MoveCar(roads[i]->cars[j], frog, frame_no, roads[0]->colour, (*taxied && *taxI == i && *taxJ == j ? *taxied : 0), CarSeparation(roads[i]->cars, roads[i]->numof_cars, j, roads[i]->y), points->points_count);
			if (Collision(frog, roads[i]->cars[j], 0, 0))
			{
				CollisionAction(w, frog, roads, obstacles, points, frame_no, numof_obstacles, ch, taxied, taxI, taxJ, i, j);
			}
			else if (roads[i]->cars[j]->rd_colour == CAR_COLOR2) // semi-enemy car (magenta) operations (semi-enemy cars stop their lane when within CAR_STOP_DISTANCE of the frog)
			{
				SemiEnemyCarAction(frog, roads, currentLane, stopI, stopJ, i, j);
			}
			Show(frog, 0, 0, roads[i]->colour); // refresh frog so it doesn't disappear under another asset
		}
		RandomRoadSpeedChange(roads[i], car_speed);
	}
}

void MainLoop(window_t* status, object_t* frog, timer_t* timer, road_t** roads, int numof_roads, object_t** obstacles, int numof_obstacles, int car_speed, point_t* points)
{
	int ch, pts = 0;
	int taxI, taxJ, taxied = 0; // taxi identification (i, j) indicating which vechicle is frog taxing with bool variable to check whether frog is currently traveling by taxi
	int stopI, stopJ; // variable to store lane information of current car
	while ((ch = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY; // ERR is ncurses predefined
		else if (!taxied)
		{
			MoveFrog(frog, ch, timer->frame_no, obstacles, numof_obstacles, roads[0]->colour, points);
			if (frog->y == FINISH) EndScreen(status->window, 1, points->points_count);
		}
		// all car-related mechanics and operations:
		CarsAction(status->window, frog, timer->frame_no, obstacles, roads, points, ch, &taxied, numof_obstacles, numof_roads, car_speed, &stopI, &stopJ, &taxI, &taxJ);

		ShowStatus(status, frog, points->points_count);
		flushinp(); // clear input buffer (avoiding multiple key pressed)
		UpdateTimer(timer, status);// update timer & sleep
	}
	EndScreen(status->window, 0, points->points_count);
}

// ---------------------------config:---------------------------
void ReadConfig(FILE* config_file, char* frogger_appeal, int* car_length, char* car_char, int* car_speed, int* road_colour)
{
	char road_col[4]; // string storing type of road coloring theme (EUR/USA - 3 character long therefore string is 4 character long making space for null terminator '\0')
	fscanf(config_file, "Size and shape of the frog: %c\n", frogger_appeal);
	fscanf(config_file, "Car length (default 3): %d\n", car_length);
	fscanf(config_file, "Shape of a car (single character repeated as a block): %c\n", car_char);
	fscanf(config_file, "Cars speed multiplier (default 3, recommended between 2 and 5): %d\n", car_speed);
	fscanf(config_file, "Road theme (USA/EUR): %s\n", &road_col);
	if (road_col != NULL)
	{
		if (strcmp(road_col, "EUR") == 0) *road_colour = ROAD_EU_COLOR;
		else if (strcmp(road_col, "USA") == 0) *road_colour = ROAD_US_COLOR;
	}
}

int main()
{
	srand(time(NULL)); // new seed for each road definition
	int numof_roads, numof_obstacles;

	WINDOW* mainwin = Start();

	Welcome(mainwin);

	window_t* playwin = Init(mainwin, LINES, COLS, 0, 0, MAIN_COLOR, DELAY_OFF);

	timer_t* timer = InitTimer(playwin);

	object_t* frog = InitFrog(playwin, FROG_COLOR, FROG_ROAD_COLOR);

	point_t* points = InitPoints();

	// config file operations
	FILE* config_file;
	config_file = fopen("config.txt", "r");

	// default values in case file read fails
	char car_char = '#';
	int car_length = 3;
	int car_speed_multiplier = 3;
	int road_color = ROAD_EU_COLOR;
	
	if (config_file != NULL)
		ReadConfig(config_file, &frog->appearance[0][0], &car_length, &car_char, &car_speed_multiplier, &road_color);

	fclose(config_file);

	object_t** obstacles;
	road_t** roads;

	Level1ne(&roads, &obstacles, &numof_roads, &numof_obstacles, playwin, road_color, car_length, car_char, car_speed_multiplier);

	// printing roads and obstacles on the screen:
	for (int i = 0; i < numof_roads; i++)
		PrintRoad(roads[i]);

	for (int i = 0; i < numof_obstacles; i++)
		Show(obstacles[i], 0, 0, roads[0]->colour); // colour is taken from 1st road since it always has to exist and colour is common for all roads (defined at the start and may be changed by the config file)

	ShowNewStatus(playwin, timer, frog, 0);
	Show(frog, 0, 0, roads[0]->colour);

	MainLoop(playwin, frog, timer, roads, numof_roads, obstacles, numof_obstacles, car_speed_multiplier, points);
}