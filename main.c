#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

typedef struct termios terminal;
terminal original;

void err(const char *s)
{
	perror(s);
	exit(1);
}

void exitRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original) == -1)
	{
		err("tcsetattr failure");
	}	
}

void enableRawMode()
{
	terminal raw;
    if(tcgetattr(STDIN_FILENO, &original) == -1) err("tcgetattr failure"); //Storing terminal settings in struct raw
    atexit(exitRawMode);
    raw = original;


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
	raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

}


int main()
{
	enableRawMode();
	char c;
	while (1)
	{
		c = 0;
		//EAGAIN - 
		if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) err("read failure");
		if(iscntrl(c))
	{
			printf("%d\r\n",c);
	}
		else 
		{
			  printf("%d ('%c')\r\n", c, c);
		}
		if(c == 'q') break;
  	}
  	return 0;
}