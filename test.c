#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#define VER "1.0"
//this macro generates the ascii value of the control sequence eg-ctrl+a - 1 ctrl+m - 13 etc
typedef struct termios terminal;

struct _editorState
{
	int cx, cy;
	unsigned short int screenrows;
	unsigned short int screencols;
	terminal original;
};
typedef struct _editorState editorState;
editorState config;


typedef struct {
	char *s;
	int len;
} abuf;
#define ABUF_INIT {NULL, 0}
/**---terminal---**/

typedef enum {
	ARROW_LEFT = 1000,
	ARROW_RIGHT = 1001,
	ARROW_UP = 1002,
	ARROW_DOWN = 1003,
	PAGE_UP = 1004,
	PAGE_DOWN = 1005,
} splKeys;
void err(const char *s)
{

	//Clear screen if error occurs then exit
	write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
	
	perror(s);
	printf("\r\n");
	exit(1);
}

void exitRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.original) == -1)
	{
		err("tcsetattr failure");
	}	
}

void enableRawMode()
{
	terminal raw;
    if(tcgetattr(STDIN_FILENO, &config.original) == -1) err("tcgetattr failure"); //Storing terminal settings in struct raw
    atexit(exitRawMode);
    raw = config.original;


    //Flags Used below
    //ECHO - Turns off echoing
    //ICANON - Turns off canonical mode (line by line input) No need to press enter
	//ISIG - Stops SIGSTP SIGINT signals
	//IXON - Stops XOFF XON signals
	//ICRNL - ctrl+m should be /r but terminal translates it to /n
	//OPOST - stops inserting /r by default
	raw.c_iflag &= ~(ICRNL|IXON|BRKINT);
    raw.c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN);
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 10;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

}

/**Append Buffer**/

void abAppend(abuf *ab, const char *s, int len)
{
	char *new = realloc(ab->s, ab->len+len); // allocating space for increasing size of string
	if(new == NULL) err("Buffer Allocation problems");
	//Can't use string function because they add 0 at the end
	memcpy(new + ab->len, s, len); //Copy string s at the end
	ab->s = new;
	ab->len += len;

}

void abFree(abuf *ab)
{
	free(ab->s);
}
int readKey()
{
	int nread;
	char c;
 	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
    	if (nread == -1 && errno != EAGAIN) err("read");
    }
	if(c == '\x1b')
	{
		char seq[3]; //0->[ 1->
		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1] , 1) != 1) return '\x1b';
		if(seq[0] == '[') 
		{
			
			switch(seq[1])
			{
        		case 'A': return ARROW_UP;
        		case 'B': return ARROW_DOWN;
        		case 'C': return ARROW_RIGHT;
        		case 'D': return ARROW_LEFT;
			}
		} 
		return '\x1b';
	}
	else 
	{
    	return (int)c;
	}
}
/**----input----**/

void moveCursor(int key)
{
	switch(key)
	{
		case ARROW_LEFT:
			if(config.cx != 0) config.cx--;
			break;
		case ARROW_UP:
			if(config.cy != 0) config.cy--;
			break;
		case ARROW_DOWN:
			if(config.cy != config.screenrows-1) config.cy++;
			break;
		case ARROW_RIGHT:
			if(config.cx != config.screencols) config.cx++;
			break;
	}
}
void processKeyPress()
{
	int c = readKey();
	char k[1];
	k[0] = (char)c;
	switch(c)
	{
		//present in ttydefaults already included
		case CTRL('q'):
			//clear screen then exit
			write(STDOUT_FILENO, "\x1b[2J", 4);
      		write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			moveCursor(c);
			break;
		default:
			write(STDOUT_FILENO, k, 1);
			break;
	}
}

/**----output----**/

/*

*/
int getCursorPosition(unsigned short int *row, unsigned short int *col)
{
	*row = *col = 0;

	//ESC [ Ps n Ps = 6 means Command from host – Please report active position (using a CPR control sequence)
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	char parse_buf[32] = {0};
	//Now the terminal will return an escape sequence of form 27[r;cR r = row, c = col. we have to parse it
	short int i = 0;
	while(i<(short int)(sizeof(parse_buf)-1))
	{
		if(read(STDIN_FILENO, &parse_buf[i], 1) != 1) break;
		if (parse_buf[i] == 'R') break;
    	i++;
	}

	//No idea why this loop is insanely slow
	// while(parse_buf[i] != 'R')
	// {
	// 	if(read(STDIN_FILENO, &parse_buf[i], 1) != 1) break;
	// 	i++;
	// }
	// printf("\r\n&buf[i]: %s\r\n", &parse_buf[1]);
	if(parse_buf[0] != '\x1b' || parse_buf[1] != '[')
	{
		return -1;
	}
	if(sscanf(&parse_buf[2], "%hu;%hu", row, col) != 2)
	{
		return -1; 
	}
	return 0;
}
int getWindowSize(unsigned short int *rows, unsigned short int *cols)
{
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) 
	{
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
		{
			return -1;	
		}
		int k = getCursorPosition(rows, cols);
		// printf("cols %d, rows %d\r\n", *cols, *rows);
		err("WTF");
		return k;
		
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		// printf("cols %d, rows %d\r\n", *cols, *rows);
		return 0;
	}
	// *rows = 24;
	// *cols = 35;
	// return 0;

	
}
void drawRows(abuf *ab)
{
	for(int y = 0; y<config.screenrows; y++)
	{
		if(y  == 0)
		{
			char welcomemsg[80] = {0};
			int msglen = snprintf(welcomemsg, sizeof(welcomemsg), "TextEditor Version %s", VER);
			if(msglen > config.screencols) msglen = config.screencols;
			abAppend(ab, welcomemsg, msglen);

		}
		else
		{
		abAppend(ab, "~", 1);
		}
		abAppend(ab, "\x1b[K", 3); //For clearing one line at a time
		if(y < config.screenrows-1)
		{
			abAppend(ab, "\r\n", 2);
		}
		
	}
}
void refreshScreen()
{
	abuf ab = ABUF_INIT;
	//0x1b-27

	// //2 clears entire screen 0-till cursor 1-cursor till end
	// abAppend(&ab, "\x1b[2J", 4);

	//Positioning cursor at beginning
	abAppend(&ab,  "\x1b[H", 3);

	//then drawing
	drawRows(&ab);
	char buf[32];
	int length = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", config.cy+1, config.cx + 1);
	if(length  == 0) err("Cursor error");
	abAppend(&ab, buf, strlen(buf));


	write(STDOUT_FILENO, ab.s, ab.len);
	// printf("\r\n%d %d", config.cx, config.cy);
	abFree(&ab);
}
/**init**/

//Responsible for initialising the editor's state structure
void initEditor() 
{
	config.cx = config.cy = 0;
	if(getWindowSize(&config.screenrows, &config.screencols) == -1) err("getWindowSize");
}
int main()
{
	enableRawMode();
	initEditor();
	// printf("%d %d\r\n", config.screencols, config.screenrows);
    char c; 
    while (c != 'q') {
        read(STDIN_FILENO, &c, 1);
        if (iscntrl(c)) {
        printf("%d\r\n", c);
        } else {
        printf("%d ('%c')\r\n", c, c);
        }
  }
  return 0;
}