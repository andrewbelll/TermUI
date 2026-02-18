# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Identity

- **Name**: TermUI (renamed from `tgui`)
- **Type**: Header-only C++11 terminal GUI framework, no external dependencies
- **License**: GPL 3.0

## File Structure

```
/home/bell/Dev/
├── termui.hpp          # Entire library (single header, ~800 lines)
├── termui_demo.cpp     # Demo: 5 tabs covering all features
├── CMakeLists.txt      # C++11, produces ./build/demo
├── README.md           # Full docs: install, quick-start, API reference
├── CLAUDE.md           # This file
├── .gitignore          # Excludes build/
└── build/
    └── demo            # Compiled binary
```

## Build Commands

```bash
cmake -B build          # Configure (from project root)
cmake --build build     # Build
./build/demo            # Run the demo
```

## Architecture

The entire library is a single header: `termui.hpp`. The demo app is `termui_demo.cpp`.

All public code lives in `namespace termui`. Internal platform code is in `namespace termui::detail`.

### Key classes

- **App** — Main application loop. Owns pages, handles raw terminal mode, input dispatch. `run()` enters the event loop; quit with `q` or Ctrl+C.
- **Page** — A tab containing lines of `Text` and an optional `SelectableList`. Supports scrolling.
- **Text / TextSpan** — Rich text built from styled spans. `Text::add()` chains spans with different `Style`.
- **Style / Color** — ANSI styling (bold, underline, reverse, 17 colors: Default + 8 standard + 8 bright).
- **Table** — Column-based table with auto/fixed widths, box-drawing separators. `render()` returns `vector<Text>`.
- **SelectableList** — Navigable list with cursor highlight and `on_select` callback. Handles KEY_UP/DOWN/ENTER.

### Free functions

- `utf8_display_width(string)` — codepoint count (no CJK fullwidth)
- `utf8_truncate(string, max_width)` — safe UTF-8 prefix

### Platform layer (`termui::detail`)

- **POSIX**: `termios` raw mode, `ioctl` for terminal size, `SIGWINCH` for resize, `SIGINT/SIGTERM` for clean exit
- **Windows**: Win32 Console API (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`, `WaitForSingleObject`)
- Key enum: `KEY_NONE`, `KEY_QUIT`, `KEY_CTRL_C`, `KEY_LEFT/RIGHT/UP/DOWN`, `KEY_ENTER`, `KEY_RESIZE`, `KEY_OTHER`

### Rendering flow

`App::render()` builds a single string buffer — top border with tab bar, content rows with left/right borders, bottom border with key hints and scroll position — then flushes in one `write_raw()` call to avoid flicker.

## Conventions

- C++11 standard (set in CMakeLists.txt)
- UTF-8 throughout; box-drawing characters used for UI elements
- Builder pattern with method chaining (e.g., `table.add_column(...).add_column(...)`)
- Pages rebuild via `clear()` + `add_line()` in callbacks — no in-place line mutation
- `SelectableList` is attached to a page via `page.set_list(list)` after items and callback are configured

## Git State

- Branch: `main`
- Auth: GitHub CLI (`gh`)
- Commits (oldest → newest):
  - `5b730e5` — LICENSE
  - `79ce196` — Initial commit: all project files
  - `0361972` — Remove ProgressBar class and live-refresh mechanism
  - `c47439e` — Optimise and clarify termui.hpp
  - `e56b199` — Add README with full API reference and quick-start guide

## Session History

1. `/init` run — CLAUDE.md created
2. Project renamed from `tgui` → `TermUI`: namespace, filenames, include guards, CMake, CLAUDE.md all updated
3. Git repo initialized, GitHub CLI authenticated, pushed to GitHub
4. `ProgressBar` class and `set_on_refresh` mechanism removed from the library
5. `termui.hpp` optimised and clarified
6. `README.md` written: preview mockup, features, requirements, installation, quick-start, full API reference, keyboard shortcuts, license
7. `context.md` merged into `CLAUDE.md` and deleted
8. Code review completed and all critical/high-priority issues fixed in refactoring session
