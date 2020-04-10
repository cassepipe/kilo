/***includes***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/***defines***/

#define CTRL_KEY(k)	((k) & 0x1f)
#define KILO_VERSION	"0.0.1"
#define WELCOME_MSG	"Welcome to the yet to be amazing Kilo Text editor -- This is version " KILO_VERSION

enum editorKey {
	DELETE_KEY = 999,
	ARROW_UP = 1000,
	ARROW_DOWN,
	ARROW_RIGHT,
	ARROW_LEFT,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

/***data***/

struct erow {
	int size;
	char *chars;
};

struct editorConfig {
	int screenrows;
	int screencols;
	int rowoff;
	int coloff;
	int cx;
	int cy;
	int numrows;
	struct erow *row;
	struct termios orig_termios;
};


struct editorConfig E ;

/***terminal***/

void die(const char* s)
{ 
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	printf("\r\n");
	exit(1);
}

void disableRawMode()
{
	if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1 )
		die("tcsetattr in disableRawMode()");
}

void enableRawMode()
{
	if( tcgetattr(STDIN_FILENO, &E.orig_termios) == -1 ) 
		die("tcgetattr in enableRawMode()");

	atexit(disableRawMode);

	struct termios raw = E.orig_termios ;

	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK) ;
	raw.c_oflag &= ~(OPOST) ;
	raw.c_cflag |= (CS8);   
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0 ; raw.c_cc[VTIME] = 1 ;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
		die("tcsetattr in enableRawMode");
}

int editorReadKey()
{
	int nread ;
	char c ;

	while( (nread = read(STDIN_FILENO, &c, 1) ) != 1 )
	{
		if (nread == -1 && errno != EAGAIN)
			die("read() failed in editorReadKey()");
	}

	if (c == '\x1b')		//Is it an escape sequence ?
	{
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return '\x1b';

		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{ 
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				if (seq[2] == '~')
				{
					switch (seq[1])
					{
						case '1': return HOME_KEY;
						case '3': return DELETE_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			else
			{
				switch (seq[1])
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
			return '\x1b';
		}
		else if (seq[0] == 'O')
		{
			switch (seq[1])
			{
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
			return '\x1b';
		}
	} 
	else 
	{
		return c;
	}
	return c;
}

int getCursorPosition(int* rows, int* cols)
{
	char buf[16];

	if( write(STDOUT_FILENO,"\x1b[6n",4) != 4) return -1 ;  

	for(unsigned int i = 0; i < sizeof(buf) - 1; i++)
	{
		if ( read(STDIN_FILENO, &buf[i], 1) != 1) break ;
		if ( buf[i] == 'R') { buf[i+1] = '\0' ; break ; }
	}


	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}	

int getWindowSize(int* rows, int* cols)
{ 
	struct winsize ws ; // This struct is defined in <sys/iotcl.h>

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999B\x1b[999C", 12) != 12) return -1;
		return getCursorPosition(rows, cols) ;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;   
	}
}

/***row operations***/

void editorAppendRow(char *s, size_t len)
{
	E.row = realloc(E.row, sizeof(struct erow) * (E.numrows + 1));

	int at = E.numrows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';
	E.numrows++;
}

/***file i/o***/

void editorOpen(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) die("Failed to open file");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;

	while ((linelen = getline(&line, &linecap, fp)) != -1)  //linecap is passed by address because getline updates it.
	{
		while (linelen > 0 && (line[linelen - 1] == '\r' || line[linelen - 1] == '\n'))
			linelen--;
		editorAppendRow(line, linelen);
	}
	free(line);
	fclose(fp);
}

/***append buffer***/

struct abuf {
	char* b;
	int len;
};

void abAppend(struct abuf* ab_ptr, const char* s, int len) // where len is the lenght is of s
{
	if((ab_ptr->b = realloc(ab_ptr->b, ab_ptr->len + len)) == NULL )
		die("abAppend");
	memcpy(ab_ptr->b + ab_ptr->len, s, len);
	ab_ptr->len += len;	 //Update buffer lenght
}

/***input***/

void editorMoveCursor(int key)
{
	switch (key)
	{
		case ARROW_UP:
			if (E.cy) 
				E.cy--;
			if (E.rowoff && !E.cy)
				E.rowoff--;
			break;
		case ARROW_DOWN:
			if (E.cy < E.screenrows - 1)
				E.cy++;
			else if (E.rowoff + E.screenrows - 1 < E.numrows)   // Or if (E.rowoff == E.numrows - E.screecols && E.cy == E.screecols)
				E.rowoff++;
			break;
		case ARROW_RIGHT:
			if (E.cx < E.screencols - 1)
				E.cx++;
			break;
		case ARROW_LEFT:
			if (E.cx > 0)
				E.cx--;
			break;
		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols - 1;
			break;
	}
}

void editorProcessKeypress()
{
	int c = editorReadKey() ;

	switch (c) 
	{	
		case CTRL_KEY('c'):
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0) ;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_RIGHT:
		case ARROW_LEFT:
		case HOME_KEY:
		case END_KEY:
			editorMoveCursor(c);
			break;
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
				break;
			}
	}
}

/***output***/

void editorDrawRows(struct abuf *ab)
{
	int y;
	for (y = E.rowoff; y < E.screenrows + E.rowoff; y++) 
	{
		if (y < E.numrows)
		{
			abAppend(ab, E.row[y].chars[coloff], E.row[y].size < E.screencols ? E.row[y].size : E.screencols);
		}
		else
		{	
			abAppend(ab, "~", 1);
		}
		if (E.numrows == 0 && y == E.screenrows / 3)
		{
			char welcome[] = WELCOME_MSG;
			int padding = (E.screencols - sizeof(welcome)) / 2;
			padding--;
			while (padding--)
				abAppend(ab, " ", 1);
			abAppend(ab, welcome, sizeof(welcome));
		}
		//abAppend(ab, "\x1b[K", 3);		//Does not work very well 
		if (y < E.screenrows - 1 + E.rowoff)
			abAppend(ab, "\r\n", 2);
	}
}	


void editorRefreshScreen()
{
	struct abuf ab = {NULL, 0};

	abAppend(&ab, "\x1b[2J", 4);		//Wipe all lines to right of cursor (moves cursor)
	abAppend(&ab, "\x1b[H", 3);		//Put the cursor at origin

	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));

	write(STDOUT_FILENO, ab.b, ab.len);	//Write the buffer at once
	free(ab.b);
}

/***init***/

void initEditor() 
{
	E.cx = 0;
	E.cy = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row = NULL;

	if(getWindowSize(&E.screenrows, &E.screencols) == -1)
		die("getWindowSize");
}

int main(int argc, char **argv)
{
	enableRawMode();
	initEditor();
	if (argc == 2)
		editorOpen(argv[1]);

	while(1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}
}


