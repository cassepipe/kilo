/***includes***/

#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/***defines***/

#define CTRL_KEY(k) ((k) & 0x1f)

/***data***/

//struct termios orig_termios ;

struct editorConfig {
	int screenrows;
	int screencols;
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
	if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1 ) die("tcsetattr in disableRawMode()");
}

void enableRawMode()

{
	if( tcgetattr(STDIN_FILENO, &E.orig_termios) == -1 ) die("tcgetattr in enableRawMode()") ;

	atexit(disableRawMode);

	struct termios raw = E.orig_termios ;

	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK) ;
	raw.c_oflag &= ~(OPOST)  ;
	raw.c_cflag |= (CS8);   
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0 ; raw.c_cc[VTIME] = 1 ;

	if ( tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr in enableRawMode") ;	
}

char editorReadKey()
{
	int nread ;
	char c ;

	while( (nread = read(STDIN_FILENO, &c, 1) ) != 1 )
	{
		if (nread == -1 && errno != EAGAIN) die("read() failed in editorReadKey()");
	}
	
	return c ;
}

int getCursorPosition(int* rows, int* cols)
{
	char buf[16];

	if( write(STDOUT_FILENO,"\x1b[6n",4) != 4) return -1 ;  

	for(int i = 0; i < sizeof(buf) -1; i++)
	{
		if( read(STDIN_FILENO, &buf[i], 1) != 1) break ;
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

/***append buffer***/

struct abuf {
	char* b;
	int len;
};

#define ABUF_INIT {NULL; 0}

void abAppend(struct abuf* ab_ptr, const char* s, int len) // where len is the lenght is of s
{
	if((ab_ptr->b = realloc(ab_ptr->b, ab_ptr->len + len)) == NULL ) die("abAppend");
	memcpy(ab_ptr->b + ab_ptr->len, s, len);
	ab_ptr->len += len; 
}
	
/***input***/

void editorProcessKeypress()
{
	char c = editorReadKey() ;

	switch (c) 
	{	case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0) ;
	}
}
/***output***/

void editorDrawRows()
{
	int y;
	for (y = 0; y < E.screenrows - 1; y++) {
	       write(STDOUT_FILENO, "~\r\n", 3);
	}
	       write(STDOUT_FILENO, "~", 1);
}	

void editorRefreshScreen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	editorDrawRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/***init***/

void initEditor() 
{
	if(
	getWindowSize(&E.screenrows, &E.screencols)
	== -1) die("getWindowSize");

}

int main()
{
	enableRawMode();
	initEditor();

	while(1)
	{
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0 ; // Will never be executed
}


