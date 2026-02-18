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
#include <chrono>

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

    Style(Color fg_color)
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
        std::string seq = "\033[0";
        if (is_bold) seq += ";1";
        if (is_underline) seq += ";4";
        if (is_reverse) seq += ";7";
        if (fg != Color::Default) seq += ";" + std::to_string(static_cast<int>(fg));
        if (bg != Color::Default) seq += ";" + std::to_string(static_cast<int>(bg) + 10);
        seq += "m";
        return seq;
    }

    static std::string reset() { return "\033[0m"; }
};

// ─── UTF-8 Helpers ──────────────────────────────────────────────────────────

// Returns the display width (number of columns) of a UTF-8 string.
// Assumes all codepoints used in termui are single-width (no CJK fullwidth).
inline size_t utf8_display_width(const std::string& s) {
    size_t width = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { ++i; }
        else if ((c & 0xE0) == 0xC0) { i += 2; }
        else if ((c & 0xF0) == 0xE0) { i += 3; }
        else { i += 4; }
        ++width;
    }
    return width;
}

// Truncate a UTF-8 string to at most max_width display columns.
// Returns the truncated string (no padding).
inline std::string utf8_truncate(const std::string& s, size_t max_width) {
    size_t width = 0;
    size_t i = 0;
    while (i < s.size() && width < max_width) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t char_len = 1;
        if (c >= 0x80) {
            if ((c & 0xE0) == 0xC0) char_len = 2;
            else if ((c & 0xF0) == 0xE0) char_len = 3;
            else char_len = 4;
        }
        i += char_len;
        ++width;
    }
    return s.substr(0, i);
}

// ─── Text ───────────────────────────────────────────────────────────────────

struct TextSpan {
    std::string content;
    Style style;

    TextSpan() {}
    TextSpan(const std::string& text) : content(text) {}
    TextSpan(const std::string& text, const Style& s) : content(text), style(s) {}
};

class Text {
public:
    Text() {}
    Text(const std::string& content) { spans_.push_back(TextSpan(content)); }
    Text(const std::string& content, const Style& s) { spans_.push_back(TextSpan(content, s)); }

    Text& add(const std::string& content, const Style& s = Style()) {
        spans_.push_back(TextSpan(content, s));
        return *this;
    }

    std::string render() const {
        std::string out;
        for (size_t i = 0; i < spans_.size(); ++i) {
            out += spans_[i].style.begin();
            out += spans_[i].content;
            out += Style::reset();
        }
        return out;
    }

    size_t length() const {
        size_t len = 0;
        for (size_t i = 0; i < spans_.size(); ++i)
            len += utf8_display_width(spans_[i].content);
        return len;
    }

private:
    std::vector<TextSpan> spans_;
};

// ─── ProgressBar ────────────────────────────────────────────────────────────

class ProgressBar {
public:
    explicit ProgressBar(double value = 0.0)
        : value_(value), filled_style_(Color::Green),
          empty_style_(Color::BrightBlack), show_percent_(true) {}

    void set_value(double v) {
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        value_ = v;
    }

    double value() const { return value_; }

    ProgressBar& filled_style(const Style& s) { filled_style_ = s; return *this; }
    ProgressBar& empty_style(const Style& s) { empty_style_ = s; return *this; }
    ProgressBar& show_percent(bool v) { show_percent_ = v; return *this; }

    Text to_text(int width) const {
        // Format: [████░░░░░░] 45%
        std::string pct_str;
        if (show_percent_) {
            int pct = static_cast<int>(value_ * 100.0 + 0.5);
            pct_str = " " + std::to_string(pct) + "%";
        }
        int bar_width = width - 2 - static_cast<int>(pct_str.size()); // subtract [ ]
        if (bar_width < 1) bar_width = 1;

        int filled = static_cast<int>(value_ * bar_width + 0.5);
        if (filled > bar_width) filled = bar_width;
        int empty = bar_width - filled;

        std::string filled_str, empty_str;
        for (int i = 0; i < filled; ++i)
            filled_str += "\xe2\x96\x88"; // U+2588 █
        for (int i = 0; i < empty; ++i)
            empty_str += "\xe2\x96\x91";  // U+2591 ░

        Text t;
        t.add("[");
        if (!filled_str.empty()) t.add(filled_str, filled_style_);
        if (!empty_str.empty()) t.add(empty_str, empty_style_);
        t.add("]");
        if (show_percent_) t.add(pct_str);
        return t;
    }

private:
    double value_;
    Style filled_style_;
    Style empty_style_;
    bool show_percent_;
};

// ─── Table ──────────────────────────────────────────────────────────────────

class Table {
public:
    struct Column {
        std::string name;
        int width; // 0 = auto
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

        // Calculate column widths
        std::vector<int> widths(columns_.size(), 0);
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (columns_[c].width > 0) {
                widths[c] = columns_[c].width;
            } else {
                widths[c] = static_cast<int>(columns_[c].name.size());
                for (size_t r = 0; r < rows_.size(); ++r) {
                    if (c < rows_[r].size()) {
                        int len = static_cast<int>(rows_[r][c].size());
                        if (len > widths[c]) widths[c] = len;
                    }
                }
            }
        }

        // Scale to available_width if given
        if (available_width > 0) {
            int total = 0;
            int separators = static_cast<int>(columns_.size()) - 1;
            for (size_t c = 0; c < widths.size(); ++c) total += widths[c];
            int usable = available_width - separators * 3; // " | " between cols
            if (usable > 0 && total > usable) {
                for (size_t c = 0; c < widths.size(); ++c) {
                    widths[c] = std::max(1, widths[c] * usable / total);
                }
            }
        }

        // Render header
        Text header;
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (c > 0) header.add(" \xe2\x94\x82 ", Style(Color::BrightBlack));
            header.add(pad_or_truncate(columns_[c].name, widths[c]), header_style_);
        }
        result.push_back(header);

        // Separator line
        Text sep;
        for (size_t c = 0; c < columns_.size(); ++c) {
            if (c > 0) sep.add("\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80",
                               Style(Color::BrightBlack));
            std::string dash;
            for (int i = 0; i < widths[c]; ++i) dash += "\xe2\x94\x80";
            sep.add(dash, Style(Color::BrightBlack));
        }
        result.push_back(sep);

        // Data rows
        for (size_t r = 0; r < rows_.size(); ++r) {
            Text row;
            for (size_t c = 0; c < columns_.size(); ++c) {
                if (c > 0) row.add(" \xe2\x94\x82 ", Style(Color::BrightBlack));
                std::string cell = (c < rows_[r].size()) ? rows_[r][c] : "";
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

    static std::string pad_or_truncate(const std::string& s, int width) {
        if (width <= 0) return "";
        int len = static_cast<int>(utf8_display_width(s));
        if (len <= width) {
            return s + std::string(width - len, ' ');
        }
        if (width <= 1) return "\xe2\x80\xa6"; // …
        return utf8_truncate(s, width - 1) + "\xe2\x80\xa6";
    }
};

// Page and SelectableList are defined after the detail namespace (SelectableList needs detail::Key)

// ─── Platform Detail ────────────────────────────────────────────────────────

namespace detail {

enum Key {
    KEY_NONE = 0,
    KEY_QUIT,       // 'q'
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

static HANDLE hStdout = INVALID_HANDLE_VALUE;
static HANDLE hStdin = INVALID_HANDLE_VALUE;
static DWORD orig_out_mode = 0;
static DWORD orig_in_mode = 0;
static UINT orig_cp = 0;

inline void enter_raw_mode() {
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdout, &orig_out_mode);
    GetConsoleMode(hStdin, &orig_in_mode);
    DWORD out_mode = orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdout, out_mode);
    DWORD in_mode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(hStdin, in_mode);
    orig_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(65001);
}

inline void exit_raw_mode() {
    SetConsoleMode(hStdout, orig_out_mode);
    SetConsoleMode(hStdin, orig_in_mode);
    SetConsoleOutputCP(orig_cp);
}

inline TermSize get_terminal_size() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    TermSize ts;
    ts.cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    ts.rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return ts;
}

inline Key read_key() {
    INPUT_RECORD ir;
    DWORD count;
    DWORD wait_result = WaitForSingleObject(hStdin, 100);
    if (wait_result != WAIT_OBJECT_0) return KEY_NONE;
    while (true) {
        if (!PeekConsoleInput(hStdin, &ir, 1, &count) || count == 0)
            return KEY_NONE;
        ReadConsoleInput(hStdin, &ir, 1, &count);
        if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) return KEY_RESIZE;
        if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
        WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
        char ch = ir.Event.KeyEvent.uChar.AsciiChar;
        if (vk == VK_RETURN) return KEY_ENTER;
        if (ch == 'q' || ch == 'Q') return KEY_QUIT;
        if (ch == 3) return KEY_CTRL_C; // Ctrl+C
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

static struct termios orig_termios;
static bool raw_mode_active = false;

inline void exit_raw_mode() {
    if (raw_mode_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = false;
    }
}

inline void enter_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = true;
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

static volatile sig_atomic_t g_resize_flag = 0;

inline Key read_key() {
    if (g_resize_flag) {
        g_resize_flag = 0;
        return KEY_RESIZE;
    }

    unsigned char c;
    int n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return KEY_NONE;

    if (c == 13) return KEY_ENTER;  // Enter / CR
    if (c == 'q' || c == 'Q') return KEY_QUIT;
    if (c == 3) return KEY_CTRL_C; // Ctrl+C

    if (c == 27) { // ESC sequence
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return KEY_OTHER;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return KEY_OTHER;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
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
    WriteConsoleA(hStdout, s.c_str(), static_cast<DWORD>(s.size()), &written, NULL);
#else
    ::write(STDOUT_FILENO, s.c_str(), s.size());
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

    const std::string& selected_item() const { return items_[cursor_]; }

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
            if (on_select_) on_select_(cursor_, items_[cursor_]);
            return true;
        default:
            return false;
        }
    }

    std::vector<Text> render(int width) const {
        std::vector<Text> result;
        for (size_t i = 0; i < items_.size(); ++i) {
            bool is_cur = (static_cast<int>(i) == cursor_);
            std::string prefix = is_cur ? "> " : "  ";
            const Style& st = is_cur ? cursor_style_ : normal_style_;

            std::string content = prefix + items_[i];
            if (width > 0 && static_cast<int>(content.size()) > width)
                content = content.substr(0, width);

            result.push_back(Text(content, st));
        }
        return result;
    }

    bool empty() const { return items_.empty(); }

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
        int total = total_lines();
        int max_scroll = std::max(0, total - visible_rows);
        scroll_ = std::min(scroll_ + n, max_scroll);
    }

    int scroll_offset() const { return scroll_; }
    const std::vector<Text>& lines() const { return lines_; }

    int total_lines() const {
        int count = static_cast<int>(lines_.size());
        if (has_list_) {
            std::vector<Text> list_lines = list_.render(0);
            count += static_cast<int>(list_lines.size());
        }
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
        : title_(title), active_tab_(0), running_(false),
          refresh_interval_ms_(0) {}

    Page& add_page(const std::string& name) {
        pages_.push_back(Page(name));
        return pages_.back();
    }

    Page& page(size_t index) { return pages_[index]; }
    size_t page_count() const { return pages_.size(); }
    size_t active_tab() const { return active_tab_; }

    void set_on_refresh(std::function<void(App&)> callback, int interval_ms = 1000) {
        refresh_callback_ = callback;
        refresh_interval_ms_ = interval_ms;
        last_refresh_ = std::chrono::steady_clock::now();
    }

    void run() {
        if (pages_.empty()) return;
        install_signals();
        detail::enter_raw_mode();
        detail::hide_cursor();
        running_ = true;
        last_refresh_ = std::chrono::steady_clock::now();

        render();
        while (running_) {
            detail::Key key = detail::read_key();
            handle_key(key);

            // Live refresh check
            if (refresh_callback_ && refresh_interval_ms_ > 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_refresh_).count();
                if (elapsed >= refresh_interval_ms_) {
                    refresh_callback_(*this);
                    last_refresh_ = now;
                    render();
                }
            }
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
    std::function<void(App&)> refresh_callback_;
    int refresh_interval_ms_;
    std::chrono::steady_clock::time_point last_refresh_;

    void install_signals() {
#ifndef _WIN32
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));

        sa.sa_handler = [](int) { detail::g_resize_flag = 1; };
        sigaction(SIGWINCH, &sa, NULL);

        sa.sa_handler = [](int) {
            detail::show_cursor();
            detail::clear_screen();
            detail::move_cursor(0, 0);
            detail::exit_raw_mode();
            std::_Exit(0);
        };
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
#endif
    }

    void handle_key(detail::Key key) {
        if (key == detail::KEY_NONE) return;

        // Quit keys always handled first
        if (key == detail::KEY_QUIT || key == detail::KEY_CTRL_C) {
            running_ = false;
            return;
        }
        if (key == detail::KEY_RESIZE) {
            render();
            return;
        }
        // Delegate to active page's list if present
        Page& p = pages_[active_tab_];
        if (p.has_list() && p.list().handle_key(key)) {
            render();
            return;
        }

        // Default key handling
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
            detail::TermSize ts = detail::get_terminal_size();
            int content_rows = ts.rows - 3;
            pages_[active_tab_].scroll_down(1, content_rows);
            render();
            break;
        }
        default:
            break;
        }
    }

    void render() {
        detail::TermSize ts = detail::get_terminal_size();
        int W = ts.cols;
        int H = ts.rows;
        if (W < 10 || H < 5) return;

        std::string buf;
        buf.reserve(4096);

        // Move home instead of clearing to avoid flicker
        buf += "\033[H\033[0m";

        // Build tab labels
        std::string tab_str;
        for (size_t i = 0; i < pages_.size(); ++i) {
            if (i == active_tab_) {
                tab_str += "\033[1;7m " + pages_[i].title() + " \033[0m";
            } else {
                tab_str += "\033[0m " + pages_[i].title() + " ";
            }
            if (i + 1 < pages_.size()) tab_str += "\033[90m|\033[0m";
        }

        // Compute plain length of tabs for padding
        size_t tab_plain_len = 0;
        for (size_t i = 0; i < pages_.size(); ++i) {
            tab_plain_len += pages_[i].title().size() + 2; // space + title + space
            if (i + 1 < pages_.size()) tab_plain_len += 1; // separator
        }

        std::string top_border = "\033[90m\xe2\x94\x8c\xe2\x94\x80\033[0m" + tab_str;
        int remaining = W - 3 - static_cast<int>(tab_plain_len);
        if (remaining > 0) {
            top_border += "\033[90m";
            for (int i = 0; i < remaining; ++i)
                top_border += "\xe2\x94\x80";
            top_border += "\033[0m";
        }
        top_border += "\033[90m\xe2\x94\x90\033[0m";
        buf += top_border;

        // ── Content area ─────────────────────────────────────────
        int content_rows = H - 3; // top border + bottom border + status line
        if (content_rows < 1) content_rows = 1;

        Page& p = pages_[active_tab_];
        int scroll = p.scroll_offset();

        // Build combined lines: static lines + list lines
        std::vector<Text> all_lines(p.lines().begin(), p.lines().end());
        if (p.has_list()) {
            std::vector<Text> list_lines = p.list().render(W - 4);
            all_lines.insert(all_lines.end(), list_lines.begin(), list_lines.end());
        }
        int total_lines = static_cast<int>(all_lines.size());

        for (int row = 0; row < content_rows; ++row) {
            buf += "\033[" + std::to_string(2 + row) + ";1H";
            buf += "\033[90m\xe2\x94\x82\033[0m"; // left border

            int line_idx = scroll + row;
            if (line_idx < total_lines) {
                const Text& line = all_lines[line_idx];
                std::string rendered = line.render();
                size_t plain_len = line.length();
                buf += " " + rendered;
                int pad = W - 3 - static_cast<int>(plain_len);
                if (pad > 0) buf += std::string(pad, ' ');
            } else {
                buf += std::string(W - 2, ' ');
            }

            buf += "\033[90m\xe2\x94\x82\033[0m"; // right border
        }

        // ── Bottom border with status ────────────────────────────
        buf += "\033[" + std::to_string(H - 1) + ";1H";
        std::string status_hint;
        if (p.has_list())
            status_hint = " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] select  [Enter] choose ";
        else
            status_hint = " [q] quit  [\xe2\x86\x90\xe2\x86\x92] tabs  [\xe2\x86\x91\xe2\x86\x93] scroll ";

        std::string scroll_hint;
        int total = total_lines;
        if (total > content_rows) {
            int end = std::min(scroll + content_rows, total);
            scroll_hint = " " + std::to_string(scroll + 1) + "-"
                + std::to_string(end) + "/" + std::to_string(total) + " ";
        }

        int status_plain_len = static_cast<int>(utf8_display_width(status_hint));
        int scroll_plain_len = static_cast<int>(scroll_hint.size());
        int total_fixed = status_plain_len + scroll_plain_len;
        int left_dash = (W - 2 - total_fixed) / 2;
        int right_dash = W - 2 - total_fixed - left_dash;
        if (left_dash < 0) left_dash = 0;
        if (right_dash < 0) right_dash = 0;

        buf += "\033[90m\xe2\x94\x94";
        for (int i = 0; i < left_dash; ++i) buf += "\xe2\x94\x80";
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
