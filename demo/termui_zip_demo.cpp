#include "termui.hpp"
#ifndef _WIN32
#  include <cstdio>    // popen, pclose, fgets, snprintf
#  include <unistd.h>  // getpid
#endif
#include <cstdlib>     // system

int main() {
    termui::App app("ZIP Browser");

    // Home tab
    termui::Page& home = app.add_page("Home");
    home.add_line(termui::Text("ZIP File Browser Demo",
                               termui::Style::bold(termui::Color::Cyan)));
    home.add_line(termui::Text(""));
    home.add_line(termui::Text("How to use:"));
    home.add_line(termui::Text("  1. Switch to the Files tab (\xe2\x86\x92)"));
    home.add_line(termui::Text("  2. Navigate to a .zip file and press Enter"));
    home.add_line(termui::Text("  3. The ZIP Contents tab will appear automatically"));
    home.add_line(termui::Text("  4. Press Space to toggle file selection, Enter on \xe2\x86\x92 Send Selected to confirm"));
    home.add_line(termui::Text("  5. Selected paths appear on the Results tab"));
    home.add_line(termui::Text(""));
    home.add_line(termui::Text("Press q to quit.",
                               termui::Style(termui::Color::BrightBlack)));

    // State for dynamically created tabs
    termui::Page* zip_page    = nullptr;
    termui::Page* result_page = nullptr;
    size_t zip_tab_idx    = 0;
    size_t result_tab_idx = 0;

    // Files tab via FileBrowser
    termui::FileBrowser browser(".");

#ifndef _WIN32
    browser.on_file_selected([&](const std::string& path) {
        // Only handle .zip files
        if (path.size() < 4 || path.compare(path.size() - 4, 4, ".zip") != 0) return;

        // Build unique temp dir
        char buf[256];
        std::snprintf(buf, sizeof(buf), "/tmp/termui_zip_%d", static_cast<int>(getpid()));
        std::string tmp_dir(buf);

        // Extract
        {
            std::string cmd = "mkdir -p \"" + tmp_dir + "\" && unzip -o \""
                              + path + "\" -d \"" + tmp_dir + "\" > /dev/null 2>&1";
            std::system(cmd.c_str());
        }

        // Collect extracted paths
        std::vector<std::string> extracted;
        {
            std::string find_cmd = "find \"" + tmp_dir + "\" -type f";
            FILE* fp = popen(find_cmd.c_str(), "r");
            if (fp) {
                char line[1024];
                while (std::fgets(line, sizeof(line), fp)) {
                    std::string s(line);
                    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
                    if (!s.empty()) extracted.push_back(s);
                }
                pclose(fp);
            }
        }

        // Create or repopulate ZIP Contents tab
        if (!zip_page) {
            zip_page    = &app.add_page("ZIP Contents");
            zip_tab_idx = app.page_count() - 1;
        } else {
            zip_page->clear();
        }

        zip_page->add_line(termui::Text("ZIP Contents",
                                        termui::Style::bold(termui::Color::Cyan)));
        zip_page->add_line(termui::Text("Source: " + path,
                                        termui::Style(termui::Color::BrightBlack)));
        zip_page->add_line(termui::Text(""));
        zip_page->add_line(termui::Text(
            std::to_string(extracted.size()) + " file(s)  \xe2\x80\x94"
            "  Space to toggle, Enter on \xe2\x86\x92 Send Selected to confirm.",
            termui::Style(termui::Color::BrightBlack)));
        zip_page->add_line(termui::Text(""));

        // Build multi-select list
        termui::SelectableList zip_list;
        zip_list.set_multi_select(true);
        for (size_t i = 0; i < extracted.size(); ++i)
            zip_list.add_item(extracted[i]);

        // "Send Selected" action — captures tmp_dir by value (outer lambda has returned)
        zip_list.add_item("\xe2\x86\x92 Send Selected",
            [&app, &zip_page, &result_page, &result_tab_idx, tmp_dir]() {
                // Collect checked items; exclude the "→ Send Selected" label itself
                std::vector<std::string> all = zip_page->list().get_selected_items();
                const std::string SEND_LABEL = "\xe2\x86\x92 Send Selected";
                std::vector<std::string> sel;
                for (size_t j = 0; j < all.size(); ++j)
                    if (all[j] != SEND_LABEL) sel.push_back(all[j]);

                // Create or repopulate Results tab
                if (!result_page) {
                    result_page    = &app.add_page("Results");
                    result_tab_idx = app.page_count() - 1;
                } else {
                    result_page->clear();
                }

                result_page->add_line(termui::Text("Selected Files",
                                                   termui::Style::bold(termui::Color::Green)));
                result_page->add_line(termui::Text(""));
                if (sel.empty()) {
                    result_page->add_line(termui::Text(
                        "  (no files selected)", termui::Style(termui::Color::BrightBlack)));
                } else {
                    for (size_t j = 0; j < sel.size(); ++j)
                        result_page->add_line(
                            termui::Text("  \xe2\x80\xa2 ", termui::Style(termui::Color::Cyan))
                                .add(sel[j], termui::Style()));
                }
                result_page->add_line(termui::Text(""));
                result_page->add_line(termui::Text(
                    std::to_string(sel.size()) + " file(s) selected.",
                    termui::Style(termui::Color::BrightBlack)));

                app.set_active_tab(result_tab_idx);
            });

        zip_page->set_list(zip_list);
        app.set_active_tab(zip_tab_idx);
    });
#endif

    browser.attach(app, "Files");

    app.run();
    return 0;
}
