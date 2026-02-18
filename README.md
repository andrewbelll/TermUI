# TermUI

A header-only C++11 terminal GUI framework with no external dependencies. Drop in a single file and get tabbed pages, styled text, selectable lists, tables, and scrollable content — on Linux, macOS, and Windows.

## Preview

```
┌─ Dashboard │ Settings │ Data │ Scroll │ About ─────────────────────┐
│                                                                     │
│  Dashboard                                                          │
│                                                                     │
│  System Status                                                      │
│    Service:   Running                                               │
│    Uptime:    14 days, 3 hours                                      │
│    Version:   1.0.0                                                 │
│                                                                     │
│                                                                     │
│                                                                     │
└────── [q] quit  [←→] tabs  [↑↓] scroll ───────────────────────────┘
```

## Features

- **Tabbed navigation** — multiple pages, switch with arrow keys
- **Styled text** — bold, underline, reverse, and 16 foreground/background colors
- **Selectable lists** — keyboard-driven menus with `on_select` callbacks
- **Tables** — fixed or auto-sized columns with box-drawing separators
- **Scrollable content** — any page scrolls when content exceeds the terminal height
- **Box-drawing borders** — clean UI using Unicode box characters
- **Flicker-free rendering** — single-write buffer flush per frame
- **Single-header, zero dependencies** — just copy `termui.hpp`

## Requirements

- **C++11** or later
- **CMake 3.10+** (only needed to build the demo; library itself is header-only)
- **Terminal**: any ANSI/VT100-compatible terminal on Linux or macOS; Windows 10+ (Virtual Terminal Processing)

## Installation

Copy `termui.hpp` into your project and include it:

```cpp
#include "termui.hpp"
```

No build system changes required. Everything is in `namespace termui`.

## Quick Start

```cpp
#include "termui.hpp"

int main() {
    termui::App app;

    auto& page = app.add_page("Home");
    page.add_line(termui::Text("Hello, world!", termui::Style::bold(termui::Color::Cyan)));
    page.add_line(termui::Text("Press q to quit."));

    app.run();
    return 0;
}
```

Compile with any C++11 compiler:

```bash
g++ -std=c++11 -o hello hello.cpp
```

## Building the Demo

```bash
cmake -B build
cmake --build build
./build/demo
```

The demo (`termui_demo.cpp`) exercises every feature: styled text, a selectable settings menu, a data table, a scrollable list, and an about page.

---

## API Reference

### App

The top-level application object. Owns all pages, manages the terminal, and runs the event loop.

```cpp
termui::App app;           // no title
termui::App app("Title");  // optional title string (reserved for future use)
```

| Method | Description |
|---|---|
| `Page& add_page(const std::string& name)` | Creates a new tab with the given title and returns a reference to it. |
| `Page& page(size_t index)` | Returns a reference to the page at `index`. |
| `size_t page_count() const` | Returns the total number of pages. |
| `size_t active_tab() const` | Returns the index of the currently visible tab. |
| `void run()` | Enters raw terminal mode and blocks until the user quits (`q` or Ctrl+C). Cleans up the terminal on exit. |

---

### Page

A single tab. Holds a list of `Text` lines and an optional `SelectableList`.

```cpp
// Pages are created via App::add_page, not directly:
auto& p = app.add_page("My Page");
```

| Method | Description |
|---|---|
| `const std::string& title() const` | Returns the page title shown in the tab bar. |
| `Page& add_line(const Text& line)` | Appends a styled `Text` line. Returns `*this` for chaining. |
| `Page& add_line(const std::string& text)` | Appends a plain-text line. Returns `*this` for chaining. |
| `void clear()` | Removes all lines and resets the scroll position to 0. |
| `void set_list(const SelectableList& list)` | Attaches a `SelectableList` to this page. List items are rendered after the static lines. |
| `bool has_list() const` | Returns `true` if a list has been set. |
| `SelectableList& list()` | Returns a mutable reference to the attached list. |
| `const SelectableList& list() const` | Returns a const reference to the attached list. |
| `void scroll_up(int n = 1)` | Scrolls up by `n` lines (clamped to 0). |
| `void scroll_down(int n, int visible_rows)` | Scrolls down by `n` lines, clamped so the last line stays visible given `visible_rows` of display height. |
| `int scroll_offset() const` | Returns the current scroll position (0 = top). |
| `int total_lines() const` | Returns the total number of content lines (static lines + list items). |
| `const std::vector<Text>& lines() const` | Returns the vector of static `Text` lines. |

---

### Text & Style

`Text` is a sequence of styled spans. Build one by chaining `add()` calls.

#### Text

```cpp
termui::Text t1;                                    // empty
termui::Text t2("plain string");                    // unstyled
termui::Text t3("label", termui::Style::bold());    // styled
```

| Method | Description |
|---|---|
| `Text& add(const std::string& content, const Style& s = Style())` | Appends a span with the given style. Returns `*this` for chaining. |
| `std::string render() const` | Returns the text as an ANSI escape sequence string ready to write to a terminal. |
| `size_t length() const` | Returns the total display-column width (counts Unicode codepoints, not bytes). |

**Example — multi-style line:**

```cpp
termui::Text line;
line.add("Status: ", termui::Style(termui::Color::BrightBlack))
    .add("OK",       termui::Style(termui::Color::Green));
```

#### Style

Describes the visual appearance of a `TextSpan`.

```cpp
termui::Style s1;                              // default (no attributes)
termui::Style s2(termui::Color::Red);          // foreground color only
termui::Style s3 = termui::Style::bold();      // bold, default color
termui::Style s4 = termui::Style::bold(termui::Color::Cyan); // bold cyan
```

**Static builders** (return a new `Style`):

| Builder | Description |
|---|---|
| `Style::bold(Color fg = Color::Default)` | Bold text, optional foreground color. |
| `Style::underline(Color fg = Color::Default)` | Underlined text, optional foreground color. |
| `Style::reverse()` | Reverse video (swap fg/bg). |

**Data members** (read/write directly):

| Member | Type | Description |
|---|---|---|
| `fg` | `Color` | Foreground color (default: `Color::Default`). |
| `bg` | `Color` | Background color (default: `Color::Default`). |
| `is_bold` | `bool` | Bold attribute. |
| `is_underline` | `bool` | Underline attribute. |
| `is_reverse` | `bool` | Reverse-video attribute. |

---

### Color

```cpp
enum class Color {
    Default,                                           // terminal default
    Black, Red, Green, Yellow,                         // standard 4
    Blue, Magenta, Cyan, White,                        // standard 4
    BrightBlack, BrightRed, BrightGreen, BrightYellow, // bright 4
    BrightBlue, BrightMagenta, BrightCyan, BrightWhite  // bright 4
};
```

`Color` values are used for both `Style::fg` and `Style::bg`. When used as `bg`, the value is automatically shifted to the ANSI background range.

---

### SelectableList

A keyboard-navigable list with a highlighted cursor row. Attach to a page with `Page::set_list`.

```cpp
termui::SelectableList list;
list.add_item("Option A")
    .add_item("Option B")
    .add_item("Option C");

list.on_select([](int index, const std::string& item) {
    // called when Enter is pressed
});

page.set_list(list);
```

| Method | Description |
|---|---|
| `SelectableList& add_item(const std::string& item)` | Appends an item. Returns `*this`. |
| `SelectableList& on_select(std::function<void(int, const std::string&)> cb)` | Sets the callback invoked on Enter. Arguments: item index and item text. Returns `*this`. |
| `SelectableList& normal_style(const Style& s)` | Style for non-cursor rows (default: no attributes). Returns `*this`. |
| `SelectableList& cursor_style(const Style& s)` | Style for the cursor row (default: `Style::reverse()`). Returns `*this`. |
| `int cursor() const` | Returns the current cursor index (0-based). |
| `size_t size() const` | Returns the number of items. |
| `bool empty() const` | Returns `true` when there are no items. |
| `const std::string& selected_item() const` | Returns the text of the item at the cursor. |
| `bool handle_key(detail::Key key)` | Processes a key event; moves cursor or fires callback. Returns `true` if the key was consumed. |
| `std::vector<Text> render(int width) const` | Renders all items as `Text` lines, truncated to `width` columns. Cursor row is prefixed with `> `, others with two spaces. |

---

### Table

A column-based table with auto or fixed column widths and box-drawing separators.

```cpp
termui::Table table;
table.add_column("Name", 12)   // fixed 12-column width
     .add_column("Role")       // auto-sized to content
     .add_column("Status", 8);

table.add_row({"Alice", "Engineer", "Active"})
     .add_row({"Bob",   "Designer", "Away"});

// Render and add to a page:
for (auto& line : table.render())
    page.add_line(line);
```

| Method | Description |
|---|---|
| `Table& add_column(const std::string& name, int width = 0)` | Adds a column. `width = 0` means auto-size to the widest cell. Returns `*this`. |
| `Table& add_row(const std::vector<std::string>& cells)` | Adds a data row. Returns `*this`. |
| `Table& header_style(const Style& s)` | Sets the style for the header row (default: bold + underline). Returns `*this`. |
| `std::vector<Text> render(int available_width = 0) const` | Renders the table as a vector of `Text` lines. When `available_width > 0`, columns are scaled proportionally to fit. |

#### Column struct

```cpp
struct Column {
    std::string name;   // header label
    int width;          // 0 = auto-sized
};
```

Columns are stored internally as `Table::Column` objects and exposed through `render()`.

---

### UTF-8 Helpers

Two free functions in `namespace termui` for working with multi-byte strings.

```cpp
size_t      termui::utf8_display_width(const std::string& s);
std::string termui::utf8_truncate(const std::string& s, size_t max_width);
```

| Function | Description |
|---|---|
| `utf8_display_width(s)` | Returns the number of display columns occupied by `s`. Counts Unicode codepoints; assumes all are single-width (no CJK fullwidth support). |
| `utf8_truncate(s, max_width)` | Returns a prefix of `s` that occupies at most `max_width` display columns. The returned string is always valid UTF-8 (never splits a multibyte sequence). |

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `q` or `Q` | Quit |
| Ctrl+C | Quit |
| `←` / `→` | Switch tabs |
| `↑` / `↓` | Scroll page (or move list cursor when a list is active) |
| Enter | Confirm selection in a `SelectableList` |

Terminal resize (SIGWINCH on POSIX, `WINDOW_BUFFER_SIZE_EVENT` on Windows) is handled automatically — the UI redraws at the new dimensions.

---

## License

TermUI is released under the **GNU General Public License v3.0**.
You are free to use, study, modify, and distribute it under the terms of the GPL-3.0. See <https://www.gnu.org/licenses/gpl-3.0.html> for the full license text.
