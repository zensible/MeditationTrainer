#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>

#include "key.h"

//-------------------------------------------------------------------------

static int stdin_fd = -1;
static struct termios original;

//-------------------------------------------------------------------------

bool keyPressed(int *character)
{
    // If this is the first time the function is called, change the stdin
    // stream so that we get each character when the keys are pressed and
    // and so that character aren't echoed to the screen when the keys are
    // pressed.
    if (stdin_fd == -1)
    {
        // Get the file descriptor associated with stdin stream.
        stdin_fd = fileno(stdin);

        // Get the terminal (termios) attritubets for stdin so we can
        // modify them and reset them before exiting the program.
        tcgetattr(stdin_fd, &original);

        // Copy the termios attributes so we can modify them.
        struct termios term;
        memcpy(&term, &original, sizeof(term));

        // Unset ICANON and ECHO for stdin. When ICANON is not set the
        // input is in noncanonical mode. In noncanonical mode input is
        // available as each key is pressed. In canonical mode input is
        // only available after the enter key is pressed. Unset ECHO so that
        // the characters aren't echoed to the screen when keys are pressed.
        // See the termios(3) man page for more information.
        term.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(stdin_fd, TCSANOW, &term);

        // Turn off buffering for stdin. We want to get the characters
        // immediately. We don't want the characters to be buffered.
        setbuf(stdin, NULL);
    }

    // Get the number of characters that are waiting to be read.
    int characters_buffered = 0;
    ioctl(stdin_fd, FIONREAD, &characters_buffered);

    // Set the return value to true if there are any characters to be read.
    bool pressed = (characters_buffered != 0);

    if (characters_buffered == 1)
    {
        // There is only one character to be read. Read it in.
        int c = fgetc(stdin);

        // Check if the caller wants the value of character read.
        if (character != NULL)
        {
            *character = c;
        }
    }
    else if (characters_buffered > 1)
    {
        // There is more than one character to be read. This can be key such
        // as the arrow keys or function keys. This code just reads them in
        // and ignores them. The caller will be informed that a key was
        // pressed, but won't get a value for the key.
        while (characters_buffered)
        {
            fgetc(stdin);
            --characters_buffered;
        }
    }

    return pressed;
}

//-------------------------------------------------------------------------

void keyboardReset(void)
{
    if (stdin_fd != -1)
    {
        // If keyPressed() has been called the terminal input has been
        // changed for the stdin stream. Put the attributes back the way
        // we found them.
        tcsetattr(stdin_fd, TCSANOW, &original);
    }
}