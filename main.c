#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
//this macro generates the ascii value of the control sequence eg-ctrl+a - 1 ctrl+m - 13 etc
typedef struct termios terminal;

struct _editorState
{
	unsigned short int screenrows;
	unsigned short int screencols;
	terminal original;
};
typedef struct _editorState editorState;
editorState config;



/**---terminal---**/
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


char readKey()
{
	int nread;
	char c;
 	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
    	if (nread == -1 && errno != EAGAIN) err("read");
    }
    return c;
}
/**----input----**/
void processKeyPress()
{
	char c = readKey();
	switch(c)
	{
		//present in ttydefaults already included
		case CTRL('q'):
			//clear screen then exit
			write(STDOUT_FILENO, "\x1b[2J", 4);
      		write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
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
		return getCursorPosition(rows, cols);
		
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}
void drawRows()
{
	for(int y = 0; y<config.screenrows; y++)
	{
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}
void refreshScreen()
{
	//0x1b-27
	//2 clears entire screen 0-till cursor 1-cursor till end
	write(STDOUT_FILENO, "\x1b[2J", 4);

	//Positioning cursor at beginning
	write(STDOUT_FILENO, "\x1b[H", 3);

	//then drawing
	drawRows();

	//reposition at beginning
	write(STDOUT_FILENO, "\x1b[H", 3);
}
/**init**/

//Responsible for initialising the editor's state structure
void initEditor() 
{
	if(getWindowSize(&config.screencols, &config.screenrows) == -1) err("getWindowSize");
}
int main()
{
	enableRawMode();
	initEditor();
	while(1)
	{
		refreshScreen();
		processKeyPress();
	}
}