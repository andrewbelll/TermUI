# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TermUI is a header-only C++11 terminal GUI framework with no external dependencies. It provides tabbed pages, styled text, progress bars, selectable lists, tables, and scrollable content. Cross-platform: Linux, macOS, Windows.

## Build Commands

```bash
# Configure (from project root)
cmake -B build

# Build
cmake --build build

# Run the demo
./build/demo
```

## Architecture

The entire library is a single header: `termui.hpp`. The demo app is `termui_demo.cpp`.

### Key classes (all in `namespace termui`):

- **App** — Main application loop. Owns pages, handles raw terminal mode, input dispatch, and a periodic refresh callback (`set_on_refresh`). `run()` enters the event loop; quit with `q` or Ctrl+C.
- **Page** — A tab containing lines of `Text` and an optional `SelectableList`. Supports scrolling.
- **Text / TextSpan** — Rich text built from styled spans. `Text::add()` chains spans with different `Style`.
- **Style / Color** — ANSI styling (bold, underline, reverse, 16 foreground/background colors).
- **ProgressBar** — Renders a `[████░░░░] 45%` bar as a `Text` object via `to_text(width)`.
- **Table** — Column-based table with auto/fixed widths, box-drawing separators. `render()` returns `vector<Text>`.
- **SelectableList** — Navigable list with cursor highlight and `on_select` callback. Handles KEY_UP/DOWN/ENTER.

### Platform layer (`termui::detail`):

Raw terminal mode, key reading, terminal size, and cursor/screen control. `#ifdef _WIN32` splits Win32 console API vs POSIX termios paths. Key enum: `KEY_QUIT`, `KEY_LEFT/RIGHT/UP/DOWN`, `KEY_ENTER`, `KEY_RESIZE`, etc.

### Rendering flow:

`App::render()` builds a single string buffer with ANSI escape sequences — top border with tab bar, content area with box-drawing borders, bottom border with status hints and scroll position — then writes it in one `write_raw()` call to avoid flicker.

## Conventions

- C++11 standard (set in CMakeLists.txt)
- UTF-8 throughout; box-drawing and block characters used for UI elements
- Builder pattern with method chaining (e.g., `table.add_column(...).add_column(...)`)
- Pages are rebuilt via `clear()` + `add_line()` in callbacks rather than mutating individual lines
