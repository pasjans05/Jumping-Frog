#include <curses.h>

#define LINES 25
#define COLS 110

// colours:
#define MAIN_COLOR 1
#define BACKGROUND_COLOR COLOR_WHITE

int main()
{
	WINDOW* win = newwin(LINES, COLS, 0, 0);
	win = initscr();

	// colours:
	start_color();
	init_pair(MAIN_COLOR, COLOR_BLUE, BACKGROUND_COLOR);
	wattron(win, COLOR_PAIR(MAIN_COLOR));

	// frame:
	for (int i = 0; i <= LINES; i++)
	{
		for (int j = 0; j <= COLS; j++)
		{
			if (i == 0 && j == 0) mvwaddch(win, i, j, 201);
			else if (i == 0 && j == COLS) mvwaddch(win, i, j, 187);
			else if (i == LINES && j == 0) mvwaddch(win, i, j, 200);
			else if (i == LINES && j == COLS) mvwaddch(win, i, j, 188);
			else if (j == 0 || j == COLS) mvwaddch(win, i, j, 186);
			else if (i == 0 || i == LINES) mvwaddch(win, i, j, 205);
			else mvwaddch(win, i, j, ' ');
		}
	}
	
	wrefresh(win);
	wgetch(win);
	wclear(win);

	return 0;
}