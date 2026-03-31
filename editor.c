/* Simple terminal text editor using ncurses
 * Controls: Ctrl+O = save, Ctrl+X = quit, Ctrl+F = view notes
 *
 * Compile: gcc -o editor editor.c -lncursesw
 */

#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <locale.h>  /* FIX #1: Added include for setlocale() – without it, UTF-8 characters
                        are not handled correctly even when _XOPEN_SOURCE_EXTENDED is defined */

#define MAX_TEXT 10000
#define STATUS_ATTR A_REVERSE

/* Explicit ASCII codes for control keys (Ctrl+letter = letter - 64) */
#define CTRL_O 15
#define CTRL_X 24
#define CTRL_F 6

/* Counts the number of words in text (words are separated by whitespace).
 * Returns 0 for NULL or empty input. */
int count_words(const char *text) {
    if (!text || !*text) return 0;

    int count = 0;
    int in_word = 0;

    for (int i = 0; text[i]; i++) {
        if (isspace((unsigned char)text[i])) {  /* FIX #2: Added cast to unsigned char –
                                                    isspace() has undefined behavior for negative
                                                    char values (common on platforms with signed char,
                                                    e.g. characters with diacritics); the cast fixes this */
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
    }
    return count;
}

/* Appends a note to ~/notes.txt in the following format:
 *   [date] subject
 *   text
 *   --------------------------------
 */
void save_notes(const char *date, const char *subject, const char *text) {
    char *home = getenv("HOME");
    if (!home) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/notes.txt", home);

    FILE *f = fopen(path, "a");
    if (!f) return;

    fprintf(f, "[%s] %s\n", date, subject);
    fprintf(f, "%s\n", text);
    fprintf(f, "--------------------------------\n");
    fclose(f);
}

int main() {
    char date[128]      = "";
    char subject[128]   = "";
    char text[MAX_TEXT] = "";
    int  len     = 0;  /* current length of text content */
    int  pos     = 0;  /* cursor position within text[] */
    int  ch;
    int  running = 1;

    /* FIX #3: Set locale for proper UTF-8 support.
       Without this call ncurses won't handle multi-byte characters correctly
       even with #define _XOPEN_SOURCE_EXTENDED. Must be called before initscr(). */
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    initscr();
    cbreak();              /* read input character by character, without waiting for Enter */
    noecho();              /* do not automatically echo pressed keys to the screen */
    keypad(stdscr, TRUE);  /* enable special keys (arrow keys, function keys, etc.) */
    curs_set(1);           /* show the cursor */

    /* Retro color scheme: green text on black background */
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        bkgd(COLOR_PAIR(1));
    }

    /* --- Input phase: read date and subject from the user --- */
    echo();  /* temporarily enable echo so the user can see what they type */

    attron(A_BOLD);
    mvprintw(0, 0, "Date   : ");
    attroff(A_BOLD);
    refresh();
    getnstr(date, sizeof(date) - 1);

    attron(A_BOLD);
    mvprintw(1, 0, "Subject: ");
    attroff(A_BOLD);
    refresh();
    getnstr(subject, sizeof(subject) - 1);

    noecho();  /* disable echo for the main editing loop */
    clear();
    refresh();

    /* --- Main editing loop --- */
    while (running) {
        erase();  /* clear the screen (deferred – no immediate repaint) */

        /* Header: date and subject in bold */
        attron(A_BOLD);
        mvprintw(0, 0, "[%s] %s", date, subject);
        attroff(A_BOLD);

        /* Render text content starting from line 1 */
        int y = 1, x = 0;
        for (int i = 0; i < len; i++) {
            if (y >= LINES - 1) break;  /* last line is reserved for the status bar */

            if (text[i] == '\n') {
                y++;
                x = 0;
            } else {
                if (x < COLS) {
                    mvaddch(y, x, (unsigned char)text[i]);  /* FIX #4: cast to unsigned char –
                                                               addch() expects chtype; passing a
                                                               negative char value would corrupt
                                                               the character with ncurses attribute bits */
                }
                x++;
                if (x >= COLS) {  /* soft word-wrap at terminal edge */
                    x = 0;
                    y++;
                }
            }
        }

        /* Calculate the visual cursor position (screen row and column) */
        int cursor_y = 1, cursor_x = 0;
        for (int i = 0; i < pos; i++) {
            if (text[i] == '\n') {
                cursor_y++;
                cursor_x = 0;
            } else {
                cursor_x++;
                if (cursor_x >= COLS) {
                    cursor_x = 0;
                    cursor_y++;
                }
            }
        }

        /* --- Status bar (last line, rendered with reverse video) --- */
        attron(STATUS_ATTR);

        /* Word count on the left */
        int word_count = count_words(text);
        mvprintw(LINES - 1, 0, "Words:%d", word_count);

        /* Current line number in the centre */
        int line_num = 1;
        for (int i = 0; i < pos; i++) {
            if (text[i] == '\n') line_num++;
        }
        mvprintw(LINES - 1, COLS / 2 - 5, "Line:%d", line_num);

        /* FIX #5: Shortened the shortcut hint string to fit within 20 characters.
           The original string "^O Uloz ^X Konec ^F Otevri" is 27 characters long;
           placing it at COLS-20 caused it to overflow past the right edge of the screen. */
        mvprintw(LINES - 1, COLS - 20, "^O Save ^X Quit");

        clrtoeol();   /* clear the rest of the status bar line */
        attroff(STATUS_ATTR);

        /* Move the physical cursor to the computed position and repaint */
        move(cursor_y, cursor_x);
        refresh();

        ch = getch();

        switch (ch) {
            /* Ctrl+O – save the current note to disk */
            case CTRL_O:
                save_notes(date, subject, text);
                attron(STATUS_ATTR);
                mvprintw(LINES - 1, 0, "Saved!");
                clrtoeol();
                attroff(STATUS_ATTR);
                refresh();
                napms(500);  /* keep the confirmation message visible for 0.5 s */
                break;

            /* Ctrl+X – exit the editor */
            case CTRL_X:
                running = 0;
                break;

            /* Ctrl+F – view notes.txt through less */
            case CTRL_F: {
                /* FIX #6: Moved 'home' declaration inside a proper block.
                   In the original code 'char *home' was declared directly inside a
                   case label without braces, which is technically valid in C99+ but
                   dangerous – a jump to another case would skip the initialisation,
                   leading to undefined behavior. Wrapping in {} fixes this. */
                char *home = getenv("HOME");

                def_prog_mode();  /* save the current ncurses terminal state */
                endwin();         /* release ncurses and hand the terminal back to the shell */

                if (home) {
                    char cmd[1024];
                    /* FIX #7: Check for file existence before calling less.
                       In the original code, if notes.txt did not yet exist, less would
                       print an error directly to the terminal and corrupt the ncurses display.
                       Now we show a friendly message and wait for Enter instead. */
                    snprintf(cmd, sizeof(cmd),
                             "[ -f \"%s/notes.txt\" ] && less \"%s/notes.txt\" "
                             "|| (echo 'notes.txt does not exist yet. Press Enter.' && read)",
                             home, home);
                    system(cmd);
                } else {
                    printf("Cannot find home directory!\n");
                    getchar();
                }

                reset_prog_mode();  /* restore the saved ncurses terminal state */
                refresh();          /* repaint the editor screen */
                break;
            }

            /* Backspace – delete the character to the left of the cursor */
            case KEY_BACKSPACE:
            case 127:
            case '\b':
                if (pos > 0) {
                    /* shift the tail of the buffer one position to the left (including '\0') */
                    memmove(&text[pos - 1], &text[pos], len - pos + 1);
                    pos--;
                    len--;
                }
                break;

            /* Enter – insert a newline at the cursor position */
            case '\n':
                if (len < MAX_TEXT - 1) {
                    memmove(&text[pos + 1], &text[pos], len - pos + 1);
                    text[pos] = '\n';
                    pos++;
                    len++;
                }
                break;

            /* Arrow left – move cursor one character to the left */
            case KEY_LEFT:
                if (pos > 0) pos--;
                break;

            /* Arrow right – move cursor one character to the right */
            case KEY_RIGHT:
                if (pos < len) pos++;
                break;

            /* Arrow up – move to the same column on the previous line */
            case KEY_UP: {
                /* Find the start of the current line */
                int line_start = pos;
                while (line_start > 0 && text[line_start - 1] != '\n') line_start--;

                if (line_start > 0) {
                    /* Column offset on the current line */
                    int col = pos - line_start;

                    /* Index of the '\n' that terminates the previous line */
                    int prev_end = line_start - 1;

                    /* Find the start of the previous line */
                    int prev_start = prev_end - 1;
                    while (prev_start > 0 && text[prev_start - 1] != '\n') prev_start--;

                    /* FIX #8: The original formula 'new_pos = prev_newline + (pos - line_start) + 1'
                       was wrong – it indexed from the newline character instead of the line start
                       and could jump past the end of the previous line.
                       New version: target = start of previous line + min(col, length of that line). */
                    int prev_line_len = prev_end - prev_start;
                    int new_pos = prev_start + (col < prev_line_len ? col : prev_line_len);
                    pos = new_pos;
                }
                break;
            }

            /* Arrow down – move to the same column on the next line */
            case KEY_DOWN: {
                /* Find the end of the current line */
                int line_end = pos;
                while (line_end < len && text[line_end] != '\n') line_end++;

                if (line_end < len) {
                    int next_line_start = line_end + 1;

                    /* Find the end of the next line */
                    int next_line_end = next_line_start;
                    while (next_line_end < len && text[next_line_end] != '\n') next_line_end++;

                    /* Column offset on the current line */
                    int line_start = pos;
                    while (line_start > 0 && text[line_start - 1] != '\n') line_start--;
                    int offset = pos - line_start;

                    int next_line_len = next_line_end - next_line_start;
                    pos = next_line_start + (offset < next_line_len ? offset : next_line_len);
                }
                break;
            }

            /* Any other printable ASCII character – insert at the cursor position */
            default:
                if (len < MAX_TEXT - 1 && ch >= 32 && ch <= 126) {
                    memmove(&text[pos + 1], &text[pos], len - pos + 1);
                    text[pos] = (char)ch;
                    pos++;
                    len++;
                }
                break;
        }
    }

    endwin();  /* shut down ncurses and restore the terminal to its original state */
    return 0;
}
