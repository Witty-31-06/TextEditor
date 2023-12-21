#include <unistd.h>
#include <termios.h>

typedef struct termios terminal;
void enableRawMode()
{
    terminal raw;

    //STDIN_FILENO - fd for input
    tcgetattr(STDIN_FILENO, &raw); //Storing terminal settings in struct raw
    raw.c_lflag &= ~(ECHO); //Turns off echoing
}
int main()
{
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1);
  return 0;
}