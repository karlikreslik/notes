#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_TEXT 10000
#define STATUS_ATTR A_REVERSE

// Explicitní definice ovládacích kláves
#define CTRL_O 15
#define CTRL_X 24
#define CTRL_F 6

int count_words(const char *text) {
    if (!text || !*text) return 0;
    
    int count = 0;
    int in_word = 0;
    
    for (int i = 0; text[i]; i++) {
        if (isspace(text[i])) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
    }
    return count;
}

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
    char date[128] = "";
    char subject[128] = "";
    char text[MAX_TEXT] = "";
    int len = 0;
    int pos = 0;
    int ch;
    int running = 1;
    
    // Inicializace ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    
    // Retro barevné schéma
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        bkgd(COLOR_PAIR(1));
    }
    
    // Načtení datumu a předmětu
    echo();
    attron(A_BOLD);
    mvprintw(0, 0, "Datum  : ");
    attroff(A_BOLD);
    refresh();
    getnstr(date, sizeof(date) - 1);
    
    attron(A_BOLD);
    mvprintw(1, 0, "Predmet: ");
    attroff(A_BOLD);
    refresh();
    getnstr(subject, sizeof(subject) - 1);
    
    noecho();
    clear();
    refresh();
    
    // Hlavní editační smyčka
    while (running) {
        erase();
        
        // Vypsání záhlaví
        attron(A_BOLD);
        mvprintw(0, 0, "[%s] %s", date, subject);
        attroff(A_BOLD);
        
        // Vypsání textu
        int y = 1;
        int x = 0;
        for (int i = 0; i < len; i++) {
            if (y >= LINES - 1) break;
            
            if (text[i] == '\n') {
                y++;
                x = 0;
            } else {
                if (x < COLS) {
                    mvaddch(y, x, text[i]);
                }
                x++;
                if (x >= COLS) {
                    x = 0;
                    y++;
                }
            }
        }
        
        // Výpočet pozice kurzoru
        int cursor_y = 1;
        int cursor_x = 0;
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
        
        // Status bar
        attron(STATUS_ATTR);
        
        // Počet slov
        int word_count = count_words(text);
        mvprintw(LINES - 1, 0, "Slova:%d", word_count);
        
        // Číslo řádku
        int line_num = 1;
        for (int i = 0; i < pos; i++) {
            if (text[i] == '\n') line_num++;
        }
        mvprintw(LINES - 1, COLS/2 - 5, "Radek:%d", line_num);
        
        // Zkratky
        mvprintw(LINES - 1, COLS - 20, "^O Uloz ^X Konec ^F Otevri");
        clrtoeol();
        attroff(STATUS_ATTR);
        
        move(cursor_y, cursor_x);
        refresh();
        
        ch = getch();
        
        switch (ch) {
            case CTRL_O: // Ctrl+O
                save_notes(date, subject, text);
                attron(STATUS_ATTR);
                mvprintw(LINES - 1, 0, "Ulozeno! ");
                clrtoeol();
                attroff(STATUS_ATTR);
                refresh();
                napms(500);
                break;
                
            case CTRL_X: // Ctrl+X
                running = 0;
                break;
                
            case CTRL_F: // Ctrl+F
                def_prog_mode();
                endwin();
                
                char *home = getenv("HOME");
                if (home) {
                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "less \"%s/notes.txt\"", home);
                    system(cmd);
                } else {
                    printf("Nelze najit domovsky adresar!\n");
                    getchar();
                }
                
                reset_prog_mode();
                refresh();
                break;
                
            case KEY_BACKSPACE:
            case 127:
            case '\b':
                if (pos > 0) {
                    memmove(&text[pos-1], &text[pos], len - pos + 1);
                    pos--;
                    len--;
                }
                break;
                
            case '\n':
                if (len < MAX_TEXT - 1) {
                    memmove(&text[pos+1], &text[pos], len - pos + 1);
                    text[pos] = '\n';
                    pos++;
                    len++;
                }
                break;
                
            case KEY_LEFT:
                if (pos > 0) pos--;
                break;
                
            case KEY_RIGHT:
                if (pos < len) pos++;
                break;
                
            case KEY_UP: {
                int line_start = pos;
                while (line_start > 0 && text[line_start-1] != '\n') line_start--;
                if (line_start > 0) {
                    int prev_newline = line_start - 2;
                    while (prev_newline > 0 && text[prev_newline] != '\n') prev_newline--;
                    int new_pos = prev_newline + (pos - line_start) + 1;
                    if (new_pos < len) pos = new_pos;
                }
                break;
            }
                
            case KEY_DOWN: {
                int line_end = pos;
                while (line_end < len && text[line_end] != '\n') line_end++;
                if (line_end < len) {
                    int next_line_start = line_end + 1;
                    int next_line_end = next_line_start;
                    while (next_line_end < len && text[next_line_end] != '\n') next_line_end++;
                    int offset = pos;
                    while (offset > 0 && text[offset-1] != '\n') offset--;
                    offset = pos - offset;
                    if (next_line_start + offset < next_line_end) {
                        pos = next_line_start + offset;
                    } else {
                        pos = next_line_end;
                    }
                }
                break;
            }
                
            default:
                if (len < MAX_TEXT - 1 && ch >= 32 && ch <= 126) {
                    memmove(&text[pos+1], &text[pos], len - pos + 1);
                    text[pos] = ch;
                    pos++;
                    len++;
                }
                break;
        }
    }
    
    endwin();
    return 0;
}
