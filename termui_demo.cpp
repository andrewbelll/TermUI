#include "termui.hpp"
#include <cmath>

int main() {
    termui::App app("TermUI Demo");

    // ── Tab 1: Dashboard — live-updating progress bars ──────────
    auto& dashboard = app.add_page("Dashboard");
    dashboard.add_line(termui::Text("Live Dashboard", termui::Style::bold(termui::Color::Cyan)));
    dashboard.add_line(termui::Text(""));
    dashboard.add_line(termui::Text("Progress bars update automatically:"));
    dashboard.add_line(termui::Text(""));
    // Placeholder lines for progress bars (indices 4, 5, 6 — label + bar pairs)
    dashboard.add_line(termui::Text("  Download:", termui::Style(termui::Color::Green)));
    dashboard.add_line(termui::Text(""));  // bar placeholder
    dashboard.add_line(termui::Text("  Upload:", termui::Style(termui::Color::Yellow)));
    dashboard.add_line(termui::Text(""));  // bar placeholder
    dashboard.add_line(termui::Text("  Processing:", termui::Style(termui::Color::Magenta)));
    dashboard.add_line(termui::Text(""));  // bar placeholder

    int tick = 0;
    app.set_on_refresh([&tick](termui::App& a) {
        ++tick;
        auto& p = a.page(0);

        double v1 = std::fmod(tick * 0.02, 1.0);
        double v2 = std::fmod(tick * 0.015, 1.0);
        double v3 = std::fmod(tick * 0.03, 1.0);

        termui::ProgressBar bar1(v1);
        termui::ProgressBar bar2(v2);
        bar2.filled_style(termui::Style(termui::Color::Yellow));
        termui::ProgressBar bar3(v3);
        bar3.filled_style(termui::Style(termui::Color::Magenta));

        // Replace lines at indices 5, 7, 9 (the bar placeholders)
        p.clear();
        p.add_line(termui::Text("Live Dashboard", termui::Style::bold(termui::Color::Cyan)));
        p.add_line(termui::Text(""));
        p.add_line(termui::Text("Progress bars update automatically:"));
        p.add_line(termui::Text(""));
        p.add_line(termui::Text("  Download:", termui::Style(termui::Color::Green)));
        p.add_line(bar1.to_text(40));
        p.add_line(termui::Text("  Upload:", termui::Style(termui::Color::Yellow)));
        p.add_line(bar2.to_text(40));
        p.add_line(termui::Text("  Processing:", termui::Style(termui::Color::Magenta)));
        p.add_line(bar3.to_text(40));
    }, 200);

    // ── Tab 2: Settings — selectable list ───────────────────────
    auto& settings = app.add_page("Settings");
    settings.add_line(termui::Text("Settings", termui::Style::bold(termui::Color::Yellow)));
    settings.add_line(termui::Text(""));
    settings.add_line(termui::Text("Use Up/Down to navigate, Enter to select:"));
    settings.add_line(termui::Text(""));

    termui::SelectableList menu;
    menu.add_item("Theme: Dark")
        .add_item("Language: English")
        .add_item("Notifications: On")
        .add_item("Auto-save: Enabled")
        .add_item("Font size: Medium")
        .add_item("Reset to defaults");

    // Status line will be appended after the list
    settings.add_line(termui::Text(""));
    settings.add_line(termui::Text("  (no selection yet)", termui::Style(termui::Color::BrightBlack)));

    menu.on_select([&settings](int idx, const std::string& item) {
        // Rebuild the status lines at the end
        settings.clear();
        settings.add_line(termui::Text("Settings", termui::Style::bold(termui::Color::Yellow)));
        settings.add_line(termui::Text(""));
        settings.add_line(termui::Text("Use Up/Down to navigate, Enter to select:"));
        settings.add_line(termui::Text(""));
        settings.add_line(termui::Text(""));
        termui::Text status;
        status.add("  Selected: ", termui::Style::bold())
              .add("[" + std::to_string(idx) + "] " + item, termui::Style(termui::Color::Green));
        settings.add_line(status);
    });

    settings.set_list(menu);

    // ── Tab 3: Data — table display ─────────────────────────────
    auto& data = app.add_page("Data");
    data.add_line(termui::Text("Sample Data Table", termui::Style::bold(termui::Color::Magenta)));
    data.add_line(termui::Text(""));

    termui::Table table;
    table.add_column("ID", 4)
         .add_column("Name", 14)
         .add_column("Role", 12)
         .add_column("Status", 10);

    table.add_row({"1",  "Alice",   "Engineer",  "Active"});
    table.add_row({"2",  "Bob",     "Designer",  "Away"});
    table.add_row({"3",  "Charlie", "Manager",   "Active"});
    table.add_row({"4",  "Diana",   "Analyst",   "Offline"});
    table.add_row({"5",  "Eve",     "DevOps",    "Active"});
    table.add_row({"6",  "Frank",   "QA Lead",   "Active"});
    table.add_row({"7",  "Grace",   "Intern",    "Away"});

    std::vector<termui::Text> table_lines = table.render();
    for (size_t i = 0; i < table_lines.size(); ++i) {
        data.add_line(table_lines[i]);
    }

    // ── Tab 4: Scroll Test ────────────────────────────────────────
    auto& scroll = app.add_page("Scroll");
    scroll.add_line(termui::Text("This page has many lines to test scrolling.", termui::Style::bold()));
    scroll.add_line(termui::Text(""));
    for (int i = 1; i <= 50; ++i) {
        termui::Color c;
        switch (i % 6) {
            case 0: c = termui::Color::Red; break;
            case 1: c = termui::Color::Green; break;
            case 2: c = termui::Color::Yellow; break;
            case 3: c = termui::Color::Blue; break;
            case 4: c = termui::Color::Magenta; break;
            default: c = termui::Color::Cyan; break;
        }
        scroll.add_line(termui::Text("  Line " + std::to_string(i) + " - scroll to see more", termui::Style(c)));
    }

    // ── Tab 5: About ────────────────────────────────────────────
    auto& about = app.add_page("About");
    about.add_line(termui::Text("TermUI - Terminal GUI Framework", termui::Style::bold(termui::Color::Cyan)));
    about.add_line(termui::Text(""));
    about.add_line(termui::Text("Header-only, C++11, no dependencies."));
    about.add_line(termui::Text("Cross-platform: Linux, macOS, Windows."));
    about.add_line(termui::Text(""));
    about.add_line(termui::Text("Features:", termui::Style::underline()));
    about.add_line(termui::Text("  - Tabbed pages with styled text"));
    about.add_line(termui::Text("  - Progress bars with live refresh"));
    about.add_line(termui::Text("  - Selectable lists with callbacks"));
    about.add_line(termui::Text("  - Formatted tables"));
    about.add_line(termui::Text("  - Scrollable content"));
    about.add_line(termui::Text("  - Box-drawing borders"));

    app.run();
    return 0;
}
