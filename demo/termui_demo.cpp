#include "../termui.hpp"

int main() {
    termui::App app("TermUI Demo");

    // ── Tab 1: Dashboard ─────────────────────────────────────────
    auto& dashboard = app.add_page("Dashboard");
    dashboard.add_line(termui::Text("Dashboard", termui::Style().bold().fg(termui::Color::Cyan)));
    dashboard.add_blank();
    dashboard.add_line(termui::Text("System Status", termui::Style().underline()));
    dashboard.add_blank();
    dashboard.add_line(termui::Text("  Service:   ", termui::Style(termui::Color::BrightBlack))
                           .add("Running", termui::Style(termui::Color::Green)));
    dashboard.add_line(termui::Text("  Uptime:    ", termui::Style(termui::Color::BrightBlack))
                           .add("14 days, 3 hours", termui::Style()));
    dashboard.add_line(termui::Text("  Version:   ", termui::Style(termui::Color::BrightBlack))
                           .add("1.0.0", termui::Style()));

    // ── Tab 2: Actions — per-item lambdas ───────────────────────
    auto& actions_page = app.add_page("Actions");

    auto rebuild = [&](termui::Text result) {
        actions_page.clear();
        actions_page.add_line(termui::Text("Actions Demo", termui::Style().bold().fg(termui::Color::Yellow)));
        actions_page.add_blank();
        actions_page.add_line(termui::Text("Press Enter on an item:"));
        actions_page.add_blank();
        actions_page.add_blank();
        actions_page.add_line(result);
    };

    termui::SelectableList action_menu;
    action_menu
        .add_item("Say hello", [&]() {
            rebuild(termui::Text("  Hello, World!", termui::Style(termui::Color::Green)));
        })
        .add_item("Show a warning", [&]() {
            rebuild(termui::Text("  Warning: something might go wrong.", termui::Style(termui::Color::Yellow)));
        })
        .add_item("Report an error", [&]() {
            rebuild(termui::Text("  Error: something went wrong!", termui::Style(termui::Color::Red)));
        })
        .add_item("Celebrate!", [&]() {
            rebuild(termui::Text("  *** Great job! ***", termui::Style().bold().fg(termui::Color::Cyan)));
        });

    rebuild(termui::Text("  (nothing selected yet)", termui::Style(termui::Color::BrightBlack)));
    actions_page.set_list(action_menu);

    // ── Tab 3: Data — table display ─────────────────────────────
    auto& data = app.add_page("Data");
    data.add_line(termui::Text("Sample Data Table", termui::Style().bold().fg(termui::Color::Magenta)));
    data.add_blank();

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

    data.add_lines(table.render());

    // ── Tab 4: Scroll Test ────────────────────────────────────────
    auto& scroll = app.add_page("Scroll");
    scroll.add_line(termui::Text("This page has many lines to test scrolling.", termui::Style().bold()));
    scroll.add_blank();
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
    about.add_line(termui::Text("TermUI - Terminal GUI Framework", termui::Style().bold().fg(termui::Color::Cyan)));
    about.add_blank();
    about.add_line(termui::Text("Header-only, C++11, no dependencies."));
    about.add_line(termui::Text("Cross-platform: Linux, macOS, Windows."));
    about.add_blank();
    about.add_line(termui::Text("Features:", termui::Style().underline()));
    about.add_line(termui::Text("  - Tabbed pages with styled text"));
    about.add_line(termui::Text("  - Selectable lists with callbacks"));
    about.add_line(termui::Text("  - Formatted tables"));
    about.add_line(termui::Text("  - Scrollable content"));
    about.add_line(termui::Text("  - Box-drawing borders"));

    // ── Tab 6: Live — animated progress bar via on_tick ──────────
    auto& live = app.add_page("Live");

    // State for the animated bar: progress steps from 0.0 to 1.0 then loops.
    termui::ProgressBar bar;
    bar.set_fill_color(termui::Color::Green)
       .set_empty_color(termui::Color::BrightBlack);

    double progress = 0.0;

    // Seed the page with its initial content so it is non-empty before run().
    auto rebuild_live = [&]() {
        live.clear();
        live.add_line(termui::Text("Live Update Demo", termui::Style().bold().fg(termui::Color::Green)));
        live.add_blank();
        live.add_line(termui::Text("Progress bar animates every ~100 ms:",
                                   termui::Style(termui::Color::BrightBlack)));
        live.add_blank();
        live.add_line(bar.render(30));
        live.add_blank();

        // Descriptive label that changes with progress.
        if (progress < 0.33)
            live.add_line(termui::Text("  Starting up...", termui::Style(termui::Color::Yellow)));
        else if (progress < 0.67)
            live.add_line(termui::Text("  In progress...", termui::Style(termui::Color::Cyan)));
        else if (progress < 1.0)
            live.add_line(termui::Text("  Almost there!", termui::Style(termui::Color::BrightCyan)));
        else
            live.add_line(termui::Text("  Complete!", termui::Style().bold().fg(termui::Color::Green)));
    };

    rebuild_live();

    // Tick callback: advance progress by ~2% per tick (100 ms), loop at 100%.
    app.set_on_tick([&]() {
        progress += 0.02;
        if (progress > 1.0) progress = 0.0;
        bar.set_value(progress);
        rebuild_live();
    });

    // ── Tab 7: Files — interactive file browser ───────────────────
    termui::FileBrowser browser(".");
    browser.on_file_selected([](const std::string& /*path*/) {
        // full path available here for integration with other code
    });
    browser.attach(app, "Files");

    // ── Tab 8: Logs ───────────────────────────────────────────────
    auto& logs = app.add_page("Logs");
    logs.add_line(termui::Text("Application Logs", termui::Style().bold().fg(termui::Color::Yellow)));
    logs.add_blank();
    logs.add_line(termui::Text("[INFO]  Service started successfully.", termui::Style(termui::Color::Green)));
    logs.add_line(termui::Text("[WARN]  High memory usage detected.", termui::Style(termui::Color::Yellow)));
    logs.add_line(termui::Text("[ERROR] Connection timeout on port 8080.", termui::Style(termui::Color::Red)));

    // ── Tab 9: Config ─────────────────────────────────────────────
    auto& config = app.add_page("Config");
    config.add_line(termui::Text("Configuration", termui::Style().bold().fg(termui::Color::Cyan)));
    config.add_blank();
    config.add_line(termui::Text("  host:    localhost"));
    config.add_line(termui::Text("  port:    8080"));
    config.add_line(termui::Text("  debug:   false"));

    // ── Tab 10: Network ───────────────────────────────────────────
    auto& network = app.add_page("Network");
    network.add_line(termui::Text("Network Status", termui::Style().bold().fg(termui::Color::Blue)));
    network.add_blank();
    network.add_line(termui::Text("  Interface:  eth0"));
    network.add_line(termui::Text("  IP:         192.168.1.100"));
    network.add_line(termui::Text("  Latency:    12 ms"));

    // ── Tab 11: Metrics ───────────────────────────────────────────
    auto& metrics = app.add_page("Metrics");
    metrics.add_line(termui::Text("Metrics", termui::Style().bold().fg(termui::Color::Magenta)));
    metrics.add_blank();
    metrics.add_line(termui::Text("  CPU:     42%"));
    metrics.add_line(termui::Text("  Memory:  68%"));
    metrics.add_line(termui::Text("  Disk:    55%"));

    // ── Tab 12: Alerts ────────────────────────────────────────────
    auto& alerts = app.add_page("Alerts");
    alerts.add_line(termui::Text("Active Alerts", termui::Style().bold().fg(termui::Color::Red)));
    alerts.add_blank();
    alerts.add_line(termui::Text("  [!] CPU spike at 14:32", termui::Style(termui::Color::Yellow)));
    alerts.add_line(termui::Text("  [!] Disk usage above 90%", termui::Style(termui::Color::Red)));

    // ── Tab 13: Users ─────────────────────────────────────────────
    auto& users = app.add_page("Users");
    users.add_line(termui::Text("Active Users", termui::Style().bold().fg(termui::Color::Cyan)));
    users.add_blank();
    users.add_line(termui::Text("  alice   (admin)"));
    users.add_line(termui::Text("  bob     (user)"));
    users.add_line(termui::Text("  charlie (user)"));

    // ── Tab 14: Events ────────────────────────────────────────────
    auto& events = app.add_page("Events");
    events.add_line(termui::Text("Event Stream", termui::Style().bold().fg(termui::Color::BrightBlack)));
    events.add_blank();
    events.add_line(termui::Text("  14:31 — deploy started"));
    events.add_line(termui::Text("  14:33 — health check passed"));
    events.add_line(termui::Text("  14:35 — deploy complete"));

    // ── Tab 15: Help ──────────────────────────────────────────────
    auto& help_page = app.add_page("Help");
    help_page.add_line(termui::Text("Help & Shortcuts", termui::Style().bold().fg(termui::Color::BrightWhite)));
    help_page.add_blank();
    help_page.add_line(termui::Text("  \xe2\x86\x90 \xe2\x86\x92     Switch tabs"));
    help_page.add_line(termui::Text("  \xe2\x86\x91 \xe2\x86\x93     Scroll / select"));
    help_page.add_line(termui::Text("  Enter   Confirm selection"));
    help_page.add_line(termui::Text("  q       Quit"));

    app.run();
    return 0;
}
