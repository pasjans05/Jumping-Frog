// this game was written using some base functions from demo game CATCH THE BALL

#define _CRT_SECURE_NO_WARNINGS
#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

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
#define STORK_COLOR 10
#define STORK_ROAD_COLOR 11
#define STORK_OBSTACLE_COLOR 12
#define BACKGROUND_COLOR COLOR_WHITE
#define ROAD_BACKGROUND_COLOR COLOR_BLACK

// key definitions:
#define ENGAGE_TAXI 'f' // key used to 'enter the taxi' - demand that the frog is catched by friendly car (taxi)
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

#define UNIQUE_CARS 3 // number of types of cars: taxi, color1 enemy car, color2 semi-enemy car

#define CAR_STOP_DISTANCE 2
#define CAR_CHANGE_CHANCE 5 // chance of a car disappearing when reaching the border and reappearing with changed attributes and possible delay are 1 in CAR_CHANGE_CHANCE
#define SPEED_CHANGE_CHANCE 50 // chance of speed of cars to be changed
#define STORK_SPEED_MULTIPLIER 3 // value by which frog speed is divided to get stor speed, it is how much slower stork is than the frog

#define SINGLE_LANE 1 // 4 rows wide
#define DOUBLE_LANE 2 // 7 rows wide
#define TRIPLE_LANE 3 // 10 rows wide

#define MAX_POINTS_TIME 10 // points are calculated as: (MAX_POINTS_TIME * FROG_JUMP_TIME) / (Y_PROGRES_FRAME - LAST_Y_PROGRESS_FRAME) so the faster you progress the more points you get!
#define MAX_LEADERBOARD_NAME_LENGTH 16 // maximum name length for names appearing on leaderboard
#define MAX_LEADERBOARD_LENGTH (MAX_LEADERBOARD_NAME_LENGTH + 10) // maximum space that leaderboard can take width-wise - maximum name length + 10 characters reserved for score, position number and margin

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
	int bckg_colour; // normal color (background color as defined); in case of cars only rd colour is initialised
	int rd_colour; // background color is road color; in case of cars only rd colour is initialised
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
	int level_multiplier;
} point_t;

typedef struct {
	int no;
	char name[MAX_LEADERBOARD_NAME_LENGTH];
	int points;
} leaderboard_t;

// basic calculation functions:
int LanesToX(int lanes) { return lanes * 3 + 1; }
int XToLanes(int x) { return (x - 1) / 3; }

// prints frame around the border and fills the middle with white characters
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

// ---------------------------leaderboard (ranking) functions:---------------------------

// check if leaderboard file exists and if so read it's content into array of leaderboard_t structs
leaderboard_t** ReadLeaderboard(int* numof_leaderboard)
{
	FILE* ranking_file;
	ranking_file = fopen("leaderboard.txt", "r");
	if (ranking_file == NULL)
	{
		*numof_leaderboard = 0;
		return NULL;
	}
	else
	{
		int i = 0; // iterator for while loop ranking file reading
		leaderboard_t** ranking = (leaderboard_t**)malloc(sizeof(leaderboard_t*));
		ranking[i] = (leaderboard_t*)malloc(sizeof(leaderboard_t));

		while (fscanf(ranking_file, "%d. %s %d", &ranking[i]->no, &ranking[i]->name, &ranking[i]->points) == 3)
		{
			i++;
			ranking = (leaderboard_t**)realloc(ranking, (i + 1) * sizeof(leaderboard_t*));
			ranking[i] = (leaderboard_t*)malloc(sizeof(leaderboard_t));
		}
		fclose(ranking_file);
		*numof_leaderboard = i;
		return ranking;
	}
}

// override previous leaderboard file with it's updated version
void SaveLeaderboard(leaderboard_t** leaderboard, int numof_leaderboard)
{
	FILE* ranking_file;
	ranking_file = fopen("leaderboard.txt", "w");
	for (int i = 0; i < numof_leaderboard; i++)
		fprintf(ranking_file, "%d. %s %d\n", leaderboard[i]->no, leaderboard[i]->name, leaderboard[i]->points);
	fclose(ranking_file);
}

// add new_entry to the leaderboard at the right position
void AddToLeaderboard(leaderboard_t** leaderboard, int numof_leaderboard, leaderboard_t* new_entry)
{
	leaderboard_t* temp = (leaderboard_t*)malloc(sizeof(leaderboard_t));
	for (int i = 0; i < numof_leaderboard; i++)
	{
		if (i == numof_leaderboard - 1)
		{
			leaderboard[i] = new_entry;
			leaderboard[i]->no = i + 1;
			free(temp);
			break;
		}
		else if (new_entry->points > leaderboard[i]->points)
		{
			temp = leaderboard[i];
			leaderboard[i] = new_entry;
			leaderboard[i]->no = i + 1;
			new_entry = temp;
			for (int j = i + 1; j < numof_leaderboard; j++)
			{
				temp = leaderboard[j];
				leaderboard[j] = new_entry;
				leaderboard[j]->no = j + 1;
				new_entry = temp;
			}
			free(temp);
			break;
		}
	}
}

// create new entry to be added leaderboard and callout function that saves it to file
void AddLeaderboardEntry(WINDOW* w, leaderboard_t** leaderboard, int* numof_leaderboard, int points)
{
	leaderboard_t* new_entry = (leaderboard_t*)malloc(sizeof(leaderboard_t));
	new_entry->points = points;

	// screen of entering a name
	PrintFrame(w);
	mvwprintw(w, 1, 1, "Your points: %d", points);
	mvwprintw(w, 2, 1, "Please enter your name (up to %d characters) and confirm it with down arrow key:", MAX_LEADERBOARD_NAME_LENGTH - 1);

	// name entering:
	int ch, i;
	for (i = 0; i < MAX_LEADERBOARD_NAME_LENGTH - 1; i++)
	{
		ch = wgetch(w);
		if (ch == KEY_DOWN)
			break;
		else if (ch >= 33 && ch <= 126) // range of accepted characters
		{
			new_entry->name[i] = ch;
			mvwaddch(w, 3, i + 1, ch);
		}
		else i--;
	}
	new_entry->name[i] = '\0';

	// find the right place on the leaderboard and sort it:
	*numof_leaderboard += 1;
	if (*numof_leaderboard == 1)
		leaderboard = (leaderboard_t**)malloc(*numof_leaderboard * sizeof(leaderboard_t*));
	else
		leaderboard = (leaderboard_t**)realloc(leaderboard, *numof_leaderboard * sizeof(leaderboard_t*));
	leaderboard[*numof_leaderboard - 1] = (leaderboard_t*)malloc(sizeof(leaderboard_t));
	
	AddToLeaderboard(leaderboard, *numof_leaderboard, new_entry);
	
	SaveLeaderboard(leaderboard, *numof_leaderboard);

	free(new_entry);
}

void PrintLeaderboard(WINDOW* win, leaderboard_t** leaderboard, int numof_leaderboard)
{
	if (numof_leaderboard)
		mvwaddstr(win, BORDER, COLS - MAX_LEADERBOARD_LENGTH, "Leaderboard: ");
	else
		mvwaddstr(win, BORDER, COLS - MAX_LEADERBOARD_LENGTH, "No leaderboard yet.");

	for (int i = 0; i < numof_leaderboard && i < LINES - BORDER; i++)
	{
		mvwprintw(win, i + BORDER + 1, COLS - MAX_LEADERBOARD_LENGTH, "%d. %s %d", leaderboard[i]->no, leaderboard[i]->name, leaderboard[i]->points);
	}
}

// ---------------------------window functions:---------------------------

// terminate execution of the program
void End(WINDOW* w)
{
	delwin(w);
	endwin();
	refresh();
	exit(0);
}

// Welcome screen: press any key to continue
void Welcome(WINDOW* win, leaderboard_t** leaderboard, int numof_leaderboard)
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
	wattron(win, COLOR_PAIR(STORK_COLOR));
	mvwaddstr(win, 7, 1, "BEWARE THE STORK STARTING IN THE TOP LEFT CORNER AS HE TRIES TO CATCH YOU!");
	wattron(win, COLOR_PAIR(MAIN_COLOR));
	mvwaddstr(win, 9, 1, "Press any key to continue...");

	PrintLeaderboard(win, leaderboard, numof_leaderboard);

	wgetch(win); // waiting here..
	wclear(win); // clear (after next refresh)
	wrefresh(win);
}

// End of the game screen: press any key to continue
void EndScreen(WINDOW* w, int pts, leaderboard_t** leaderboard, int numof_leaderboard)
{
	nodelay(w, FALSE);
	wattron(w, COLOR_PAIR(MAIN_COLOR));
	PrintFrame(w);
	PrintLeaderboard(w, leaderboard, numof_leaderboard);
	mvwaddstr(w, 8, 1, "Press any key to quit...");
	if (pts)
	{
		mvwaddstr(w, 1, 1, "You won.");
		mvwaddstr(w, 4, 1, "Your points: ");
		mvwprintw(w, 4, 15, "%d", pts);
		mvwaddstr(w, 5, 1, "Press 'l' to add your score to the leaderboard.");

		int ch = wgetch(w);

		if (ch == 'l')
		{
			AddLeaderboardEntry(w, leaderboard, &numof_leaderboard, pts);
			wattron(w, COLOR_PAIR(MAIN_COLOR));
			PrintFrame(w);
			leaderboard = ReadLeaderboard(&numof_leaderboard);
			PrintLeaderboard(w, leaderboard, numof_leaderboard);
			mvwaddstr(w, 1, 1, "You won.");
			mvwaddstr(w, 4, 1, "Your points: ");
			mvwprintw(w, 4, 15, "%d", pts);
			mvwaddstr(w, 8, 1, "Press any key to quit...");
		}
		else
		{
			End(w);
		}
	}
	else
		mvwaddstr(w, 1, 1, "You lost.");

	wgetch(w); // wait here

	End(w);
}

// window start parameters and initialization
WINDOW* Start()
{
	WINDOW* win;
	if ((win = initscr()) == NULL) // initialize ncurses
	{
		fprintf(stderr, "Error initialising ncurses.\n");
		exit(EXIT_FAILURE);
	}

	keypad(win, TRUE); // make keypad work as input (for arrow)

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
	init_pair(STORK_COLOR, COLOR_RED, BACKGROUND_COLOR);
	init_pair(STORK_ROAD_COLOR, COLOR_RED, ROAD_BACKGROUND_COLOR);
	init_pair(STORK_OBSTACLE_COLOR, COLOR_RED, COLOR_CYAN);

	noecho(); // Switch off echoing, turn off cursor
	curs_set(0);
	return win;
}

// default screen
void CleanWin(window_t* W)
{
	wclear(W->window);
	wrefresh(W->window);

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
window_t* Init(WINDOW* parent, int y, int x, int colour, int delay)
{
	window_t* W = (window_t*)malloc(sizeof(window_t));
	W->x = x; W->y = y; W->lines = LINES; W->cols = COLS; W->colour = colour;
	W->window = subwin(parent, LINES, COLS, y, x);
	if (delay == DELAY_OFF) // non-blocking reading of characters (for real-time game)
		nodelay(W->window, TRUE);
	wrefresh(W->window);
	return W;
}

int GetBckgCol(int y, int x)
{
	chtype ch = mvinch(y, x);
	int color_pair = PAIR_NUMBER(ch & A_COLOR); // extract color pair from ch
	short foregrnd, bckgrnd;
	pair_content(color_pair, &foregrnd, &bckgrnd); // extract foreground and background color from color_pair
	return bckgrnd;
}

// check if at position (y,x) background colour matches background colour set for roads
int CheckRoad(int y, int x)
{
	if (GetBckgCol(y, x) == ROAD_BACKGROUND_COLOR)
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

// ---------------------------object_t and other structure-related functions:---------------------------

// shows object on the screen
void Print(object_t* object)
{
	for (int i = 0; i < object->height; i++)
		mvwprintw(object->win->window, object->y + i, object->x, "%s", object->appearance[i]);
}

// replaces object position with block of the same size consisting of c character
void PrintBlank(object_t* object, char c)
{
	for (int i = 0; i < object->height; i++)
		for (int j=0; j<object->width; j++)
			mvwprintw(object->win->window, object->y + i, object->x + j, "%c", c);
}

// change colour pair specified in object's parameters overriding current colour pair
void PrintColored(object_t* object)
{
	wattron(object->win->window, COLOR_PAIR(object->bckg_colour)); // ensures object is against proper background
	Print(object);
}

// shows road on the screen
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

// function changing position of the object and replacing it's previous position with character that originaly were there
void Movements(object_t* obj, int moveY, int moveX, char* trail)
{	
	if ((moveY > 0) && (obj->y + obj->height < LINES - BORDER)) // moving down
	{
		obj->y += moveY;
		for (int i = 1; i <= moveY; i++)
			mvwprintw(obj->win->window, obj->y - i, obj->x, "%s", trail);
	}
	if ((moveY < 0) && (obj->y > BORDER)) // moving up
	{
		obj->y += moveY;
		for (int i = 1; i <= abs(moveY); i++)
			mvwprintw(obj->win->window, obj->y + obj->height, obj->x, "%s", trail);
	}
	if ((moveX > 0) && (obj->x + obj->width < COLS - BORDER)) // moving right
	{
		obj->x += moveX;
		if (obj->x - moveX != 0) // so that frame isn't overwriten with object's blank space trail
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
	if ((moveX < 0) && (obj->x > BORDER)) // moving left
	{
		obj->x += moveX;
		if (obj->x - moveX != COLS - obj->width) // so that frame isn't overwriten with object's blank space trail
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

// check if point y, x is on the road line
int CheckRoadLine(int y, int x)
{
	if ((mvinch(y, x + 1) & A_CHARTEXT) == '-' || (mvinch(y, x - 1) & A_CHARTEXT) == '-') return 1;
	return 0;
}

// universal function to print any object_t to the screen and handle it's movements
void Show(object_t* object, int moveY, int moveX, int road_colour)
{
	// 'rebuilding' lane bounding road or printing empty character after object passes 
	char* trail = (char*)malloc((object->width + 1) * sizeof(char));
	if (CheckRoadLine(object->y, object->x))
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

// restore character after stork moves
void ClearStorkSpace(object_t* stork, int road_color)
{
	int backgrndcol = GetBckgCol(stork->y, stork->x);
	short obstacle_colour, obstacle_bckgrnd_col;
	pair_content(OBSTACLE_COLOR, &obstacle_colour, &obstacle_bckgrnd_col);
	if (backgrndcol == BACKGROUND_COLOR)
	{
		wattron(stork->win->window, COLOR_PAIR(MAIN_COLOR));
		PrintBlank(stork, ' ');
	}
	else if (backgrndcol == ROAD_BACKGROUND_COLOR)
	{
		wattron(stork->win->window, COLOR_PAIR(road_color));
		if (CheckRoadLine(stork->y, stork->x)) PrintBlank(stork, '-');
		else PrintBlank(stork, ' ');
	}
	else if (backgrndcol == obstacle_colour)
	{
		wattron(stork->win->window, COLOR_PAIR(OBSTACLE_COLOR));
		PrintBlank(stork, '7');
	}
}

void MoveStork(object_t* stork, int moveY, int moveX, int road_color)
{
	ClearStorkSpace(stork, road_color);
	stork->x += moveX;
	stork->y += moveY;
	// ensure proper background and store it in stork->bckg_color
	int backgroundcol = GetBckgCol(stork->y, stork->x);
	if ((mvinch(stork->y, stork->x) & A_CHARTEXT) == '7') stork->bckg_colour = STORK_OBSTACLE_COLOR; // first check if new stork position is above obstacle
	else if (backgroundcol == BACKGROUND_COLOR) stork->bckg_colour = STORK_COLOR;
	else if (backgroundcol == ROAD_BACKGROUND_COLOR) stork->bckg_colour = STORK_ROAD_COLOR;

	PrintColored(stork);
}

// frog initialisation; note: frog is of a constant size of 1x1 (single character)
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

// function to randomly choose car colour (in case of cars attributes are strictly related to colour)
int CarColour()
{
	switch RA(0, 3)
	{
	case 0: case 1: // greater chance for ordinary enemy car
		return CAR_COLOR1;
		break;
	case 2: // semi-enemy car that stops when near the frog
		return CAR_COLOR2;
		break;
	case 3: // friendly car - taxi that can give the frog a lift on demand
		return CAR_TAXI_COLOR;
		break;
	}
}

// initialise all car's parameters
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

// randomly assigns cars to a lane while maintaining proper separation between cars on the same lane
void AllocateCars(road_t* road, int car_lngth, char car_char)
{
	road->cars = (object_t**)malloc(sizeof(object_t*));

	int i, lane, last_lane = 0, posX, lastposX = BORDER + 1;

	for (i = 0; lastposX + car_lngth + MAX_CAR_SEPARATION < COLS - BORDER - car_lngth; i++)
	{
		road->cars = (object_t**)realloc(road->cars, (i+1)*sizeof(object_t*));
		lane = road->y + LanesToX(RA(0, XToLanes(road->width) - 1)); // randomly generate lane (y) for the car
		posX = lastposX + car_lngth + RA(MIN_CAR_SEPARATION, MAX_CAR_SEPARATION); // randomly generate separation from the last car (x)
		road->cars[i] = InitCar(road->win, lane, posX, road->speed, car_lngth, car_char); // assign generated parameters to the car
		last_lane = lane; lastposX = posX;
	}
	road->numof_cars = i;
}

// randomly generate speed for lane of cars while maintaining original predetermined direction of traffic
int CarSpeed(int car_speed) { return (car_speed/abs(car_speed))*RA(FRAME_TIME / abs(car_speed), FRAME_TIME / (abs(car_speed) / 2)); } // cs/abs(cs) indicated direction: 1 for right, -1 for left

// initialise all road parameters and call out function to assign cars to this road
road_t* InitRoad(window_t* w, int posY, int lanes, int col, int car_lngth, char car_char, int car_speed)
{
	road_t* road = (road_t*)malloc(sizeof(road_t));
	road->colour = col;
	road->win = w;
	road->y = posY;
	road->width = LanesToX(lanes);
	road->speed = CarSpeed(car_speed);
	road->stopped = (int*)malloc(lanes * sizeof(int));
	for (int i = 0; i < lanes; i++) road->stopped[i] = 0;
	
	AllocateCars(road, car_lngth, car_char);

	return road;
}

// initialise obstacle parameters
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

// initialise starting points parameters
point_t* InitPoints(int level_number)
{
	point_t* points = (point_t*)malloc(sizeof(point_t));
	points->last_y_progres_frame = 0;
	points->y_progres = LINES - BORDER;
	points->points_count = 0;
	points->level_multiplier = level_number;
	return points;
}

// initialise stork: 1x1 object depicted as %, STORK_SPEED_MULTIPLIER slower then the frog while continously moving in it's direction
object_t* InitStork(window_t* w, int road_color, int level_no)
{
	object_t* object = (object_t*)malloc(sizeof(object_t));
	object->win = w;
	object->y = 2;
	object->x = 1;
	object->bckg_colour = STORK_COLOR;
	object->width = 1;
	object->height = 1;
	object->speed = FROG_JUMP_TIME * (level_no == 3 ? 2 * STORK_SPEED_MULTIPLIER : STORK_SPEED_MULTIPLIER); // stork is 2 times slower on level 3
	object->interval = 0;

	object->appearance = (char**)malloc(sizeof(char*));
	object->appearance[0] = (char*)malloc(sizeof(char));

	strcpy(object->appearance[0], "%");

	Show(object, 0, 0, road_color);

	return object;
}

// formula to calculate points from how much time has passed between moves classified as progres (frog appearing to new y line, not revisiting a lane)
int CalculatePoints(int frame_delta)
{
	int points = (MAX_POINTS_TIME * FROG_JUMP_TIME) / (frame_delta);
	return (points > 0 ? points : 1); // minimum point for a move is 1
}

// function to check whether object 1 is within (deltaY, deltaX) of object 2 (doesn't work diagonally i think), for direct collisions deltaX,Y = 0
int Collision(object_t* ob1, object_t* ob2, int deltaY, int deltaX)
{
	if (((ob1->y + deltaY >= ob2->y && ob1->y + deltaY < ob2->y + ob2->height) || (ob2->y >= ob1->y + deltaY && ob2->y < ob1->y + ob1->height + deltaY)) &&
		((ob1->x + deltaX >= ob2->x && ob1->x + deltaX < ob2->x + ob2->width) || (ob2->x >= ob1->x + deltaX && ob2->x < ob1->x + ob1->width + deltaX))) return 1;
	return 0;
}

// check for collision with any of the obstacles and if there are none move the object; also calculates points
int MoveFrog(object_t** obstacles, int numof_obstacles, object_t* object, int moveY, int moveX, int road_color, point_t* points, int frame)
{
	for (int i = 0; i < numof_obstacles; i++)
	{
		if (Collision(object, obstacles[i], moveY, moveX)) return 1;
	}
	if (object->y + moveY < points->y_progres) // whether frog has already made progress up to this lane
	{
		points->points_count += (CalculatePoints(frame - points->last_y_progres_frame) * points->level_multiplier);
		points->last_y_progres_frame = frame;
		points->y_progres = object->y + moveY;
	}
	Show(object, moveY, moveX, road_color);

	return 0;
}

// if sufficient time has passed (minimal frog jump time) check input for direction the frog is supposed to move next
void FrogAction(object_t* object, int ch, unsigned int frame, object_t** obstacle, int numof_obstacles, int road_color, point_t* points)
{
	if (frame - object->interval >= object->speed)
	{
		object->interval = frame;
		switch (ch) {
		case KEY_UP: MoveFrog(obstacle, numof_obstacles, object, -1, 0, road_color, points, frame); break;
		case KEY_DOWN: MoveFrog(obstacle, numof_obstacles, object, 1, 0, road_color, points, frame); break;
		case KEY_LEFT: MoveFrog(obstacle, numof_obstacles, object, 0, -1, road_color, points, frame); break;
		case KEY_RIGHT: MoveFrog(obstacle, numof_obstacles, object, 0, 1, road_color, points, frame); break;
		}
	}
}

// if sufficient time has passed (minimal stork jump time) check what direction is frog and call proper function to move in that direction
void StorkAction(WINDOW* w, object_t* frog, object_t* stork, int frame, int road_color, leaderboard_t** leaderboard, int numof_leaderboard)
{
	if (frame - stork->interval >= stork->speed)
	{
		stork->interval = frame;
		if (stork->y == frog->y) // check if frog is straight up, down, left or right from the frog
		{
			if (stork->x > frog->x) MoveStork(stork, 0, -1, road_color); // move stork left
			else MoveStork(stork, 0, 1, road_color); // move stork right
		}
		else if (stork->x == frog->x)
		{
			if (stork->y > frog->y) MoveStork(stork, -1, 0, road_color); // move stork up
			else MoveStork(stork, 1, 0, road_color); // move stork up
		}
		else // if not, check the diagonals using trigonometry
		{
			int deltaX = frog->x - stork->x;
			int deltaY = frog->y - stork->y;
			int radi = sqrt(deltaX * deltaX + deltaY * deltaY);
			float sin = float(deltaY) / float(radi);
			float cos = float(deltaX) / float(radi);
			// checking directions:
			if (sin < 0 && cos > 0) MoveStork(stork, -1, 1, road_color); // move up and right
			else if (sin > 0 && cos > 0) MoveStork(stork, 1, 1, road_color); // move down and right
			else if (sin > 0 && cos < 0) MoveStork(stork, 1, -1, road_color); // move down and left
			else if (sin < 0 && cos < 0) MoveStork(stork, -1, -1, road_color); // move up and left
		}
		if (Collision(frog, stork, 0, 0)) EndScreen(w, 0, leaderboard, numof_leaderboard); // if player lost return 0 as points
	}
	else PrintColored(stork); // print stork object each time function is called so that stork is never covered by another asset
}

// car behaviour when it reaches the border; some cars may reappear with delay and changed attributes
void CarWrapping(object_t* object, int frame, int taxiing, int x_separation, int direction, int road_colour, leaderboard_t** leaderboard, int numof_leaderboard)
{
	wattron(object->win->window, COLOR_PAIR(road_colour));
	PrintBlank(object, ' '); // clear space after car
	if (direction > 0)
		object->x = BORDER - 1;
	else
		object->x = COLS - BORDER - object->width + 1;

	if (taxiing) EndScreen(object->win->window, 0, leaderboard, numof_leaderboard); // if player lost return 0 as points
	if (RA(1, CAR_CHANGE_CHANCE) % CAR_CHANGE_CHANCE == 0) // 1 in CAR_CHANGE_CHANCE chance of car changing it's attributes
	{
		object->rd_colour = CarColour(); // car behaviour is connected to it's colour so changing car colour changes car
		if (x_separation > MIN_CAR_SEPARATION)
			object->interval = frame + RA(1, x_separation - MIN_CAR_SEPARATION); // time interval between previous car disappearing and new one appearing
	}
	else // standard wrapping
	{
		object->interval = frame;
		Show(object, 0, direction, road_colour);
	}
}

// move car if sufficient time has passed since the last movement, check whether frog is traveling with the car and check for wrapping
void MoveCar(object_t* object, object_t* frog, int frame, int road_color, int taxiing, int x_separation, leaderboard_t** leaderboard, int numof_leaderboard)
{
	int moveX = object->speed / abs(object->speed); // taking the direction data from speed
	if (frame - object->interval >= abs(object->speed))
	{
		if ((moveX > 0 && object->x == COLS - BORDER - object->width) || (moveX < 0 && object->x == BORDER)) // if car reaches border (cases for right- and left-moving cars)
		{
			CarWrapping(object, frame, taxiing, x_separation, moveX, road_color, leaderboard, numof_leaderboard);
		}
		else
		{
			if (taxiing) Show(frog, 0, moveX, road_color); // shows frog above car it's traveling with
			Show(object, 0, moveX, road_color);
			object->interval = frame;
		}
	}
}

// find the separation behind car number i from array of numof_cars cars
int CarSeparation(object_t** cars, int numof_cars, int i, int road_y)
{
	int iLane = XToLanes(cars[i]->y - road_y);
	if (cars[i]->speed < 0) // left-moving cars
	{
		int j = (i < numof_cars - 1 ? i + 1 : 0);
		while (XToLanes(cars[j]->y - road_y) != iLane) // find nearest car on the same lane
		{
			j++;
			if (j > numof_cars - 1) j = 0;
			else if (j == i) return COLS - 2 * BORDER - cars[i]->width;
		}
		if (i < j) return cars[j]->x - cars[i]->x - cars[i]->width; // substracted width could have been of any car since it's defined in config file to be the same for each
		else return (cars[j]->x - BORDER + (COLS - BORDER) - cars[i]->x - cars[j]->width); // wrapping separation
	}
	else if (cars[i]->speed > 0) // right-moving cars
	{
		int j = (i > 0 ? i - 1 : numof_cars - 1);
		// find neares car behind on the same lane
		while (XToLanes(cars[j]->y - road_y) != iLane) // find nearest car on the same lane
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


// ---------------------------timer functions:---------------------------

void Sleep(unsigned int tui)
{
	clock_t start_time = clock();
	clock_t end_time = start_time + (clock_t)(tui * CLOCKS_PER_SEC / 1000);
	while (clock() < end_time) { /* while loop that doesn't end until specified time */ }
}

timer_t* InitTimer()
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

// initialise constant level-specific enviroment such as road and obstacle layout
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

	// printing roads and obstacles on the screen:
	for (int i = 0; i < *numof_roads; i++)
		PrintRoad(roads1[i]);

	for (int i = 0; i < *numof_obstacles; i++)
		Show(obstacles1[i], 0, 0, roads1[0]->colour); // colour is taken from 1st road since it always has to exist and colour is common for all roads (defined at the start and may be changed by the config file)
}

void Level2wo(road_t*** roads, object_t*** obstacles, int* numof_roads, int* numof_obstacles, window_t* w, int col, int car_length, char car_char, int car_speed) // medium level with some road narrowings might pose a challange to new players
{
	*numof_roads = 3;
	*numof_obstacles = 4;

	// initialise roads:

	road_t** roads1 = (road_t**)malloc(*numof_roads * sizeof(road_t*));

	roads1[0] = InitRoad(w, 23, SINGLE_LANE, col, car_length, car_char, -car_speed);
	roads1[1] = InitRoad(w, 15, DOUBLE_LANE, col, car_length, car_char, car_speed);
	roads1[2] = InitRoad(w, 3, TRIPLE_LANE, col, car_length, car_char, -car_speed);

	*roads = roads1;

	// initialise obstacles:

	object_t** obstacles1 = (object_t**)malloc(*numof_obstacles * sizeof(object_t*));

	obstacles1[0] = InitObstacle(w, 2, 40, OBSTACLE_COLOR, 60, 2);
	obstacles1[1] = InitObstacle(w, 26, 1, OBSTACLE_COLOR, 90, 1);
	obstacles1[2] = InitObstacle(w, 22, 70, OBSTACLE_COLOR, 49, 1);
	obstacles1[3] = InitObstacle(w, 13, 1, OBSTACLE_COLOR, 50, 2);

	*obstacles = obstacles1;

	// printing roads and obstacles on the screen:
	for (int i = 0; i < *numof_roads; i++)
		PrintRoad(roads1[i]);

	for (int i = 0; i < *numof_obstacles; i++)
		PrintColored(obstacles1[i]); // colour is taken from 1st road since it always has to exist and colour is common for all roads (defined at the start and may be changed by the config file)

}

object_t** Level3Obstacles(window_t* w, int numof_obstacles)
{
	object_t** obstacles = (object_t**)malloc(numof_obstacles * sizeof(object_t*));

	obstacles[0] = InitObstacle(w, 2, 40, OBSTACLE_COLOR, 78, 2);
	obstacles[1] = InitObstacle(w, 2, 2, OBSTACLE_COLOR, 35, 2);
	obstacles[2] = InitObstacle(w, 7, 1, OBSTACLE_COLOR, 44, 2);
	obstacles[3] = InitObstacle(w, 7, 61, OBSTACLE_COLOR, 58, 3);
	obstacles[4] = InitObstacle(w, 6, 45, OBSTACLE_COLOR, 1, 4);
	obstacles[5] = InitObstacle(w, 12, 45, OBSTACLE_COLOR, 1, 4);
	obstacles[6] = InitObstacle(w, 12, 90, OBSTACLE_COLOR, 1, 4);
	obstacles[7] = InitObstacle(w, 12, 45, OBSTACLE_COLOR, 45, 2);
	obstacles[8] = InitObstacle(w, 15, 47, OBSTACLE_COLOR, 40, 1);
	obstacles[9] = InitObstacle(w, 18, 20, OBSTACLE_COLOR, 1, 6);
	obstacles[10] = InitObstacle(w, 18, 100, OBSTACLE_COLOR, 1, 6);
	obstacles[11] = InitObstacle(w, 18, 21, OBSTACLE_COLOR, 80, 1);
	obstacles[12] = InitObstacle(w, 12, 1, OBSTACLE_COLOR, 40, 4);

	return obstacles;
}

void Level3hree(road_t*** roads, object_t*** obstacles, int* numof_roads, int* numof_obstacles, window_t* w, int col, int car_length, char car_char, int car_speed)
{
	*numof_roads = 4;
	*numof_obstacles = 13;

	// initialise roads:

	road_t** roads1 = (road_t**)malloc(*numof_roads * sizeof(road_t*));

	roads1[0] = InitRoad(w, 23, SINGLE_LANE, col, car_length, car_char, -car_speed);
	roads1[1] = InitRoad(w, 15, SINGLE_LANE, col, car_length, car_char, car_speed);
	roads1[2] = InitRoad(w, 9, SINGLE_LANE, col, car_length, car_char, -car_speed);
	roads1[3] = InitRoad(w, 3, SINGLE_LANE, col, car_length, car_char, car_speed);

	*roads = roads1;

	// initialise obstacles:
	object_t** obstacles1 = Level3Obstacles(w, *numof_obstacles);
	*obstacles = obstacles1;

	// printing roads and obstacles on the screen:
	for (int i = 0; i < *numof_roads; i++)
		PrintRoad(roads1[i]);

	for (int i = 0; i < *numof_obstacles; i++)
		PrintColored(obstacles1[i]); // colour is taken from 1st road since it always has to exist and colour is common for all roads (defined at the start and may be changed by the config file)
}

// prints information about levels to the screen
void LevelSelectionText(WINDOW* w)
{
	mvwaddstr(w, 1, 1, "Welcome to Jumping Frog: The Game");
	mvwaddstr(w, 2, 1, "Please select level you want to play.");
	mvwaddstr(w, 3, 1, "Points you get are multiplied by the level number you choose.");
	mvwaddstr(w, 5, 1, "Level 1ne: ");
	mvwaddstr(w, 6, 1, "Very basic level to familiarise player with game mechanics and objective.");
	mvwaddstr(w, 7, 1, "Neither cars, stork nor obstacles should pose a problem with going to the fininsh, more of a slight inconvinience.");
	mvwaddstr(w, 9, 1, "Level 2wo: ");
	mvwaddstr(w, 10, 1, "Slightly harder level.");
	mvwaddstr(w, 11, 1, "Baku GP - styled road narrowings could pose a challenge to unexperience players,");
	mvwaddstr(w, 12, 1, "but wide roads ease the traffic so cars should not be a problem.");
	mvwaddstr(w, 14, 1, "Level 3hree: ");
	mvwaddstr(w, 15, 1, "A true challenge for Jumping Frog enjoyers. Multiple harder and easier paths to victory.");
	mvwaddstr(w, 16, 1, "Navigate around narrow streets with walls on the sides just as in Monaco GP to find the way to score the most points.");
}

// ask player to select level and initialise it
int LevelSelection(road_t*** roads, object_t*** obstacles, int* numof_roads, int* numof_obstacles, window_t* w, int col, int car_length, char car_char, int car_speed)
{
	int level;
	wattron(w->window, COLOR_PAIR(MAIN_COLOR));
	PrintFrame(w->window);

	LevelSelectionText(w->window);

	while (level = wgetch(w->window))
	{
		if (level == '1') 
		{
			CleanWin(w);
			Level1ne(roads, obstacles, numof_roads, numof_obstacles, w, col, car_length, car_char, car_speed);
			break;
		}
		else if (level == '2')
		{
			CleanWin(w);
			Level2wo(roads, obstacles, numof_roads, numof_obstacles, w, col, car_length, car_char, car_speed);
			break;
		}
		else if (level == '3')
		{
			CleanWin(w);
			Level3hree(roads, obstacles, numof_roads, numof_obstacles, w, col, car_length, car_char, car_speed);
			break;
		}
	}

	return level - '0';
}

// ---------------------------main loop and related functions:---------------------------

// sub-function of CarsAction to do operations related to collisions
void CollisionAction(WINDOW* w, object_t* frog, road_t** roads, object_t** obstacles, point_t* points, int frame, int numof_obstacles, int ch, int* taxied, int* taxI, int* taxJ, int i, int j, leaderboard_t** leaderboard, int numof_leaderboard)
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
				MoveFrog(obstacles, numof_obstacles, frog, -1, 0, roads[0]->colour, points, frame); // moves frog one space up
				*taxied = 0;
			}
		}
	}
	else
		EndScreen(w, 0, leaderboard, numof_leaderboard); // if player lost return 0 as points
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

// performing all actions related to cars and their attributes
void CarsAction(WINDOW* w, object_t* frog, int frame_no, object_t** obstacles, road_t** roads, point_t* points, int ch, int* taxied, int numof_obstacles, int numof_roads, int car_speed, int* stopI, int* stopJ, int* taxI, int* taxJ, leaderboard_t** leaderboard, int numof_leaderboard)
{
	int currentLane; // variable to store lane information of current car
	for (int i = 0; i < numof_roads; i++)
	{
		for (int j = 0; j < roads[i]->numof_cars; j++)
		{
			currentLane = XToLanes(roads[i]->cars[j]->y - roads[i]->y);
			if (roads[i]->stopped[currentLane] == 0) // moves car if current lane isn't stopped
				MoveCar(roads[i]->cars[j], frog, frame_no, roads[0]->colour, (*taxied && *taxI == i && *taxJ == j ? *taxied : 0), CarSeparation(roads[i]->cars, roads[i]->numof_cars, j, roads[i]->y), leaderboard, numof_leaderboard);
			if (Collision(frog, roads[i]->cars[j], 0, 0))
			{
				CollisionAction(w, frog, roads, obstacles, points, frame_no, numof_obstacles, ch, taxied, taxI, taxJ, i, j, leaderboard, numof_leaderboard);
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

void MainLoop(window_t* status, object_t* frog, timer_t* timer, road_t** roads, int numof_roads, object_t** obstacles, int numof_obstacles, int car_speed, point_t* points, leaderboard_t** leaderboard, int numof_leaderboard, object_t* stork)
{
	int ch, pts = 0;
	int taxI, taxJ, taxied = 0; // taxi identification (i, j) indicating which vechicle is frog taxing with bool variable to check whether frog is currently traveling by taxi
	int stopI, stopJ; // variable to store lane information of current car
	while ((ch = wgetch(status->window)) != QUIT) // NON-BLOCKING! (nodelay=TRUE)
	{
		if (ch == ERR) ch = NOKEY; // ERR is ncurses predefined
		else if (!taxied)
		{
			FrogAction(frog, ch, timer->frame_no, obstacles, numof_obstacles, roads[0]->colour, points);
			if (frog->y == FINISH) EndScreen(status->window, points->points_count, leaderboard, numof_leaderboard);
		}
		// all car-related mechanics and operations:
		CarsAction(status->window, frog, timer->frame_no, obstacles, roads, points, ch, &taxied, numof_obstacles, numof_roads, car_speed, &stopI, &stopJ, &taxI, &taxJ, leaderboard, numof_leaderboard);

		StorkAction(status->window, frog, stork, timer->frame_no, roads[0]->colour, leaderboard, numof_leaderboard);

		ShowStatus(status, frog, points->points_count);
		flushinp(); // clear input buffer (avoiding multiple key pressed)
		UpdateTimer(timer, status);// update timer & sleep
	}
	EndScreen(status->window, 0, leaderboard, numof_leaderboard); // if player lost return 0 as points
}

// ---------------------------config file data reading:---------------------------
void ReadConfig(char* frog_appeal, int* car_length, char* car_char, int* car_speed, int* road_colour)
{
	FILE* config_file;
	config_file = fopen("config.txt", "r");

	// default values in case file read fails
	*frog_appeal = 'Q';
	*car_char = '#';
	*car_length = 3;
	*car_speed = 3;
	*road_colour = ROAD_EU_COLOR;

	if (config_file != NULL)
	{
		char road_col[4]; // string storing type of road coloring theme (EUR/USA - 3 character long therefore string is 4 character long making space for null terminator '\0')
		fscanf(config_file, "Size and shape of the frog: %c\n", frog_appeal);
		fscanf(config_file, "Car length (default 3): %d\n", car_length);
		fscanf(config_file, "Shape of a car (single character repeated as a block): %c\n", car_char);
		fscanf(config_file, "Cars speed multiplier (default 3, recommended between 2 and 5): %d\n", car_speed);
		fscanf(config_file, "Road theme (USA/EUR): %s\n", &road_col);
		if (road_col != NULL)
		{
			if (strcmp(road_col, "EUR") == 0) *road_colour = ROAD_EU_COLOR;
			else if (strcmp(road_col, "USA") == 0) *road_colour = ROAD_US_COLOR;
		}
		fclose(config_file);
	}	
}

int main()
{
	srand(time(NULL)); // new seed for each road definition
	int numof_roads, numof_obstacles, numof_leaderboard, level_no;

	leaderboard_t** leaderboard = ReadLeaderboard(&numof_leaderboard);

	WINDOW* mainwin = Start();

	Welcome(mainwin, leaderboard, numof_leaderboard);

	window_t* playwin = Init(mainwin, 0, 0, MAIN_COLOR, DELAY_OFF);

	timer_t* timer = InitTimer();

	object_t* frog = InitFrog(playwin, FROG_COLOR, FROG_ROAD_COLOR);

	// config file operations
	char car_char; int car_length, car_speed_multiplier, road_color; // variables to store config data
	ReadConfig(&frog->appearance[0][0], &car_length, &car_char, &car_speed_multiplier, &road_color);

	object_t** obstacles;
	road_t** roads;
	

	level_no = LevelSelection(&roads, &obstacles, &numof_roads, &numof_obstacles, playwin, road_color, car_length, car_char, car_speed_multiplier);
	
	point_t* points = InitPoints(level_no);
	object_t* stork = InitStork(playwin, road_color, level_no);

	ShowNewStatus(playwin, timer, frog, 0);
	Show(frog, 0, 0, roads[0]->colour);

	MainLoop(playwin, frog, timer, roads, numof_roads, obstacles, numof_obstacles, car_speed_multiplier, points, leaderboard, numof_leaderboard, stork);
}