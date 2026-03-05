#ifndef DCC_LEXER_LEX_HH
#define DCC_LEXER_LEX_HH

#include <lex/token.hh>
#include <optional>
#include <string_view>
#include <util/si.hh>
#include <util/source_manager.hh>

namespace dcc::lex
{
    class Lexer
    {
    public:
        explicit Lexer(const sm::SourceFile& file, si::StringInterner& interner) noexcept;

        [[nodiscard]] Token next();
        [[nodiscard]] const Token& peek();
        bool eat(TokenKind kind);
        [[nodiscard]] std::optional<Token> eat_token(TokenKind kind);

        [[nodiscard]] bool at_end();

        [[nodiscard]] sm::Offset position() const noexcept { return m_pos; }

        [[nodiscard]] sm::FileId file_id() const noexcept { return m_file.id(); }
        [[nodiscard]] const sm::SourceFile& file() const noexcept { return m_file; }
        [[nodiscard]] si::StringInterner& interner() noexcept { return m_interner; }

    private:
        const sm::SourceFile& m_file;
        si::StringInterner& m_interner;
        std::string_view m_source;
        sm::Offset m_pos{};
        std::optional<Token> m_peek;

        [[nodiscard]] char current() const noexcept;
        [[nodiscard]] char lookahead(sm::Offset ahead = 1) const noexcept;
        char advance() noexcept;
        bool match(char expected) noexcept;
        [[nodiscard]] bool at_eof() const noexcept;

        [[nodiscard]] Token make(TokenKind kind, sm::Offset start) const noexcept;

        [[nodiscard]] Token lex();

        void skip_trivia();
        void skip_line_comment() noexcept;
        bool skip_block_comment() noexcept;

        [[nodiscard]] Token lex_identifier();
        [[nodiscard]] Token lex_number();
        [[nodiscard]] Token lex_string();
        [[nodiscard]] Token lex_char();

        void consume_digits(bool (*pred)(char)) noexcept;

        [[nodiscard]] static bool is_digit_base(char c, int base) noexcept;
        [[nodiscard]] static std::optional<int64_t> parse_integer_literal(std::string_view spelling, int base) noexcept;
        [[nodiscard]] static std::optional<double> parse_float_literal(std::string_view spelling) noexcept;
        [[nodiscard]] static std::optional<uint32_t> parse_char_literal(std::string_view content) noexcept;
        [[nodiscard]] static std::string_view strip_suffix(std::string_view spelling) noexcept;
        [[nodiscard]] static bool is_literal_suffix(std::string_view sv) noexcept;
        [[nodiscard]] static std::optional<std::string> parse_string_literal(std::string_view content) noexcept;
        [[nodiscard]] static TokenValue parse_literal_value(const Token& tok) noexcept;

        [[nodiscard]] static bool is_dec(char c) noexcept;
        [[nodiscard]] static bool is_hex(char c) noexcept;
        [[nodiscard]] static bool is_bin(char c) noexcept;
        [[nodiscard]] static bool is_oct(char c) noexcept;
        [[nodiscard]] static bool is_ident_head(char c) noexcept;
        [[nodiscard]] static bool is_ident_body(char c) noexcept;
    };

} // namespace dcc::lex

#endif /* DCC_LEXER_LEX_HH */
