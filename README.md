```
  _______ ______ _____  __  __ _    _ _____ 
 |__   __|  ____|  __ \|  \/  | |  | |_   _|
    | |  | |__  | |__) | \  / | |  | | | |  
    | |  |  __| |  _  /| |\/| | |  | | | |  
    | |  | |____| | \ \| |  | | |__| |_| |_ 
    |_|  |______|_|  \_\_|  |_|\____/|_____|
                                                                                
```

A header-only C++11 terminal GUI framework with no external dependencies. Drop in a single file and get tabbed pages, styled text, selectable lists, tables, progress bars, and live-updating content — on Linux, macOS, and Windows.

## Preview

```
┌─ Dashboard │ Actions │ Data │ Scroll │ About │ Live │ Files │ Logs  >─┐
│                                                                         │
│  Dashboard                                                              │
│                                                                         │
│  System Status                                                          │
│    Service:   Running                                                   │
│    Uptime:    14 days, 3 hours                                          │
│    Version:   1.0.0                                                     │
│                                                                         │
│                                                                         │
└────────────────── [q] quit  [←→] tabs  [↑↓] scroll ───────────────────┘
```

When tabs overflow the terminal width, dim `<` / `>` indicators appear at the edges and the bar scrolls automatically as you switch tabs.

## Features

- **Tabbed navigation** — multiple pages, switch with arrow keys; tab bar scrolls horizontally with `<`/`>` indicators when tabs exceed the terminal width
- **Styled text** — bold, underline, reverse, and 16 foreground/background colors
- **Selectable lists** — keyboard-driven menus with per-item actions or a global `on_select` callback
- **Tables** — fixed or auto-sized columns with box-drawing separators
- **File browser** — navigable filesystem widget with directory traversal and file-selection callback
- **Progress bars** — block-character bars with configurable colors and width
- **Live updates** — `set_on_tick` callback fires every ~100 ms for animated or polling content
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
    page.add_line(termui::Text("Hello, world!", termui::Style().bold().fg(termui::Color::Cyan)));
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

The demo (`termui_demo.cpp`) covers every feature across 15 tabs: styled text, a selectable actions menu with per-item callbacks, a data table, a scrollable list, an about page, a live-animating progress bar, a file browser, and additional static-content tabs that overflow a standard 80-column terminal to demonstrate horizontal tab bar scrolling.

---

## Common Patterns

### Key-value dashboard rows

```cpp
auto& page = app.add_page("Status");
page.add_line(termui::Text("Service: ", termui::Color::BrightBlack)
                  .add("Running", termui::Color::Green))
    .add_line(termui::Text("Version: ", termui::Color::BrightBlack)
                  .add("1.0.0"));
```

### Menu with per-item callbacks

```cpp
termui::SelectableList menu;
menu.add_item("Start",   [&]() { /* ... */ })
    .add_item("Stop",    [&]() { /* ... */ })
    .add_item("Restart", [&]() { /* ... */ });
page.set_list(menu);
```

### Live-updating content via `on_tick`

```cpp
int counter = 0;
app.set_on_tick([&]() {
    ++counter;
    app.active_page().clear();
    app.active_page().add_line("Tick count: " + std::to_string(counter));
});
```

### Displaying a table

```cpp
termui::Table table;
table.add_column("Name", 12).add_column("Status", 8);
table.add_row({"Alice", "Active"}).add_row({"Bob", "Away"});
page.add_blank().add_lines(table.render());
```

### Multi-select list

```cpp
termui::SelectableList list;
list.set_multi_select(true);
list.add_item("Option A").add_item("Option B").add_item("Option C");
list.on_select([&](int, const std::string&) {
    std::vector<std::string> chosen = list.get_selected_items();
    // process chosen...
});
page.set_list(list);
```

Space toggles the checkbox on the highlighted row; Enter confirms. Call `list.get_selected_items()` to retrieve all checked items.

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
| `Page& active_page()` | Returns a reference to the currently visible page. Shorthand for `page(active_tab())`. |
| `size_t page_count() const` | Returns the total number of pages. |
| `size_t active_tab() const` | Returns the index of the currently visible tab. |
| `void set_on_tick(std::function<void()> cb)` | Registers a callback invoked ~every 100 ms when no key is pressed. Use it to update page content for live/animated displays; `render()` is called automatically after each tick. |
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
| `Page& add_lines(const std::vector<Text>& lines)` | Appends each element of `lines`. Convenient for adding table output. Returns `*this` for chaining. |
| `Page& add_blank()` | Appends an empty line (vertical spacing shorthand). Returns `*this` for chaining. |
| `void clear()` | Removes all lines and resets the scroll position to 0. |
| `Page& set_list(const SelectableList& list)` | Attaches a `SelectableList` to this page. List items are rendered after the static lines. Returns `*this` for chaining. |
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
| `Text& add(const std::string& content, Color fg)` | Shorthand — appends a span colored with `fg`. Returns `*this` for chaining. |
| `std::string render(int max_width = 0) const` | Returns the text as an ANSI escape sequence string. If `max_width > 0`, content is truncated to at most `max_width` display columns. |
| `size_t length() const` | Returns the total display-column width (counts Unicode codepoints, not bytes). |

**Example — multi-style line:**

```cpp
termui::Text line;
line.add("Status: ", termui::Color::BrightBlack)
    .add("OK",       termui::Color::Green);
```

#### Style

Describes the visual appearance of a `TextSpan`. Methods return a modified copy so they can be chained.

```cpp
termui::Style s1;                                          // default (no attributes)
termui::Style s2(termui::Color::Red);                      // foreground color only
termui::Style s3 = termui::Style().bold();                 // bold, default color
termui::Style s4 = termui::Style().bold().fg(termui::Color::Cyan);  // bold cyan
termui::Style s5 = termui::Style().underline().fg(termui::Color::Yellow);
termui::Style s6 = termui::Style().reversed();             // reverse video
```

**Chainable instance methods** (each returns a new `Style`):

| Method | Description |
|---|---|
| `Style bold() const` | Set bold attribute. |
| `Style underline() const` | Set underline attribute. |
| `Style reversed() const` | Set reverse-video attribute (swap fg/bg). |
| `Style fg(Color c) const` | Set foreground color. |
| `Style bg(Color c) const` | Set background color. |

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

// Per-item actions: each item carries its own lambda.
list.add_item("Say hello",  [&]() { /* ... */ })
    .add_item("Show stats", [&]() { /* ... */ })
    .add_item("Quit",       [&]() { /* ... */ });

// Optional global callback — fires after the per-item action (if any).
list.on_select([](int index, const std::string& item) {
    // called for every Enter press; receives index and item text
});

page.set_list(list);
```

| Method | Description |
|---|---|
| `SelectableList& add_item(const std::string& item, std::function<void()> action = nullptr)` | Appends an item with an optional per-item action invoked on Enter. Returns `*this`. |
| `SelectableList& on_select(std::function<void(int, const std::string&)> cb)` | Sets a global callback invoked on Enter for any item (receives index and text). Runs after the per-item action if both are set. Returns `*this`. |
| `SelectableList& clear_items()` | Clears all items, resets cursor to 0, clears the `on_select` callback, and restores default styles. Returns `*this`. |
| `SelectableList& normal_style(const Style& s)` | Style for non-cursor rows (default: no attributes). Returns `*this`. |
| `SelectableList& cursor_style(const Style& s)` | Style for the cursor row (default: `Style().reversed()`). Returns `*this`. |
| `int cursor() const` | Returns the current cursor index (0-based). |
| `size_t size() const` | Returns the number of items. |
| `bool empty() const` | Returns `true` when there are no items. |
| `const std::string& selected_item() const` | Returns the text of the item at the cursor. |
| `const std::string& get_item(int index) const` | Returns the item text at `index`, or an empty string if out of range. |
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
page.add_lines(table.render());
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

### ProgressBar

A horizontal progress bar that renders to a single `Text` line using block characters (`█` fill, `░` empty).

```cpp
termui::ProgressBar bar;
bar.set_fill_color(termui::Color::Green)
   .set_empty_color(termui::Color::BrightBlack)
   .set_value(0.75);  // 75%

page.add_line(bar.render(30));  // 30-character wide bar
```

| Method | Description |
|---|---|
| `ProgressBar& set_value(double v)` | Sets the fill fraction in `[0.0, 1.0]`. Clamped automatically. Returns `*this`. |
| `ProgressBar& set_fill_color(Color c)` | Color for filled block characters (default: `Color::Green`). Returns `*this`. |
| `ProgressBar& set_empty_color(Color c)` | Color for empty block characters (default: `Color::Default`). Returns `*this`. |
| `double value() const` | Returns the current fill fraction. |
| `Text render(int width = 20) const` | Renders the bar as a `Text` line with a `[░░██] nn%` format. `width` is the number of block characters (not total line width). |

Typical live-update pattern using `set_on_tick`:

```cpp
termui::ProgressBar bar;
double progress = 0.0;

app.set_on_tick([&]() {
    progress += 0.01;
    if (progress > 1.0) progress = 0.0;
    bar.set_value(progress);
    live_page.clear();
    live_page.add_line(bar.render(40));
});
```

---

### FileBrowser

A self-contained filesystem navigator that occupies its own tab. The user browses directories with the standard cursor keys; pressing Enter on a file fires a callback and displays the selected path in the page header.

```cpp
termui::FileBrowser browser(".");          // start in the current directory
browser.on_file_selected([](const std::string& path) {
    // path is the full path of the chosen file
});
browser.attach(app, "Files");             // adds the tab and returns Page&
app.run();
```

> **Lifetime**: `FileBrowser` must outlive `app.run()`. Item callbacks capture `this`,
> so the browser object must remain alive for the duration of the event loop.

#### Constructor

```cpp
explicit FileBrowser(const std::string& start_path = ".");
```

Opens at `start_path`. Relative paths (e.g. `"."`) are accepted. Hidden entries (names starting with `.`) are excluded from the listing.

#### Methods

| Method | Description |
|---|---|
| `FileBrowser& on_file_selected(std::function<void(const std::string&)> cb)` | Registers a callback invoked with the full path whenever the user confirms a file. Returns `*this`. |
| `const std::string& selected_file() const` | Returns the full path of the last selected file, or an empty string if nothing has been selected yet. |
| `Page& attach(App& app, const std::string& tab_name = "Files")` | Adds a tab named `tab_name` to `app`, populates it with the initial directory listing, and returns the `Page&`. Must be called before `app.run()`. |

#### Page layout

```
  File Browser
  Path: /home/user/projects

> ../
  src/
  docs/
  README.md
  CMakeLists.txt

  Selected: /home/user/projects/README.md
```

- `../` — navigate to the parent directory (always the first item)
- `name/` — enter a subdirectory
- `name` — select a file; fires `on_file_selected` and updates the `Selected:` line

#### Keys

The standard `SelectableList` keys apply while the Files tab is active:

| Key | Action |
|---|---|
| `↑` / `↓` | Move cursor |
| Enter | Enter directory or select file |
| `←` / `→` | Switch to another tab |

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
| `←` / `→` | Switch tabs; tab bar scrolls automatically when tabs exceed terminal width |
| `↑` / `↓` | Scroll page (or move list cursor when a list is active) |
| Enter | Confirm selection in a `SelectableList` |

Terminal resize (SIGWINCH on POSIX, `WINDOW_BUFFER_SIZE_EVENT` on Windows) is handled automatically — the UI redraws at the new dimensions.

---

## License

TermUI is released under the **GNU General Public License v3.0**.
You are free to use, study, modify, and distribute it under the terms of the GPL-3.0. See <https://www.gnu.org/licenses/gpl-3.0.html> for the full license text.
