#ifndef TERMUI_HPP
#define TERMUI_HPP

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

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
    Color fg;
    Color bg;
    bool is_bold;
    bool is_underline;
    bool is_reverse;

    Style()
        : fg(Color::Default), bg(Color::Default),
          is_bold(false), is_underline(false), is_reverse(false) {}

    explicit Style(Color fg_color)
        : fg(fg_color), bg(Color::Default),
          is_bold(false), is_underline(false), is_reverse(false) {}

    static Style bold(Color fg = Color::Default) {
        Style s; s.fg = fg; s.is_bold = true; return s;
    }
    static Style underline(Color fg = Color::Default) {
        Style s; s.fg = fg; s.is_underline = true; return s;
    }
    static Style reverse() {
        Style s; s.is_reverse = true; return s;
    }

    std::string begin() const {
        std::string seq;
        seq.reserve(16);
        seq = "\033[0";
        if (is_bold)      seq += ";1";
        if (is_underline) seq += ";4";
        if (is_reverse)   seq += ";7";
        if (fg != Color::Default) seq += ";" + std::to_string(static_cast<int>(fg));
        // ANSI background codes are foreground + 10: standard 30-37 → 40-47,
        // bright 90-97 → 100-107.  The +10 offset holds for both ranges.
        if (bg != Color::Default) seq += ";" + std::to_string(static_cast<int>(bg) + 10);
        seq += "m";
        return seq;
    }

    static std::string reset() { return "\033[0m"; }
};

// ─── UTF-8 Helpers ──────────────────────────────────────────────────────────

// Returns the display width (column count) of a UTF-8 string.
// Assumes all codepoints are single-width (no CJK fullwidth).
inline size_t utf8_display_width(const std::string& s) {
    size_t width = 0;
    size_t i = 0;
    const size_t len = s.size();
    while (i < len) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t char_len = 1;
        if      (c >= 0x80) {
            if      ((c & 0xE0) == 0xC0) char_len = 2;
            else if ((c & 0xF0) == 0xE0) char_len = 3;
            else                          char_len = 4;
        }
        if (i + char_len > len) break; // truncated sequence
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
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t char_len = 1;
        if (c >= 0x80) {
            if      ((c & 0xE0) == 0xC0) char_len = 2;
            else if ((c & 0xF0) == 0xE0) char_len = 3;
            else                          char_len = 4;
        }
        if (i + char_len > len) break; // truncated sequence
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

    Text& add(const std::string& content, const Style& s = Style()) {
        spans_.push_back(TextSpan(content, s));
        return *this;
    }

    // Renders the text to an ANSI escape sequence string.
    std::string render() const {
        std::string out;
        out.reserve(spans_.size() * 32);
        for (const TextSpan& span : spans_) {
            out += span.style.begin();
            out += span.content;
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
        header_style_.is_bold = true;
        header_style_.is_underline = true;
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
                    widths[c] = std::max(1, (widths[c] * usable + total / 2) / total);
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
        char ch = ir.Event.KeyEvent.uChar.AsciiChar;
        if (vk == VK_RETURN) return KEY_ENTER;
        if (ch == 'q' || ch == 'Q') return KEY_QUIT;
        if (ch == 3) return KEY_CTRL_C;
        switch (vk) {
            case VK_LEFT:  return KEY_LEFT;
            case VK_RIGHT: return KEY_RIGHT;
            case VK_UP:    return KEY_UP;
            case VK_DOWN:  return KEY_DOWN;
        }
        if (ch != 0) return KEY_OTHER;
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
    tcgetattr(STDIN_FILENO, &orig_termios_ref());
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

    if (c == 13)                   return KEY_ENTER;
    if (c == 'q' || c == 'Q')     return KEY_QUIT;
    if (c == 3)                    return KEY_CTRL_C;

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
                unsigned char drain;
                while (read(STDIN_FILENO, &drain, 1) > 0) {
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
        : cursor_(0), cursor_style_(Style::reverse()) {}

    SelectableList& add_item(const std::string& item) {
        items_.push_back(item);
        return *this;
    }

    SelectableList& on_select(std::function<void(int, const std::string&)> cb) {
        on_select_ = cb;
        return *this;
    }

    SelectableList& normal_style(const Style& s) { normal_style_ = s; return *this; }
    SelectableList& cursor_style(const Style& s) { cursor_style_ = s; return *this; }

    int cursor() const { return cursor_; }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    const std::string& selected_item() const {
        static const std::string empty;
        if (items_.empty()) return empty;
        return items_[static_cast<size_t>(cursor_)];
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
            if (on_select_) on_select_(cursor_, items_[static_cast<size_t>(cursor_)]);
            return true;
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
    std::vector<std::string> items_;
    int cursor_;
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

    void clear() { lines_.clear(); scroll_ = 0; }

    void set_list(const SelectableList& list) {
        list_ = list;
        has_list_ = true;
    }

    bool has_list() const { return has_list_; }
    SelectableList& list() { return list_; }
    const SelectableList& list() const { return list_; }

    void scroll_up(int n = 1) {
        scroll_ = std::max(0, scroll_ - n);
    }

    void scroll_down(int n, int visible_rows) {
        int max_scroll = std::max(0, total_lines() - visible_rows);
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
        : title_(title), active_tab_(0), running_(false) {}

    Page& add_page(const std::string& name) {
        pages_.push_back(Page(name));
        return pages_.back();
    }

    Page& page(size_t index) { return pages_[index]; }
    size_t page_count() const { return pages_.size(); }
    size_t active_tab() const { return active_tab_; }

    void run() {
        if (pages_.empty()) return;
        install_signals();
        detail::enter_raw_mode();
        detail::hide_cursor();
        running_ = true;

        render();
        while (running_) {
            detail::Key key = detail::read_key();
            handle_key(key);
        }

        detail::show_cursor();
        detail::clear_screen();
        detail::move_cursor(0, 0);
        detail::exit_raw_mode();
    }

private:
    std::string title_;
    std::vector<Page> pages_;
    size_t active_tab_;
    bool running_;

    void install_signals() {
#ifndef _WIN32
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));

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
            if (active_tab_ > 0) { --active_tab_; render(); }
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
            const int content_rows = ts.rows - 3;
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

        // Build tab bar: escape sequences and plain-text width in one pass.
        std::string tab_str;
        size_t tab_plain_len = 0;
        for (size_t i = 0; i < pages_.size(); ++i) {
            const std::string& tab_title = pages_[i].title();
            if (i == active_tab_) {
                tab_str += "\033[1;7m " + tab_title + " \033[0m";
            } else {
                tab_str += "\033[0m " + tab_title + " ";
            }
            tab_plain_len += utf8_display_width(tab_title) + 2; // leading space + title + trailing space
            if (i + 1 < pages_.size()) {
                tab_str += "\033[90m|\033[0m";
                tab_plain_len += 1; // separator
            }
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
                buf += ' ';
                buf += line.render();
                const int pad = W - 3 - static_cast<int>(plain_len);
                if (pad > 0) buf += std::string(static_cast<size_t>(pad), ' ');
            } else {
                buf += std::string(static_cast<size_t>(W - 2), ' ');
            }

            buf += "\033[90m\xe2\x94\x82\033[0m"; // right border
        }

        // Bottom border with status hints and scroll position.
        buf += "\033[";
        buf += std::to_string(H - 1);
        buf += ";1H";

        const char* status_hint = p.has_list()
            ? " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] select  [Enter] choose "
            : " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] scroll ";

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

} // namespace termui

#endif // TERMUI_HPP
