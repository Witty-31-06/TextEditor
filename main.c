#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define VER "1.0"
#define TABSIZE 4

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdarg.h>

typedef struct termios terminal;

//This struct contains row of text
typedef struct erow
{
    int size;
    char *chars;

    int rsize;
    char *render;
} erow;

/*This struct contains the buffer which we have to output in terminal*/
typedef struct abuf{
    char *s;
    int len;
} abuf;
#define ABUF_INIT {NULL, 0}
/**---terminal---**/

typedef enum keys{
    ARROW_LEFT = 1000,
    ARROW_RIGHT = 1001,
    ARROW_UP = 1002,
    ARROW_DOWN = 1003,
    PAGE_UP = 1004,
    PAGE_DOWN = 1005,
    HOME = 1006,
    END = 1007,
    DEL = 1008,
} splKeys;


typedef struct estate
{
    int cx, cy;
    int rx;
    unsigned short int screenrows;
    unsigned short int screencols;
    int numrow;
    int colOff;
    int rowOff; //for scrolling
    erow *row; //Stores a row of text from the file
    char *filename;
    char statusMsg[80];
    time_t statusMsgTime;
    terminal original;
} editorState;
editorState config;


/**
 * @brief Outputs the error in screen and exits
 *
 * @param s
 */
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


/**
 * @brief Enables raw mode of the terminal
 *
 */
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

/**row operations**/


int CxToRx(erow *row, int cx)
{
    int rx = 0;
    int j;
    for(j = 0; j<cx; j++)
    {
        if(row->chars[j] =='\t')
        {
            rx += (TABSIZE-1) - (rx%TABSIZE);
        }
        rx++;
    }
    return rx;
}
void updateRow(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
    {
      if (row->chars[j] == '\t') tabs++;
    }
    free(row->render);
    row->render = malloc(row->size + tabs*(TABSIZE-1) + 1); 
    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            int t = 1;
            row->render[idx++] = ' ';
            while (t % TABSIZE != 0)
            {
                row->render[idx++] = ' ';
                t++;
            }
        } 
        else 
        {
            row->render[idx++] = row->chars[j];
        
        }
    }
  row->render[idx] = '\0';
  row->rsize = idx;
}
void appendRow(char *s, size_t len)
{
    //Adding one extra space for new row
    config.row = realloc(config.row, (sizeof(erow)) * (config.numrow+1));

    //Inserting the row info at the specific index
    int ind = config.numrow;
    config.row[ind].size = len;
    config.row[ind].chars = malloc(len+1);
    strncpy((config.row[ind].chars), s, len);


    config.row[ind].rsize = 0;
    config.row[ind].render = NULL;
    updateRow(&config.row[ind]);
    config.numrow++;

}
/**FILE I/O**/

/**
 * @brief This function opens the file
 *
 * @param filename
 */
void editorOpen(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if(!fp) err("fopen");
    config.filename = strdup(filename);
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) //read till EOF
    {
        while(linelen > 0 && (line[linelen-1] == '\r' || line[linelen-1] == '\n')) //trim /r/n
        {
            linelen--;
        }
        appendRow(line, linelen); //Insert the line in row struct
    }

    free(line);
    fclose(fp);
}
/**Append Buffer**/

/**
 * @brief This function appends the string to the buffer
 *
 * @param ab
 * @param s
 * @param len
 */
void abAppend(abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->s, ab->len+len); // allocating space for increasing size of string
    if(new == NULL) err("Buffer Allocation problems");
    //Can't use string function because they add 0 at the end
    memcpy(new + ab->len, s, len); //Copy string s at the end
    ab->s = new;
    ab->len += len;

}

/**
 * @brief This function frees the buffer
 *
 * @param ab
 */
void abFree(abuf *ab)
{
    free(ab->s);
}

/**----input----**/

/**
 * @brief This function reads the keypress
 *
 * @return int
 */
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
        //If escape sequence is detected then read two more bytes
        //Page down and up 4 bytes. esc[5~ esc[6~
        //Arrow keys 3 bytes esc[A esc[B esc[C esc[D
        char seq[3];
        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1] , 1) != 1) return '\x1b';
        if(seq[0] == '[')
        {
            if(seq[1] >= '0' && seq[1] <= '9')
            {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                //If third byte not there return esc
                if(seq[2] == '~') //Page up and down ends etc etc with ~
                {
                    switch(seq[1])
                    {
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '1':
                        case '7': return HOME;
                        case '4':
                        case '8': return END;
                        case '3': return DEL;
                    }
                }

            }
            else
            {
                switch(seq[1])
                {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME;
                    case 'F': return END;
                }
            }
        }
        return '\x1b';
    }
    else
    {
        return (int)c;
    }
}


/**
 * @brief This function moves the cursor
 *
 * @param key
 */
void moveCursor(int key)
{
    erow *row;
    if(config.cy >= config.numrow)
    {
        row = NULL; //If cursor moves past end of file, row is null
    }
    else
    {
        row = &config.row[config.cy]; //else row points to the current line in the file
    }
    switch(key)
    {
        case ARROW_LEFT:
            if(config.cx != 0) config.cx--;
            else if(config.cy > 0)
            {
                //moving cursor to end of prev line
                config.cy--;
                config.cx = config.row[config.cy].size;
            }
            break;
        case ARROW_UP:
            if(config.cy != 0) config.cy--;
            break;
        case ARROW_DOWN:
            if(config.cy < config.numrow) config.cy++;
            break;
        case ARROW_RIGHT:
            if(row && config.cx < row->size)
                config.cx++; //increment cx only till length of the current row
            else if (row && config.cx == row->size)
            {
                config.cy++;
                config.cx = 0;
            }

            break;
    }

    //Since after moving from a long line to
    //a shorter line cx remains same as that of position in longer line we need to
    //modify cx
    row = (config.cy>=config.numrow) ? NULL:&config.row[config.cy];
    int rowlen = row ? row->size: 0;
    if(config.cx > rowlen) config.cx = rowlen;
}

/**
 * @brief This function processes the keypress
 *
 */
void processKeyPress()
{
    int c = readKey();
    switch(c)
    {
        //present in ttydefaults already included
        case CTRL('q'):
            //clear screen then exit
            write(STDOUT_FILENO, "\x1b[2J", 4);
              write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
        case HOME:
            config.cx = 0;
            break;
        case END:
            if(config.cy<config.numrow) config.cx = config.row[config.cy].size-1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
        {
            if (c == PAGE_UP) {
            config.cy = config.rowOff;
            } else if (c == PAGE_DOWN) {
            config.cy = config.rowOff + config.screenrows - 1;
            if (config.cy > config.numrow) config.cy = config.numrow;
            }
            int times = config.screenrows;
            while (times--)
            moveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            moveCursor(c);
            break;
        default:
            break;
    }
}

/**
 * @brief This function gets the cursor position
 *
 * @param row
 * @param col
 * @return int
 * */
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

/**----output----**/


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
        err("WTF");
        return k;

    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }


}


void scroll()
{
    config.rx = 0;
    if(config.cy<config.numrow)
    {
        config.rx = CxToRx(&config.row[config.cy], config.cx);
    }
    if(config.cy<config.rowOff)
    {
        
        config.rowOff = config.cy;
    }
    if(config.cy >= config.rowOff + config.screenrows)
    {
        config.rowOff = config.cy-config.screenrows + 1;
    }
    if(config.rx < config.colOff)
    {
        config.colOff = config.rx;
    }
    if(config.rx >= config.colOff + config.screencols)
    {
        config.colOff = config.rx-config.screencols + 1;
    }

}
/**
 * @brief This function draws the rows of the editor
 *
 * @param ab
 */
void drawRows(abuf *ab)
{
    for(int y = 0; y<config.screenrows; y++)
    {
        int filerow = y+config.rowOff;
        if(filerow >= config.numrow)
        {
            if(y>=config.numrow)
            {
                if(config.numrow == 0 && y == config.screenrows/3)
                {
                    char welcomemsg[80] = {0};
                    int msglen = snprintf(welcomemsg, sizeof(welcomemsg), "TextEditor Version %s", VER);
                    if(msglen > config.screencols) msglen = config.screencols;
                    int padding = (config.screencols - msglen) / 2;
                    if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                    }
                    while (padding--) abAppend(ab, " ", 1);
                    abAppend(ab, welcomemsg, msglen);

                }
                else
                {
                    abAppend(ab, "~", 1);
                }
            }
        }
        else
        {
            int len = config.row[filerow].rsize - config.colOff;
            if(len<0) len = 0;
            if(len > config.screencols) len = config.screencols;
            abAppend(ab, &config.row[filerow].render[config.colOff], len);
        }
        abAppend(ab, "\x1b[K", 3); //For clearing one line at a time
        // if(y < config.screenrows-1)
        // {
        abAppend(ab, "\r\n", 2);
        // }

    }
}

void drawStatusBar(abuf *ab)
{
    abAppend(ab, "\x1b[7m", 4);
    char status[100];
    char lno[30]; //Shows line number

    int len = snprintf(status, sizeof(status), "%.20s - %d lines",
        config.filename ? config.filename : "[Untitled]", config.numrow);
    int rlen = snprintf(lno, sizeof(lno), "Ln %d, Col %d", config.cy+1, config.cx + 1);
    if(len > config.screencols) len = config.screencols;
    abAppend(ab, status , len);
    while(len<config.screencols)
    {
        if(config.screencols - len == rlen)
        {
            abAppend(ab, lno, rlen);
            break;
        }
        else
        {
            abAppend(ab, " ", 1);
            len++;
        }

    }
    abAppend(ab, "\x1b[0m", 4);
    abAppend(ab, "\r\n", 2);

}

void drawMsgBar(abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\x1b[7m", 4);
    int msglen = strlen(config.statusMsg);
    if(msglen > config.screencols) msglen = config.screencols;
    if(time(NULL) - config.statusMsgTime < 3)
    {
        if(msglen != 0)
        {
            abAppend(ab, "\x1b[7m", 4);
            abAppend(ab, config.statusMsg, msglen);
        }
        for(int i = 0; i<config.screencols - msglen; i++)
        {
            abAppend(ab, " ", 1);
        }
    }
    else
    {
        for(int i = 0; i<config.screencols; i++)
        {
            abAppend(ab, " ", 1);
        }        
    }

    
    abAppend(ab, "\x1b[m", 3);
}
/**
 * @brief This function refreshes the screen after every keypress
 *
 */
void refreshScreen()
{
    scroll();
    abuf ab = ABUF_INIT;
    //0x1b-27

    // //2 clears entire screen 0-till cursor 1-cursor till end
    // abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[?25l", 6);
    //Positioning cursor at beginning
    abAppend(&ab,  "\x1b[H", 3);

    //then drawing
    drawRows(&ab);
    drawStatusBar(&ab);
    drawMsgBar(&ab);


    char buf[32];
    int length = snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
                    (config.cy - config.rowOff+1), (config.rx - config.colOff + 1));
    if(length  == 0) err("Cursor error");
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    //Writes all the buffer at once
    write(STDOUT_FILENO, ab.s, ab.len);
    abFree(&ab);
}
void setStatusMsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(config.statusMsg, sizeof(config.statusMsg), fmt, ap);
    va_end(ap);
    config.statusMsgTime = time(NULL);
}

/**init**/

/**
 * @brief This function initializes the editor
 *
 */
void initEditor()
{
    config.cx = config.cy = 0;
    config.numrow = 0;
    config.row = NULL;
    config.rowOff = config.colOff = 0;
    config.rx = 0;
    config.filename = NULL;
    config.statusMsg[0] = 0;
    config.statusMsgTime = 0;
    if(getWindowSize(&config.screenrows, &config.screencols) == -1) err("getWindowSize");
    config.screenrows -= 2; //Making two empty space at bottom of the screen
}
int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if(argc >= 2)
    {
        editorOpen(argv[1]);
    }
    setStatusMsg("HELP: Ctrl-Q = quit");
    while(1)
    {
        refreshScreen();
        processKeyPress();
    }
}