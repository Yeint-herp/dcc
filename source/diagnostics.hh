#ifndef DCC_DIAGNOSTICS_HH
#define DCC_DIAGNOSTICS_HH

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <util/source_manager.hh>
#include <vector>

namespace dcc::diag
{
    enum class Severity : uint8_t
    {
        Error,
        Warning,
        Note,
        Help,
    };
    [[nodiscard]] std::string_view to_string(Severity s) noexcept;

    enum class LabelStyle : uint8_t
    {
        Primary,
        Secondary,
    };

    struct Label
    {
        sm::SourceRange range;
        std::string message;
        LabelStyle style{LabelStyle::Primary};
    };

    struct FixIt
    {
        sm::SourceRange range;
        std::string replacement;
        std::string message;
    };

    class Diagnostic
    {
    public:
        explicit Diagnostic(Severity severity, std::string message);

        Diagnostic& with_code(std::string code) &;
        Diagnostic&& with_code(std::string code) &&;

        Diagnostic& with_label(Label label) &;
        Diagnostic&& with_label(Label label) &&;

        Diagnostic& with_primary(sm::SourceRange range, std::string message) &;
        Diagnostic&& with_primary(sm::SourceRange range, std::string message) &&;

        Diagnostic& with_secondary(sm::SourceRange range, std::string message) &;
        Diagnostic&& with_secondary(sm::SourceRange range, std::string message) &&;

        Diagnostic& with_note(std::string note) &;
        Diagnostic&& with_note(std::string note) &&;

        Diagnostic& with_help(std::string help) &;
        Diagnostic&& with_help(std::string help) &&;

        Diagnostic& with_fix(FixIt fix) &;
        Diagnostic&& with_fix(FixIt fix) &&;

        [[nodiscard]] Severity severity() const noexcept { return m_severity; }
        [[nodiscard]] const std::string& message() const noexcept { return m_message; }
        [[nodiscard]] const std::string& code() const noexcept { return m_code; }
        [[nodiscard]] std::span<const Label> labels() const noexcept { return m_labels; }
        [[nodiscard]] std::span<const std::string> notes() const noexcept { return m_notes; }
        [[nodiscard]] std::span<const std::string> helps() const noexcept { return m_helps; }
        [[nodiscard]] std::span<const FixIt> fixes() const noexcept { return m_fixes; }

    private:
        Severity m_severity;
        std::string m_message;
        std::string m_code;
        std::vector<Label> m_labels;
        std::vector<std::string> m_notes;
        std::vector<std::string> m_helps;
        std::vector<FixIt> m_fixes;
    };

    [[nodiscard]] Diagnostic error(std::string message);
    [[nodiscard]] Diagnostic warning(std::string message);
    [[nodiscard]] Diagnostic note(std::string message);
    [[nodiscard]] Diagnostic help(std::string message);

    class DiagnosticPrinter
    {
    public:
        explicit DiagnosticPrinter(const sm::SourceManager& sm, std::ostream& os);

        void emit(const Diagnostic& diag);

        void set_color(bool enable) noexcept { m_color = enable; }
        [[nodiscard]] bool color() const noexcept { return m_color; }

        void set_context_lines(uint32_t n) noexcept { m_context_lines = n; }
        void set_tab_width(uint32_t n) noexcept { m_tab_width = n; }

    private:
        const sm::SourceManager& m_sm;
        std::ostream& m_os;
        bool m_color{true};
        uint32_t m_context_lines{1};
        uint32_t m_tab_width{4};

        void style_on(std::string_view ansi);
        void style_off();
        void bold_on();

        [[nodiscard]] std::string_view severity_color(Severity s) const noexcept;
        [[nodiscard]] std::string_view label_color(LabelStyle s, Severity sev) const noexcept;
        [[nodiscard]] static constexpr std::string_view blue() noexcept { return "\033[94m"; }
        [[nodiscard]] static constexpr std::string_view cyan() noexcept { return "\033[96m"; }
        [[nodiscard]] static constexpr std::string_view green() noexcept { return "\033[92m"; }

        struct LineAnnotation
        {
            uint32_t col_start;
            uint32_t col_end;
            LabelStyle style;
            std::string_view message;
        };

        struct DisplayLine
        {
            uint32_t line_number;
            std::string expanded_text;
            std::vector<LineAnnotation> annotations;
        };

        void render_header(const Diagnostic& diag);
        void render_location(const sm::SourceFile& file, sm::Offset offset, int gutter_w);
        void render_snippet(const sm::SourceFile& file, std::span<const Label> labels, Severity sev, int gutter_w);
        void render_source_line(const DisplayLine& dl, Severity sev, int gutter_w);
        void render_annotations(const DisplayLine& dl, Severity sev, int gutter_w);
        void render_footer(const Diagnostic& diag, int gutter_w);

        void write_gutter(int gutter_w, uint32_t line_no);
        void write_gutter_empty(int gutter_w);
        void write_gutter_break(int gutter_w);

        [[nodiscard]] std::string expand_tabs(std::string_view line) const;
        [[nodiscard]] uint32_t byte_to_display_col(std::string_view line, uint32_t byte_off) const;

        [[nodiscard]] static int digit_count(uint32_t n) noexcept;
    };

} // namespace dcc::diag

#endif /* DCC_DIAGNOSTICS_HH */
