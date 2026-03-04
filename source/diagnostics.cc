#include <algorithm>
#include <diagnostics.hh>
#include <format>
#include <iostream>
#include <map>
#include <print>
#include <ranges>
#include <set>
#include <util/utf8.hh>

namespace dcc::diag
{
    std::string_view to_string(Severity s) noexcept
    {
        switch (s)
        {
            case Severity::Error:
                return "error";
            case Severity::Warning:
                return "warning";
            case Severity::Note:
                return "note";
            case Severity::Help:
                return "help";
        }
        return "unknown";
    }

    Diagnostic::Diagnostic(Severity severity, std::string message) : m_severity{severity}, m_message{std::move(message)} {}

    Diagnostic& Diagnostic::with_code(std::string code) &
    {
        m_code = std::move(code);
        return *this;
    }

    Diagnostic&& Diagnostic::with_code(std::string code) &&
    {
        return std::move(with_code(std::move(code)));
    }

    Diagnostic& Diagnostic::with_label(Label label) &
    {
        m_labels.push_back(std::move(label));
        return *this;
    }

    Diagnostic&& Diagnostic::with_label(Label label) &&
    {
        return std::move(with_label(std::move(label)));
    }

    Diagnostic& Diagnostic::with_primary(sm::SourceRange range, std::string message) &
    {
        return with_label({range, std::move(message), LabelStyle::Primary});
    }

    Diagnostic&& Diagnostic::with_primary(sm::SourceRange range, std::string message) &&
    {
        return std::move(with_primary(range, std::move(message)));
    }

    Diagnostic& Diagnostic::with_secondary(sm::SourceRange range, std::string message) &
    {
        return with_label({range, std::move(message), LabelStyle::Secondary});
    }

    Diagnostic&& Diagnostic::with_secondary(sm::SourceRange range, std::string message) &&
    {
        return std::move(with_secondary(range, std::move(message)));
    }

    Diagnostic& Diagnostic::with_note(std::string note) &
    {
        m_notes.push_back(std::move(note));
        return *this;
    }

    Diagnostic&& Diagnostic::with_note(std::string note) &&
    {
        return std::move(with_note(std::move(note)));
    }

    Diagnostic& Diagnostic::with_help(std::string help) &
    {
        m_helps.push_back(std::move(help));
        return *this;
    }

    Diagnostic&& Diagnostic::with_help(std::string help) &&
    {
        return std::move(with_help(std::move(help)));
    }

    Diagnostic& Diagnostic::with_fix(FixIt fix) &
    {
        m_fixes.push_back(std::move(fix));
        return *this;
    }

    Diagnostic&& Diagnostic::with_fix(FixIt fix) &&
    {
        return std::move(with_fix(std::move(fix)));
    }

    Diagnostic error(std::string message)
    {
        return Diagnostic{Severity::Error, std::move(message)};
    }

    Diagnostic warning(std::string message)
    {
        return Diagnostic{Severity::Warning, std::move(message)};
    }

    Diagnostic note(std::string message)
    {
        return Diagnostic{Severity::Note, std::move(message)};
    }

    Diagnostic help(std::string message)
    {
        return Diagnostic{Severity::Help, std::move(message)};
    }

    namespace ansi
    {
        static constexpr std::string_view reset = "\033[0m";
        static constexpr std::string_view bold = "\033[1m";
        static constexpr std::string_view bold_red = "\033[1;91m";
        static constexpr std::string_view bold_yel = "\033[1;93m";
        static constexpr std::string_view bold_blue = "\033[1;94m";
        static constexpr std::string_view bold_cyan = "\033[1;96m";
        static constexpr std::string_view bold_green = "\033[1;92m";
    } // namespace ansi

    void DiagnosticPrinter::style_on(std::string_view code)
    {
        if (m_color)
            std::print(m_os, "{}", code);
    }

    void DiagnosticPrinter::style_off()
    {
        if (m_color)
            std::print(m_os, "{}", ansi::reset);
    }

    void DiagnosticPrinter::bold_on()
    {
        if (m_color)
            std::print(m_os, "{}", ansi::bold);
    }

    std::string_view DiagnosticPrinter::severity_color(Severity s) const noexcept
    {
        switch (s)
        {
            case Severity::Error:
                return ansi::bold_red;
            case Severity::Warning:
                return ansi::bold_yel;
            case Severity::Note:
                return ansi::bold_blue;
            case Severity::Help:
                return ansi::bold_cyan;
        }
        return ansi::bold;
    }

    std::string_view DiagnosticPrinter::label_color(LabelStyle s, Severity sev) const noexcept
    {
        if (s == LabelStyle::Primary)
            return severity_color(sev);

        return ansi::bold_blue;
    }

    DiagnosticPrinter::DiagnosticPrinter(const sm::SourceManager& sm, std::ostream& os) : m_sm{sm}, m_os{os} {}

    int DiagnosticPrinter::digit_count(uint32_t n) noexcept
    {
        if (n == 0)
            return 1;

        int count = 0;
        while (n > 0)
        {
            ++count;
            n /= 10;
        }
        return count;
    }

    std::string DiagnosticPrinter::expand_tabs(std::string_view line) const
    {
        std::string result;
        result.reserve(line.size());
        uint32_t col = 0;
        for (char c : line)
        {
            if (c == '\t')
            {
                uint32_t spaces = m_tab_width - (col % m_tab_width);
                result.append(spaces, ' ');
                col += spaces;
            }
            else
            {
                result.push_back(c);
                ++col;
            }
        }
        return result;
    }

    uint32_t DiagnosticPrinter::byte_to_display_col(std::string_view line, uint32_t byte_off) const
    {
        uint32_t col = 0;
        for (auto it = utf8::codepoints(line).begin(), e = utf8::codepoints(line).end(); it != e; ++it)
        {
            if (it.byte_offset() >= byte_off)
                break;
            auto res = *it;
            if (!res)
            {
                ++col;
                continue;
            }

            utf8::Codepoint cp = res->codepoint;
            if (cp == U'\t')
                col += m_tab_width - (col % m_tab_width);
            else
                col += static_cast<uint32_t>(utf8::codepoint_width(cp));
        }

        return col;
    }

    void DiagnosticPrinter::write_gutter(int gutter_w, uint32_t line_no)
    {
        auto num = std::to_string(line_no);
        int pad = gutter_w - static_cast<int>(num.size());
        if (pad < 0)
            pad = 0;

        style_on(ansi::bold_blue);
        m_os << ' ';
        m_os << std::string(static_cast<std::size_t>(pad), ' ');
        m_os << num;
        m_os << " | ";
        style_off();
    }

    void DiagnosticPrinter::write_gutter_empty(int gutter_w)
    {
        style_on(ansi::bold_blue);
        m_os << ' ';
        m_os << std::string(static_cast<std::size_t>(gutter_w), ' ');
        m_os << " | ";
        style_off();
    }

    void DiagnosticPrinter::write_gutter_break(int gutter_w)
    {
        style_on(ansi::bold_blue);
        m_os << ' ';
        int pad = gutter_w + 1 - 3;
        if (pad > 0)
            m_os << std::string(static_cast<std::size_t>(pad), ' ');

        m_os << "...";
        style_off();
        m_os << '\n';
    }

    void DiagnosticPrinter::render_header(const Diagnostic& diag)
    {
        style_on(severity_color(diag.severity()));
        std::print(m_os, "{}", to_string(diag.severity()));
        if (!diag.code().empty())
            std::print(m_os, "[{}]", diag.code());

        style_off();

        bold_on();
        std::print(m_os, ": {}", diag.message());
        style_off();

        std::print(m_os, "\n");
    }

    void DiagnosticPrinter::render_location(const sm::SourceFile& file, sm::Offset offset, int gutter_w)
    {
        auto lc = file.line_col(offset);

        style_on(ansi::bold_blue);
        m_os << ' ';
        m_os << std::string(static_cast<std::size_t>(gutter_w), ' ');
        m_os << "--> ";
        style_off();

        m_os << file.path().string();
        if (lc)
            std::print(m_os, ":{}:{}", lc->line, lc->column);

        m_os << '\n';
    }

    void DiagnosticPrinter::render_snippet(const sm::SourceFile& file, std::span<const Label> labels, Severity sev, int gutter_w)
    {
        if (labels.empty())
            return;

        struct LabelInfo
        {
            uint32_t start_line, end_line;
            uint32_t start_byte_in_line, end_byte_in_line;
            const Label* label;
        };
        std::vector<LabelInfo> infos;
        infos.reserve(labels.size());

        for (const auto& lab : labels)
        {
            auto sl = file.line_col(lab.range.begin.offset);
            auto el = file.line_col(lab.range.end.offset);
            if (!sl || !el)
                continue;

            auto start_line_text = file.line_text(sl->line);
            auto end_line_text = file.line_text(el->line);
            if (!start_line_text || !end_line_text)
                continue;

            uint32_t sb = sl->byte_col - 1;
            uint32_t eb = el->byte_col - 1;

            infos.push_back({sl->line, el->line, sb, eb, &lab});
        }

        if (infos.empty())
            return;

        std::set<uint32_t> needed_lines;
        for (const auto& info : infos)
            for (uint32_t l = info.start_line; l <= info.end_line; ++l)
                needed_lines.insert(l);

        std::set<uint32_t> display_lines;
        uint32_t total_lines = file.line_count();
        for (uint32_t l : needed_lines)
        {
            uint32_t lo = (l > m_context_lines) ? l - m_context_lines : 1;
            uint32_t hi = std::min(l + m_context_lines, total_lines);
            for (uint32_t k = lo; k <= hi; ++k)
                display_lines.insert(k);
        }

        std::vector<DisplayLine> dlines;
        dlines.reserve(display_lines.size());

        for (uint32_t line_no : display_lines)
        {
            auto text = file.line_text(line_no);
            if (!text)
                continue;

            std::string_view raw = *text;
            if (!raw.empty() && raw.back() == '\n')
                raw.remove_suffix(1);
            if (!raw.empty() && raw.back() == '\r')
                raw.remove_suffix(1);

            DisplayLine dl;
            dl.line_number = line_no;
            dl.expanded_text = expand_tabs(raw);

            for (const auto& info : infos)
            {
                if (line_no < info.start_line || line_no > info.end_line)
                    continue;

                uint32_t byte_start = 0;
                uint32_t byte_end = static_cast<uint32_t>(raw.size());

                if (line_no == info.start_line)
                    byte_start = info.start_byte_in_line;
                if (line_no == info.end_line)
                    byte_end = info.end_byte_in_line;

                uint32_t col_start = byte_to_display_col(raw, byte_start);
                uint32_t col_end = byte_to_display_col(raw, byte_end);

                if (col_end <= col_start)
                    col_end = col_start + 1;

                std::string_view msg;
                if (line_no == info.end_line)
                    msg = info.label->message;

                dl.annotations.push_back({col_start, col_end, info.label->style, msg});
            }

            std::ranges::sort(dl.annotations, {}, &LineAnnotation::col_start);

            dlines.push_back(std::move(dl));
        }

        write_gutter_empty(gutter_w);
        std::print(m_os, "\n");

        for (std::size_t i = 0; i < dlines.size(); ++i)
        {
            if (i > 0 && dlines[i].line_number > dlines[i - 1].line_number + 1)
                write_gutter_break(gutter_w);

            render_source_line(dlines[i], sev, gutter_w);
            if (!dlines[i].annotations.empty())
                render_annotations(dlines[i], sev, gutter_w);
        }
    }

    void DiagnosticPrinter::render_source_line(const DisplayLine& dl, Severity, int gutter_w)
    {
        write_gutter(gutter_w, dl.line_number);
        std::print(m_os, "{}\n", dl.expanded_text);
    }

    void DiagnosticPrinter::render_annotations(const DisplayLine& dl, Severity sev, int gutter_w)
    {
        const auto& annots = dl.annotations;
        if (annots.empty())
            return;

        uint32_t max_col = 0;
        for (const auto& a : annots)
            max_col = std::max(max_col, a.col_end);

        struct Cell
        {
            char ch;
            LabelStyle style;
        };
        std::vector<Cell> underline(max_col, {' ', LabelStyle::Primary});

        for (const auto& a : annots)
        {
            char ch = (a.style == LabelStyle::Primary) ? '^' : '~';
            for (uint32_t c = a.col_start; c < a.col_end && c < max_col; ++c)
                underline[c] = {ch, a.style};
        }

        write_gutter_empty(gutter_w);
        for (uint32_t c = 0; c < max_col; ++c)
        {
            if (underline[c].ch != ' ')
            {
                style_on(label_color(underline[c].style, sev));
                m_os.put(underline[c].ch);
                style_off();
            }
            else
            {
                m_os.put(' ');
            }
        }

        auto with_msg = annots | std::views::filter([](const auto& a) { return !a.message.empty(); });
        auto msg_count = std::ranges::distance(with_msg);

        if (msg_count == 1)
        {
            auto& a = *with_msg.begin();
            m_os.put(' ');
            style_on(label_color(a.style, sev));
            std::print(m_os, "{}", a.message);
            style_off();
            std::print(m_os, "\n");
            return;
        }

        std::print(m_os, "\n");

        if (msg_count == 0)
            return;

        struct PendingMsg
        {
            uint32_t col;
            LabelStyle style;
            std::string_view message;
        };
        std::vector<PendingMsg> pending;
        for (const auto& a : annots)
            if (!a.message.empty())
                pending.push_back({a.col_start, a.style, a.message});

        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();

            write_gutter_empty(gutter_w);

            uint32_t col = 0;

            for (const auto& p : pending)
            {
                while (col < p.col)
                {
                    m_os.put(' ');
                    ++col;
                }

                style_on(label_color(p.style, sev));
                m_os.put('|');
                style_off();
                ++col;
            }

            while (col < current.col)
            {
                m_os.put(' ');
                ++col;
            }
            style_on(label_color(current.style, sev));
            std::print(m_os, "{}", current.message);
            style_off();
            std::print(m_os, "\n");
        }
    }

    void DiagnosticPrinter::render_footer(const Diagnostic& diag, int gutter_w)
    {
        auto render_item = [&](std::string_view prefix, std::string_view color, std::string_view text) {
            style_on(ansi::bold_blue);
            m_os << ' ';
            m_os << std::string(static_cast<std::size_t>(gutter_w), ' ');
            m_os << ' ';
            style_off();

            style_on(color);
            bold_on();
            m_os << "= " << prefix;
            style_off();
            m_os << ": " << text << '\n';
        };

        for (const auto& n : diag.notes())
            render_item("note", ansi::bold_blue, n);

        for (const auto& h : diag.helps())
            render_item("help", ansi::bold_cyan, h);

        for (const auto& f : diag.fixes())
        {
            std::string text = f.message;
            if (!f.replacement.empty())
            {
                if (!text.empty())
                    text += ": ";
                text += '`';
                text += f.replacement;
                text += '`';
            }
            render_item("fix", ansi::bold_green, text);
        }
    }

    void DiagnosticPrinter::emit(const Diagnostic& diag)
    {
        render_header(diag);

        if (diag.labels().empty())
        {
            render_footer(diag, 1);
            std::print(m_os, "\n");
            return;
        }

        std::map<sm::FileId, std::vector<const Label*>> file_groups;
        for (const auto& lab : diag.labels())
            file_groups[lab.range.begin.fileId].push_back(&lab);

        int gutter_w = 1;
        for (const auto& [fid, labs] : file_groups)
        {
            const auto* file = m_sm.get(fid);
            if (!file)
                continue;

            for (const auto* lab : labs)
            {
                auto lc = file->line_col(lab->range.end.offset);
                if (lc)
                {
                    uint32_t end_line = lc->line + m_context_lines;
                    gutter_w = std::max(gutter_w, digit_count(end_line));
                }
            }
        }

        bool first_file = true;
        for (const auto& [fid, lab_ptrs] : file_groups)
        {
            const auto* file = m_sm.get(fid);
            if (!file)
                continue;

            std::vector<Label> file_labels;
            file_labels.reserve(lab_ptrs.size());
            for (const auto* lp : lab_ptrs)
                file_labels.push_back(*lp);

            const Label* primary = nullptr;
            for (const auto& l : file_labels)
            {
                if (l.style == LabelStyle::Primary)
                {
                    primary = &l;
                    break;
                }
            }

            if (!primary)
                primary = &file_labels.front();

            if (first_file)
            {
                render_location(*file, primary->range.begin.offset, gutter_w);
                first_file = false;
            }
            else
            {
                style_on(ansi::bold_blue);
                m_os << ' ';
                m_os << std::string(static_cast<std::size_t>(gutter_w), ' ');
                m_os << "::: ";
                style_off();

                m_os << file->path().string();
                auto lc = file->line_col(primary->range.begin.offset);
                if (lc)
                    std::print(m_os, ":{}:{}", lc->line, lc->column);

                m_os << '\n';
            }

            render_snippet(*file, file_labels, diag.severity(), gutter_w);
        }

        write_gutter_empty(gutter_w);
        std::print(m_os, "\n");

        render_footer(diag, gutter_w);
        std::print(m_os, "\n");
    }

} // namespace dcc::diag
