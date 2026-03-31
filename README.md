# retro-notes

A tiny terminal text editor for jotting down notes. Green text on black. No frills.

## Build

```bash
gcc -o retro-notes editor.c -lncursesw
```

## Run

```bash
./retro-notes
```

Enter a date and subject, then start typing. Each note is appended to `~/notes.txt` in this format:

```
[2024-03-31] Meeting recap
Discussed the Q2 roadmap...
--------------------------------
```

## Keys

| Key | Action |
|---|---|
| `Ctrl+O` | Save |
| `Ctrl+X` | Quit |
| `Ctrl+F` | Browse saved notes |

## Requirements

`libncursesw` — install with `apt install libncursesw5-dev` or equivalent.
