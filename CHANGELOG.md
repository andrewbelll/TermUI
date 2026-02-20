# Changelog

All notable changes to TermUI are documented here.

## [1.0.0] - 2026-02-20

### Core Framework
- `App`, `Page`, `Text`, `TextSpan`, `Style`, `Color` — complete terminal GUI primitives
- `SelectableList` — navigable list with cursor highlight and `on_select` callback
- `Table` — column-based table with auto/fixed widths and box-drawing separators
- `ProgressBar` — live-updating progress indicator with `on_tick` callback
- `FileBrowser` — interactive filesystem navigator

### Features Added
- Multi-select mode for `SelectableList`
- Horizontal tab bar scrolling with overflow indicators (`<` / `>`)
- Per-item action callbacks on `SelectableList`
- `Text(str, Color)` convenience constructor
- `update_line()` for in-place line updates
- Method chaining throughout the API
- `set_on_select()` fluent setter
- Windows support (Win32 Console API with `ENABLE_VIRTUAL_TERMINAL_PROCESSING`)

### API
- Style API refactored to chainable instance methods (`.bold()`, `.fg(Color)`, etc.)
- `utf8_display_width()` and `utf8_truncate()` free functions
- Demo moved to `demo/` subdirectory with its own `CMakeLists.txt`

### Robustness
- Signal handler rewritten to use only async-signal-safe operations
- UTF-8 bounds checks in display-width and truncation functions
- ODR violation fixed (function-local statics)
- Partial write loop handles `EINTR` and short writes
- Poll-based ESC sequence drain
- Move semantics for `Page` and `App`
