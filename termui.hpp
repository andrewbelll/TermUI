#ifndef TERMUI_HPP
#define TERMUI_HPP

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cassert>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <conio.h>
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#  include <dirent.h>
#  include <sys/stat.h>
#endif

namespace termui {

// ─── Colors & Style ─────────────────────────────────────────────────────────

enum class Color {
    Default = 0,
    Black = 30, Red = 31, Green = 32, Yellow = 33,
    Blue = 34, Magenta = 35, Cyan = 36, White = 37,
    BrightBlack = 90, BrightRed = 91, BrightGreen = 92, BrightYellow = 93,
    BrightBlue = 94, BrightMagenta = 95, BrightCyan = 96, BrightWhite = 97
};

struct Style {
    Style()
        : fg_(Color::Default), bg_(Color::Default),
          is_bold_(false), is_underline_(false), is_reverse_(false) {}

    explicit Style(Color fg_color)
        : fg_(fg_color), bg_(Color::Default),
          is_bold_(false), is_underline_(false), is_reverse_(false) {}

    // Chainable instance methods — each returns a modified copy.
    Style bold()      const { Style s = *this; s.is_bold_      = true; return s; }
    Style underline() const { Style s = *this; s.is_underline_ = true; return s; }
    Style reversed()  const { Style s = *this; s.is_reverse_   = true; return s; }
    Style fg(Color c) const { Style s = *this; s.fg_ = c; return s; }
    Style bg(Color c) const { Style s = *this; s.bg_ = c; return s; }

    std::string begin() const {
        std::string seq;
        seq.reserve(16);
        seq = "\033[0";
        if (is_bold_)      seq += ";1";
        if (is_underline_) seq += ";4";
        if (is_reverse_)   seq += ";7";
        if (fg_ != Color::Default) seq += ";" + std::to_string(static_cast<int>(fg_));
        // ANSI background codes are foreground + 10: standard 30-37 → 40-47,
        // bright 90-97 → 100-107.  The +10 offset holds for both ranges.
        if (bg_ != Color::Default) seq += ";" + std::to_string(static_cast<int>(bg_) + 10);
        seq += "m";
        return seq;
    }

    static std::string reset() { return "\033[0m"; }

private:
    Color fg_;
    Color bg_;
    bool  is_bold_;
    bool  is_underline_;
    bool  is_reverse_;
};

// ─── Internal UTF-8 Helper ──────────────────────────────────────────────────
namespace detail {
// Returns the byte length of the UTF-8 sequence starting at s[i].
// Returns 0 for continuation bytes, invalid lead bytes, or if i >= s.size().
// Callers should skip invalid positions (i.e. call ++i; continue on return 0).
inline size_t utf8_char_len(const std::string& s, size_t i) {
    const size_t len = s.size();
    if (i >= len) return 0;
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = (c < 0x80)           ? 1
             : ((c & 0xE0) == 0xC0) ? 2
             : ((c & 0xF0) == 0xE0) ? 3
             : ((c & 0xF8) == 0xF0) ? 4
             : 0; // continuation or invalid lead byte
    if (n == 0 || i + n > len) return 0;
    return n;
}
} // namespace detail

// ─── UTF-8 Helpers ──────────────────────────────────────────────────────────

// Returns the display width (column count) of a UTF-8 string.
// Assumes all codepoints are single-width (no CJK fullwidth).
inline size_t utf8_display_width(const std::string& s) {
    size_t width = 0;
    size_t i = 0;
    const size_t len = s.size();
    while (i < len) {
        size_t char_len = detail::utf8_char_len(s, i);
        if (char_len == 0) { ++i; continue; } // continuation or invalid: skip
        i += char_len;
        ++width;
    }
    return width;
}

// Truncate a UTF-8 string to at most max_width display columns.
inline std::string utf8_truncate(const std::string& s, size_t max_width) {
    size_t width = 0;
    size_t i = 0;
    const size_t len = s.size();
    while (i < len && width < max_width) {
        size_t char_len = detail::utf8_char_len(s, i);
        if (char_len == 0) { ++i; continue; } // continuation or invalid: skip
        i += char_len;
        ++width;
    }
    return s.substr(0, i);
}

// ─── Text ───────────────────────────────────────────────────────────────────

struct TextSpan {
    std::string content;
    Style style;

    TextSpan() = default;
    explicit TextSpan(const std::string& text) : content(text), style() {}
    TextSpan(const std::string& text, const Style& s) : content(text), style(s) {}
};

class Text {
public:
    Text() = default;
    explicit Text(const std::string& content) { spans_.push_back(TextSpan(content)); }
    Text(const std::string& content, const Style& s) { spans_.push_back(TextSpan(content, s)); }
    Text(const std::string& content, Color fg) { spans_.push_back(TextSpan(content, Style(fg))); }

    Text& add(const std::string& content, const Style& s = Style()) {
        spans_.push_back(TextSpan(content, s));
        return *this;
    }

    Text& add(const std::string& content, Color fg) {
        return add(content, Style(fg));
    }

    // Renders the text to an ANSI escape sequence string.
    // If max_width > 0, content is truncated to at most max_width display columns.
    std::string render(int max_width = 0) const {
        std::string out;
        out.reserve(spans_.size() * 32);
        int remaining = max_width;
        for (const TextSpan& span : spans_) {
            if (max_width > 0 && remaining <= 0) break;
            const std::string* text = &span.content;
            std::string truncated;
            if (max_width > 0) {
                const int w = static_cast<int>(utf8_display_width(span.content));
                if (w > remaining) {
                    truncated = utf8_truncate(span.content, static_cast<size_t>(remaining));
                    text = &truncated;
                    remaining = 0;
                } else {
                    remaining -= w;
                }
            }
            out += span.style.begin();
            out += *text;
            out += "\033[0m";
        }
        return out;
    }

    // Returns the total display-column width (not byte length).
    size_t length() const {
        size_t len = 0;
        for (const TextSpan& span : spans_)
            len += utf8_display_width(span.content);
        return len;
    }

private:
    std::vector<TextSpan> spans_;
};

// ─── Table ──────────────────────────────────────────────────────────────────

class Table {
public:
    struct Column {
        std::string name;
        int width; // 0 = auto-sized to content
        Column(const std::string& n, int w) : name(n), width(w) {}
    };

    Table() {
        header_style_ = Style().bold().underline();
    }

    Table& add_column(const std::string& name, int width = 0) {
        columns_.push_back(Column(name, width));
        return *this;
    }

    Table& add_row(const std::vector<std::string>& cells) {
        rows_.push_back(cells);
        return *this;
    }

    Table& header_style(const Style& s) { header_style_ = s; return *this; }

    std::vector<Text> render(int available_width = 0) const {
        std::vector<Text> result;
        if (columns_.empty()) return result;

        // Determine column widths (fixed or auto-sized to content).
        std::vector<int> widths(columns_.size(), 0);
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (columns_[c].width > 0) {
                widths[c] = columns_[c].width;
            } else {
                widths[c] = static_cast<int>(utf8_display_width(columns_[c].name));
                for (size_t r = 0; r < rows_.size(); ++r) {
                    if (c < rows_[r].size()) {
                        int cell_w = static_cast<int>(utf8_display_width(rows_[r][c]));
                        if (cell_w > widths[c]) widths[c] = cell_w;
                    }
                }
            }
        }

        // Scale columns proportionally when a width limit is given.
        if (available_width > 0) {
            int total = 0;
            int separators = static_cast<int>(columns_.size()) - 1;
            for (size_t c = 0; c < widths.size(); ++c) total += widths[c];
            int usable = available_width - separators * 3; // " | " between cols
            if (usable > 0 && total > usable) {
                // Round-half-up to minimise cumulative truncation error.
                for (size_t c = 0; c < widths.size(); ++c)
                    widths[c] = std::max(1, static_cast<int>(
                        (static_cast<long long>(widths[c]) * usable + total / 2) / total));
            }
        }

        // Header row.
        Text header;
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (c > 0) header.add(" \xe2\x94\x82 ", Style(Color::BrightBlack));
            header.add(pad_or_truncate(columns_[c].name, widths[c]), header_style_);
        }
        result.push_back(header);

        // Separator row.
        Text sep;
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (c > 0) sep.add("\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80", Style(Color::BrightBlack));
            // Build a run of horizontal-rule characters (3 bytes each).
            std::string dash;
            dash.reserve(static_cast<size_t>(widths[c]) * 3);
            for (int i = 0; i < widths[c]; ++i) dash += "\xe2\x94\x80";
            sep.add(dash, Style(Color::BrightBlack));
        }
        result.push_back(sep);

        // Data rows.
        for (size_t r = 0; r < rows_.size(); ++r) {
            Text row;
            for (size_t c = 0; c < columns_.size(); ++c) {
                if (c > 0) row.add(" \xe2\x94\x82 ", Style(Color::BrightBlack));
                const std::string& cell = (c < rows_[r].size()) ? rows_[r][c] : empty_str();
                row.add(pad_or_truncate(cell, widths[c]));
            }
            result.push_back(row);
        }

        return result;
    }

private:
    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
    Style header_style_;

    // Returns a ref to an empty string; avoids constructing temporaries in render().
    static const std::string& empty_str() {
        static const std::string s;
        return s;
    }

    static std::string pad_or_truncate(const std::string& s, int width) {
        if (width <= 0) return "";
        int len = static_cast<int>(utf8_display_width(s));
        if (len <= width)
            return s + std::string(static_cast<size_t>(width - len), ' ');
        if (width <= 1) return "\xe2\x80\xa6"; // …
        return utf8_truncate(s, static_cast<size_t>(width - 1)) + "\xe2\x80\xa6";
    }
};

// ─── ProgressBar ────────────────────────────────────────────────────────────

// A simple horizontal progress bar that renders to a single Text line.
// The user owns the value and calls render() inside a tick callback.
//
// Example (inside an on_tick callback):
//   bar.set_value(progress);
//   live_page.clear();
//   live_page.add_line(bar.render(30));
class ProgressBar {
public:
    ProgressBar()
        : value_(0.0), fill_(Color::Green), empty_(Color::Default) {}

    // Set the fill fraction in the range [0.0, 1.0].
    ProgressBar& set_value(double v) {
        value_ = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
        return *this;
    }

    // Color applied to the filled block characters.
    ProgressBar& set_fill_color(Color c)  { fill_  = c; return *this; }

    // Color applied to the empty block characters.
    ProgressBar& set_empty_color(Color c) { empty_ = c; return *this; }

    double value() const { return value_; }

    // Render a bar of the given character width followed by a percentage label.
    // Characters: U+2588 FULL BLOCK (█) for fill, U+2591 LIGHT SHADE (░) for empty.
    Text render(int width = 20) const {
        if (width <= 0) width = 1;
        const int filled = static_cast<int>(value_ * width + 0.5);
        const int empty  = width - filled;

        // UTF-8 encodings: █ = \xe2\x96\x88, ░ = \xe2\x96\x91
        const char* fill_char  = "\xe2\x96\x88";
        const char* empty_char = "\xe2\x96\x91";

        std::string filled_chars, empty_chars;
        filled_chars.reserve(static_cast<size_t>(filled) * 3);
        empty_chars.reserve(static_cast<size_t>(empty)  * 3);
        for (int i = 0; i < filled; ++i) filled_chars += fill_char;
        for (int i = 0; i < empty;  ++i) empty_chars  += empty_char;

        const int pct = static_cast<int>(value_ * 100.0 + 0.5);

        Text t;
        t.add("[", Style(Color::BrightBlack));
        if (!filled_chars.empty()) t.add(filled_chars, Style(fill_));
        if (!empty_chars.empty())  t.add(empty_chars,  Style(empty_));
        t.add("] ", Style(Color::BrightBlack));
        t.add(std::to_string(pct) + "%", Style().bold());
        return t;
    }

private:
    double value_;
    Color  fill_;
    Color  empty_;
};

// Page and SelectableList are defined after the detail namespace.

// ─── Platform Detail ────────────────────────────────────────────────────────

namespace detail {

enum Key {
    KEY_NONE = 0,
    KEY_QUIT,
    KEY_CTRL_C,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,
    KEY_SPACE,
    KEY_RESIZE,
    KEY_OTHER
};

struct TermSize {
    int cols;
    int rows;
};

#ifdef _WIN32

inline HANDLE& hStdout_ref() { static HANDLE h = INVALID_HANDLE_VALUE; return h; }
inline HANDLE& hStdin_ref()  { static HANDLE h = INVALID_HANDLE_VALUE; return h; }
inline DWORD&  orig_out_mode_ref() { static DWORD v = 0; return v; }
inline DWORD&  orig_in_mode_ref()  { static DWORD v = 0; return v; }
inline UINT&   orig_cp_ref()       { static UINT  v = 0; return v; }

inline void enter_raw_mode() {
    hStdout_ref() = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdin_ref()  = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdout_ref(), &orig_out_mode_ref());
    GetConsoleMode(hStdin_ref(),  &orig_in_mode_ref());
    DWORD out_mode = orig_out_mode_ref() | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdout_ref(), out_mode);
    DWORD in_mode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(hStdin_ref(), in_mode);
    orig_cp_ref() = GetConsoleOutputCP();
    SetConsoleOutputCP(65001);
}

inline void exit_raw_mode() {
    if (hStdout_ref() == INVALID_HANDLE_VALUE) return;
    SetConsoleMode(hStdout_ref(), orig_out_mode_ref());
    SetConsoleMode(hStdin_ref(),  orig_in_mode_ref());
    SetConsoleOutputCP(orig_cp_ref());
}

inline TermSize get_terminal_size() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hStdout_ref(), &csbi);
    TermSize ts;
    ts.cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    ts.rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return ts;
}

inline Key read_key() {
    INPUT_RECORD ir;
    DWORD count;
    DWORD wait_result = WaitForSingleObject(hStdin_ref(), 100);
    if (wait_result != WAIT_OBJECT_0) return KEY_NONE;
    while (true) {
        if (!PeekConsoleInput(hStdin_ref(), &ir, 1, &count) || count == 0)
            return KEY_NONE;
        ReadConsoleInput(hStdin_ref(), &ir, 1, &count);
        if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) return KEY_RESIZE;
        if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
        WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
        WCHAR wch = ir.Event.KeyEvent.uChar.UnicodeChar;
        if (vk == VK_RETURN)             return KEY_ENTER;
        if (wch == L'q' || wch == L'Q') return KEY_QUIT;
        if (wch == 3)                   return KEY_CTRL_C;
        switch (vk) {
            case VK_LEFT:  return KEY_LEFT;
            case VK_RIGHT: return KEY_RIGHT;
            case VK_UP:    return KEY_UP;
            case VK_DOWN:  return KEY_DOWN;
            case VK_SPACE: return KEY_SPACE;
        }
        if (wch != 0) return KEY_OTHER;
    }
}

#else // POSIX

// Function-local statics avoid ODR violations when this header is included in
// multiple translation units.  Each accessor returns a reference to a single
// shared instance across the whole program.
inline struct termios& orig_termios_ref() {
    static struct termios t;
    return t;
}
inline bool& raw_mode_active_ref() {
    static bool active = false;
    return active;
}
inline volatile sig_atomic_t& g_resize_flag_ref() {
    static volatile sig_atomic_t flag = 0;
    return flag;
}

inline void exit_raw_mode() {
    if (raw_mode_active_ref()) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios_ref());
        raw_mode_active_ref() = false;
    }
}

inline void enter_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios_ref()) != 0)
        return; // not a terminal; raw mode unavailable
    struct termios raw = orig_termios_ref();
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1; // 100 ms read timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active_ref() = true;
}

inline TermSize get_terminal_size() {
    struct winsize ws;
    TermSize ts = {80, 24};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        ts.cols = ws.ws_col;
        ts.rows = ws.ws_row;
    }
    return ts;
}

inline Key read_key() {
    if (g_resize_flag_ref()) {
        g_resize_flag_ref() = 0;
        return KEY_RESIZE;
    }

    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return KEY_NONE;

    if (c == '\r')                 return KEY_ENTER;   // CR
    if (c == 'q' || c == 'Q')     return KEY_QUIT;
    if (c == '\x03')               return KEY_CTRL_C;  // ETX / Ctrl+C
    if (c == ' ')                  return KEY_SPACE;

    if (c == 27) { // ESC sequence
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return KEY_OTHER;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return KEY_OTHER;
        if (seq[0] == '[') {
            // Single-letter final byte: standard cursor-movement sequences.
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
            }
            // Longer CSI sequences (e.g. \033[1;5C): drain until a letter
            // terminates the sequence so stale bytes don't pollute the next
            // read_key() call.
            if (seq[1] >= '0' && seq[1] <= '9') {
                // Drain until a letter terminates the sequence; cap at 32 bytes
                // to prevent an indefinite loop on malformed or adversarial input.
                int limit = 32;
                unsigned char drain;
                while (limit-- > 0 && read(STDIN_FILENO, &drain, 1) > 0) {
                    if ((drain >= 'A' && drain <= 'Z') ||
                        (drain >= 'a' && drain <= 'z')) break;
                }
            }
        }
        return KEY_OTHER;
    }

    return KEY_OTHER;
}

#endif // _WIN32

inline void write_raw(const std::string& s) {
#ifdef _WIN32
    DWORD written;
    WriteConsoleA(hStdout_ref(), s.c_str(), static_cast<DWORD>(s.size()), &written, NULL);
#else
    // Loop to handle partial writes (signal interrupts, pipe back-pressure, …).
    const char* ptr = s.c_str();
    size_t remaining = s.size();
    while (remaining > 0) {
        ssize_t n = ::write(STDOUT_FILENO, ptr, remaining);
        if (n <= 0) break; // terminal closed or unrecoverable error
        ptr       += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
#endif
}

inline void clear_screen() { write_raw("\033[2J"); }
inline void move_cursor(int row, int col) {
    write_raw("\033[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H");
}
inline void hide_cursor() { write_raw("\033[?25l"); }
inline void show_cursor() { write_raw("\033[?25h"); }

} // namespace detail

// ─── SelectableList ─────────────────────────────────────────────────────────

class SelectableList {
public:
    SelectableList()
        : cursor_(0), cursor_style_(Style().reversed()), multi_select_(false) {}

    SelectableList& add_item(const std::string& item,
                             std::function<void()> action = nullptr) {
        items_.push_back(item);
        actions_.push_back(std::move(action));
        selected_.push_back(false);
        return *this;
    }

    // Post-selection hook called on every ENTER. Fires after the per-item action.
    // Use per-item actions for specific behaviour; use this for cross-cutting
    // concerns (e.g. dismiss a status bar, refresh a sibling page).
    // Note: fires after per-item action when both are set.
    SelectableList& set_on_select(std::function<void(int, const std::string&)> cb) {
        on_select_ = std::move(cb);
        return *this;
    }

    // deprecated: use set_on_select()
    SelectableList& on_select(std::function<void(int, const std::string&)> cb) {
        return set_on_select(std::move(cb));
    }

    SelectableList& normal_style(const Style& s) { normal_style_ = s; return *this; }
    SelectableList& cursor_style(const Style& s) { cursor_style_ = s; return *this; }

    int cursor() const { return cursor_; }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    // Returns the item text at index, or an empty string if out of range.
    const std::string& get_item(int index) const {
        static const std::string empty_str;
        if (index < 0 || static_cast<size_t>(index) >= items_.size()) return empty_str;
        return items_[static_cast<size_t>(index)];
    }

    // Clears all items, resets cursor and selection, and restores default styles.
    SelectableList& clear_items() {
        items_.clear();
        actions_.clear();
        selected_.clear();
        cursor_ = 0;
        on_select_ = nullptr;
        normal_style_ = Style();
        cursor_style_ = Style().reversed();
        return *this;
    }

    const std::string& selected_item() const {
        static const std::string empty;
        if (items_.empty()) return empty;
        return items_[static_cast<size_t>(cursor_)];
    }

    SelectableList& set_multi_select(bool enabled) { multi_select_ = enabled; return *this; }
    bool is_multi_select() const { return multi_select_; }

    std::vector<std::string> get_selected_items() const {
        std::vector<std::string> result;
        for (size_t i = 0; i < items_.size(); ++i)
            if (i < selected_.size() && selected_[i])
                result.push_back(items_[i]);
        return result;
    }

    void clear_selection() {
        for (size_t i = 0; i < selected_.size(); ++i) selected_[i] = false;
    }

    bool handle_key(detail::Key key) {
        if (items_.empty()) return false;
        switch (key) {
        case detail::KEY_UP:
            if (cursor_ > 0) { --cursor_; return true; }
            return false;
        case detail::KEY_DOWN:
            if (cursor_ + 1 < static_cast<int>(items_.size())) { ++cursor_; return true; }
            return false;
        case detail::KEY_ENTER:
            if (!actions_.empty() && actions_[static_cast<size_t>(cursor_)])
                actions_[static_cast<size_t>(cursor_)]();
            if (on_select_)
                on_select_(cursor_, items_[static_cast<size_t>(cursor_)]);
            return true;
        case detail::KEY_SPACE:
            if (multi_select_) {
                const size_t idx = static_cast<size_t>(cursor_);
                if (idx < selected_.size()) selected_[idx] = !selected_[idx];
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    std::vector<Text> render(int width) const {
        std::vector<Text> lines;
        lines.reserve(items_.size());
        for (size_t i = 0; i < items_.size(); ++i) {
            const bool is_cursor = (static_cast<int>(i) == cursor_);
            const Style& st = is_cursor ? cursor_style_ : normal_style_;

            if (multi_select_) {
                const bool checked = (i < selected_.size()) && selected_[i];
                std::string cursor_mark = is_cursor ? "> " : "  ";
                std::string checkbox    = checked   ? "[x] " : "[ ] ";
                std::string item_text   = items_[i];
                const int prefix_width = 6; // "> " (2) + "[x] " (4)
                if (width > prefix_width) {
                    int avail = width - prefix_width;
                    if (static_cast<int>(utf8_display_width(item_text)) > avail)
                        item_text = utf8_truncate(item_text, static_cast<size_t>(avail));
                }
                Text line;
                line.add(cursor_mark, st);
                line.add(checkbox, Style(Color::BrightBlack));
                line.add(item_text, st);
                lines.push_back(line);
                continue;
            }

            std::string content = is_cursor ? "> " : "  ";
            content += items_[i];

            // Truncate by display width, not raw byte length.
            if (width > 0 && utf8_display_width(content) > static_cast<size_t>(width))
                content = utf8_truncate(content, static_cast<size_t>(width));

            lines.push_back(Text(content, st));
        }
        return lines;
    }

private:
    std::vector<std::string>           items_;
    std::vector<std::function<void()>> actions_;
    std::vector<bool>                  selected_;
    int cursor_;
    bool multi_select_;
    std::function<void(int, const std::string&)> on_select_;
    Style normal_style_;
    Style cursor_style_;
};

// ─── Page ───────────────────────────────────────────────────────────────────

class Page {
public:
    explicit Page(const std::string& title)
        : title_(title), scroll_(0), has_list_(false), list_() {}

    const std::string& title() const { return title_; }

    Page& add_line(const Text& line) {
        lines_.push_back(line);
        return *this;
    }

    Page& add_line(const std::string& text) {
        lines_.push_back(Text(text));
        return *this;
    }

    Page& add_lines(const std::vector<Text>& lines) {
        for (const Text& line : lines) lines_.push_back(line);
        return *this;
    }

    Page& add_blank() {
        lines_.push_back(Text(""));
        return *this;
    }

    // Update a single line in-place without clearing the page.
    // Silently ignored if index is out of range.
    Page& update_line(size_t index, const Text& text) {
        if (index < lines_.size()) lines_[index] = text;
        return *this;
    }

    // Removes all static lines and resets the scroll position to 0.
    Page& clear() { lines_.clear(); scroll_ = 0; return *this; }

    Page& set_list(const SelectableList& list) {
        list_ = list;
        has_list_ = true;
        return *this;
    }

    bool has_list() const { return has_list_; }
    SelectableList& list() { return list_; }
    const SelectableList& list() const { return list_; }

    void scroll_up(int n = 1) {
        scroll_ = std::max(0, scroll_ - n);
    }

    void scroll_down(int n = 1, int visible_rows = 0) {
        int effective_rows = visible_rows > 0 ? visible_rows : total_lines();
        int max_scroll = std::max(0, total_lines() - effective_rows);
        scroll_ = std::min(scroll_ + n, max_scroll);
    }

    int scroll_offset() const { return scroll_; }
    const std::vector<Text>& lines() const { return lines_; }

    // Total number of content lines (static + list items).
    int total_lines() const {
        int count = static_cast<int>(lines_.size());
        if (has_list_)
            count += static_cast<int>(list_.size());
        return count;
    }

private:
    std::string title_;
    std::vector<Text> lines_;
    int scroll_;
    bool has_list_;
    SelectableList list_;
};

// ─── App ────────────────────────────────────────────────────────────────────

class App {
public:
    explicit App(const std::string& title = "")
        : title_(title), active_tab_(0), tab_offset_(0), running_(false), on_tick_() {}

    // Register a callback invoked roughly every 100 ms when no key is pressed.
    // Inside the callback the application has already re-entered the render
    // cycle, so mutating page content and returning is sufficient — render()
    // is called automatically after on_tick_() returns.
    App& set_on_tick(std::function<void()> cb) { on_tick_ = std::move(cb); return *this; }

    Page& add_page(const std::string& name) {
        pages_.push_back(Page(name));
        return pages_.back();
    }

    Page& page(size_t index) {
        assert(index < pages_.size() && "App::page index out of range");
        return pages_[index];
    }
    Page& active_page() { return pages_[active_tab_]; }
    size_t page_count() const { return pages_.size(); }
    size_t active_tab() const { return active_tab_; }

    void set_active_tab(size_t index) {
        if (index < pages_.size()) active_tab_ = index;
    }

    void run() {
        if (pages_.empty()) return;
        install_signals();
        detail::enter_raw_mode();
        detail::hide_cursor();
        running_ = true;

        render();
        while (running_) {
            detail::Key key = detail::read_key();
            if (key == detail::KEY_NONE) {
                if (on_tick_) { on_tick_(); render(); }
            } else {
                handle_key(key);
            }
        }

        detail::show_cursor();
        detail::clear_screen();
        detail::move_cursor(0, 0);
        detail::exit_raw_mode();
    }

private:
    std::string title_;   // reserved; currently unused in rendering
    // deque: push_back does not invalidate existing element references,
    // so the Page& returned by add_page() remains valid as long as no
    // page is erased.
    std::deque<Page> pages_;
    size_t active_tab_;
    size_t tab_offset_;
    bool running_;
    std::function<void()> on_tick_;

    void install_signals() {
#ifndef _WIN32
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);

        // NOTE: g_resize_flag_ref() accesses a function-local static, which is
        // technically non-conforming in POSIX signal handlers (not async-signal-safe),
        // but resolves to a single .bss load on GCC/Clang/Linux and is safe in practice.
        sa.sa_handler = [](int) { detail::g_resize_flag_ref() = 1; };
        sigaction(SIGWINCH, &sa, NULL);

        // SIGINT/SIGTERM handler: only async-signal-safe operations permitted.
        //   • ::write() with a literal string — safe.
        //   • tcsetattr() (called inside exit_raw_mode) — safe.
        //   • _Exit() — safe.
        // Notably absent: std::to_string, malloc, stdio — all unsafe in handlers.
        sa.sa_handler = [](int) {
            // Restore cursor visibility, clear screen, home cursor in one write.
            static const char seq[] = "\033[?25h\033[2J\033[1;1H";
            ::write(STDOUT_FILENO, seq, sizeof(seq) - 1);
            detail::exit_raw_mode();
            _Exit(0);
        };
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
#endif
    }

    void handle_key(detail::Key key) {
        if (key == detail::KEY_NONE) return;

        if (key == detail::KEY_QUIT || key == detail::KEY_CTRL_C) {
            running_ = false;
            return;
        }
        if (key == detail::KEY_RESIZE) {
            render();
            return;
        }

        Page& p = pages_[active_tab_];
        if (p.has_list() && p.list().handle_key(key)) {
            render();
            return;
        }

        switch (key) {
        case detail::KEY_LEFT:
            if (active_tab_ > 0) {
                --active_tab_;
                if (active_tab_ < tab_offset_) tab_offset_ = active_tab_;
                render();
            }
            break;
        case detail::KEY_RIGHT:
            if (active_tab_ + 1 < pages_.size()) { ++active_tab_; render(); }
            break;
        case detail::KEY_UP:
            pages_[active_tab_].scroll_up(1);
            render();
            break;
        case detail::KEY_DOWN: {
            const detail::TermSize ts = detail::get_terminal_size();
            const int content_rows = std::max(1, ts.rows - 3);
            pages_[active_tab_].scroll_down(1, content_rows);
            render();
            break;
        }
        default:
            break;
        }
    }

    void render() {
        const detail::TermSize ts = detail::get_terminal_size();
        const int W = ts.cols;
        const int H = ts.rows;
        if (W < 10 || H < 5) return;

        std::string buf;
        buf.reserve(static_cast<size_t>(W * H) * 8);

        buf += "\033[H\033[0m"; // home cursor, reset attributes

        // Ensure active_tab_ is not left of the visible window (right scroll is
        // handled by the advancement loop below).
        if (active_tab_ < tab_offset_) tab_offset_ = active_tab_;

        // Returns the index of the last tab that fits when rendering from `offset`
        // within `budget` display columns.  Always returns at least `offset`.
        auto compute_last_visible = [&](size_t offset, int budget) -> size_t {
            if (offset >= pages_.size()) return offset;
            size_t last = offset;
            int used = static_cast<int>(utf8_display_width(pages_[offset].title())) + 2;
            for (size_t i = offset + 1; i < pages_.size(); ++i) {
                const int tab_w = static_cast<int>(utf8_display_width(pages_[i].title())) + 2;
                // Reserve 2 cols for ' >' indicator if there are tabs after this one.
                const int right_reserve = (i + 1 < pages_.size()) ? 2 : 0;
                if (used + 1 + tab_w + right_reserve > budget) break;
                used += 1 + tab_w; // separator + tab
                last = i;
            }
            return last;
        };

        // Advance tab_offset_ until active_tab_ is within the visible window.
        while (true) {
            const int budget = W - 3 - (tab_offset_ > 0 ? 2 : 0);
            const size_t lv = compute_last_visible(tab_offset_, budget);
            if (active_tab_ <= lv) break;
            ++tab_offset_;
        }

        // Final visible range for rendering.
        const int tab_budget = W - 3 - (tab_offset_ > 0 ? 2 : 0);
        const size_t last_visible = compute_last_visible(tab_offset_, tab_budget);

        // Build tab bar string.
        std::string tab_str;
        size_t tab_plain_len = 0;
        if (tab_offset_ > 0) {
            tab_str += "\033[90m<\033[0m "; // dim '<' + plain space
            tab_plain_len += 2;
        }
        for (size_t i = tab_offset_; i <= last_visible; ++i) {
            const std::string& tab_title = pages_[i].title();
            if (i == active_tab_) {
                tab_str += "\033[1;7m " + tab_title + " \033[0m";
            } else {
                tab_str += "\033[0m " + tab_title + " ";
            }
            tab_plain_len += utf8_display_width(tab_title) + 2;
            if (i < last_visible) {
                tab_str += "\033[90m|\033[0m";
                tab_plain_len += 1; // separator
            }
        }
        if (last_visible + 1 < pages_.size()) {
            tab_str += " \033[90m>\033[0m"; // plain space + dim '>'
            tab_plain_len += 2;
        }

        // Top border: corner + dash + tab bar + remaining dashes + corner.
        buf += "\033[90m\xe2\x94\x8c\xe2\x94\x80\033[0m";
        buf += tab_str;
        int remaining = W - 3 - static_cast<int>(tab_plain_len);
        if (remaining > 0) {
            buf += "\033[90m";
            for (int i = 0; i < remaining; ++i) buf += "\xe2\x94\x80";
            buf += "\033[0m";
        }
        buf += "\033[90m\xe2\x94\x90\033[0m";

        // Content area.
        int content_rows = std::max(1, H - 3);

        const Page& p = pages_[active_tab_];
        const int scroll = p.scroll_offset();

        // Combine static page lines with selectable list lines (if present).
        const std::vector<Text>& static_lines = p.lines();
        const int n_static = static_cast<int>(static_lines.size());

        std::vector<Text> list_lines;
        if (p.has_list()) {
            list_lines = p.list().render(W - 4);
        }
        const int total = n_static + static_cast<int>(list_lines.size());

        for (int row = 0; row < content_rows; ++row) {
            buf += "\033[";
            buf += std::to_string(2 + row);
            buf += ";1H\033[90m\xe2\x94\x82\033[0m"; // row position + left border

            const int line_idx = scroll + row;
            if (line_idx < total) {
                const Text& line = (line_idx < n_static)
                    ? static_lines[static_cast<size_t>(line_idx)]
                    : list_lines[static_cast<size_t>(line_idx - n_static)];
                const size_t plain_len = line.length();
                const int content_width = W - 3;
                buf += ' ';
                buf += line.render(content_width); // truncates overflowing lines
                if (static_cast<int>(plain_len) < content_width) {
                    const int pad = content_width - static_cast<int>(plain_len);
                    buf += std::string(static_cast<size_t>(pad), ' ');
                }
            } else {
                buf += std::string(static_cast<size_t>(W - 2), ' ');
            }

            buf += "\033[90m\xe2\x94\x82\033[0m"; // right border
        }

        // Bottom border with status hints and scroll position.
        buf += "\033[";
        buf += std::to_string(H - 1);
        buf += ";1H";

        const char* status_hint;
        if (p.has_list() && p.list().is_multi_select())
            status_hint = " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs"
                          "  [\xe2\x86\x91\xe2\x86\x93] select  [Space] toggle  [Enter] confirm ";
        else if (p.has_list())
            status_hint = " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] select  [Enter] choose ";
        else
            status_hint = " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] scroll ";

        std::string scroll_hint;
        if (total > content_rows) {
            const int end = std::min(scroll + content_rows, total);
            scroll_hint = ' ' + std::to_string(scroll + 1) + '-'
                + std::to_string(end) + '/' + std::to_string(total) + ' ';
        }

        const int status_plain_len  = static_cast<int>(utf8_display_width(status_hint));
        const int scroll_plain_len  = static_cast<int>(scroll_hint.size());
        const int total_fixed       = status_plain_len + scroll_plain_len;
        int left_dash  = std::max(0, (W - 2 - total_fixed) / 2);
        int right_dash = std::max(0, W - 2 - total_fixed - left_dash);

        buf += "\033[90m\xe2\x94\x94";
        for (int i = 0; i < left_dash; ++i)  buf += "\xe2\x94\x80";
        buf += "\033[0m";
        buf += status_hint;
        buf += "\033[90m";
        for (int i = 0; i < right_dash; ++i) buf += "\xe2\x94\x80";
        if (!scroll_hint.empty()) {
            buf += "\033[0m";
            buf += scroll_hint;
            buf += "\033[90m";
        }
        buf += "\xe2\x94\x98\033[0m";
        buf += "\033[J"; // clear from cursor to end of screen

        detail::write_raw(buf);
    }
};

// ─── FileBrowser ─────────────────────────────────────────────────────────────

// A reusable file browser widget that occupies its own tab in an App.
// The user can navigate directories with UP/DOWN/ENTER; selecting a file
// fires on_file_selected() and updates the "Selected:" header line.
//
// Lifetime requirement: the FileBrowser instance must outlive app.run(),
// because SelectableList item lambdas capture `this`.
class FileBrowser {
public:
    explicit FileBrowser(const std::string& start_path = ".")
        : current_path_(start_path), page_(nullptr) {
        while (current_path_.size() > 1 && current_path_.back() == '/')
            current_path_.pop_back();
    }

    // Callback fired when a file (not a directory) is confirmed; receives the full path.
    FileBrowser& on_file_selected(std::function<void(const std::string&)> cb) {
        on_file_selected_ = std::move(cb);
        return *this;
    }

    // Full path of the last selected file; empty if nothing has been selected yet.
    const std::string& selected_file() const { return selected_file_; }

    // Add a tab named tab_name to app, seed its content, and return the Page&.
    // LIFETIME: this object must outlive app.run() — lambdas capture `this`.
    Page& attach(App& app, const std::string& tab_name = "Files") {
        Page& p = app.add_page(tab_name);
        page_ = &p;
        navigate_to(current_path_);
        return p;
    }

private:
    struct Entry { std::string name; bool is_dir; };

    std::string current_path_;
    std::string selected_file_;
    std::function<void(const std::string&)> on_file_selected_;
    Page* page_; // raw ptr safe: deque never invalidates existing elements

    // Returns the parent directory of path (which must have no trailing slash).
    static std::string parent_path(const std::string& path) {
        size_t pos = path.rfind('/');
        if (pos == std::string::npos) return "."; // relative path, no separator
        if (pos == 0)                return "/";  // e.g. parent of "/foo" is "/"
        return path.substr(0, pos);
    }

    std::vector<Entry> list_directory(const std::string& path) const {
        std::vector<Entry> entries;
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((path + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return entries;
        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;
            bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entries.push_back({name, is_dir});
        } while (FindNextFileA(h, &fd));
        FindClose(h);
#else
        DIR* dir = opendir(path.c_str());
        if (!dir) return entries;
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string name = ent->d_name;
            if (name[0] == '.') continue; // skip ., .., and hidden entries
            std::string full = path + "/" + name;
            struct stat st;
            bool is_dir = false;
            if (stat(full.c_str(), &st) == 0)
                is_dir = S_ISDIR(st.st_mode);
            entries.push_back({name, is_dir});
        }
        closedir(dir);
#endif
        // Directories first, then files; alphabetical within each group.
        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) {
                      if (a.is_dir != b.is_dir) return a.is_dir && !b.is_dir;
                      return a.name < b.name;
                  });
        return entries;
    }

    void navigate_to(const std::string& path) {
        current_path_ = path;
        while (current_path_.size() > 1 && current_path_.back() == '/')
            current_path_.pop_back();

        page_->clear();
        page_->add_line(Text("File Browser", Style().bold().fg(Color::Cyan)));
        page_->add_line(Text("Path: " + current_path_, Style(Color::BrightBlack)));
        page_->add_line(Text(""));
        if (!selected_file_.empty())
            page_->add_line(Text("Selected: ").add(selected_file_, Style(Color::Green)));

        std::vector<Entry> entries = list_directory(current_path_);
        SelectableList lst;

        const std::string parent = parent_path(current_path_);
        lst.add_item("../", [this, parent]() { navigate_to(parent); });

        for (const Entry& e : entries) {
            if (e.is_dir) {
                std::string dir_path = current_path_ + "/" + e.name;
                lst.add_item(e.name + "/", [this, dir_path]() { navigate_to(dir_path); });
            }
        }
        for (const Entry& e : entries) {
            if (!e.is_dir) {
                std::string file_path = current_path_ + "/" + e.name;
                lst.add_item(e.name, [this, file_path]() {
                    selected_file_ = file_path;
                    if (on_file_selected_) on_file_selected_(selected_file_);
                    navigate_to(current_path_);
                });
            }
        }

        page_->set_list(lst);
    }
};

} // namespace termui

#endif // TERMUI_HPP
