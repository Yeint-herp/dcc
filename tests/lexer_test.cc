#include <gtest/gtest.h>
#include <lex/lexer.hh>
#include <util/si.hh>
#include <util/source_manager.hh>

using namespace dcc::lex;
using namespace dcc::sm;
using namespace dcc::si;

namespace dcc::test
{
    TEST(Lexer, BasicTokens)
    {
        SourceManager sm;
        StringInterner interner;
        Token token;

        {
            auto id = sm.add_synthetic("test_file", "");
            const SourceFile& file = *sm.get(id);
            Lexer lexer(file, interner);

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Eof);
        }

        {
            std::string source = "int a = 10;";
            auto id = sm.add_synthetic("test_file", source);
            const SourceFile& file = *sm.get(id);
            Lexer lexer(file, interner);

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Identifier);
            EXPECT_EQ(token.text, "int");

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Identifier);
            EXPECT_EQ(token.text, "a");

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Eq);
            EXPECT_EQ(token.text, "=");

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::IntLiteral);
            EXPECT_EQ(token.text, "10");

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Semicolon);
            EXPECT_EQ(token.text, ";");

            token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Eof);
        }
    }

    TEST(Lexer, Keywords)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "void u32 f32 struct enum union";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::KwVoid);
        EXPECT_EQ(token.text, "void");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Kwu32);
        EXPECT_EQ(token.text, "u32");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Kwf32);
        EXPECT_EQ(token.text, "f32");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::KwStruct);
        EXPECT_EQ(token.text, "struct");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::KwEnum);
        EXPECT_EQ(token.text, "enum");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::KwUnion);
        EXPECT_EQ(token.text, "union");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, Identifiers)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "var1 _var2 var_name";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Identifier);
        EXPECT_EQ(token.text, "var1");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Identifier);
        EXPECT_EQ(token.text, "_var2");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Identifier);
        EXPECT_EQ(token.text, "var_name");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, IntLiterals)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "10 0x1A 0b1010 0o12";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::IntLiteral);
        EXPECT_EQ(token.text, "10");
        EXPECT_EQ(token.value->as_int_value(), 10);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::IntLiteral);
        EXPECT_EQ(token.text, "0x1A");
        EXPECT_EQ(token.value->as_int_value(), 26);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::IntLiteral);
        EXPECT_EQ(token.text, "0b1010");
        EXPECT_EQ(token.value->as_int_value(), 10);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::IntLiteral);
        EXPECT_EQ(token.text, "0o12");
        EXPECT_EQ(token.value->as_int_value(), 10);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, FloatLiterals)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "1.0 0.1 1e-1 1.0f32";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
        EXPECT_EQ(token.text, "1.0");
        EXPECT_DOUBLE_EQ(token.value->as_float_value(), 1.0);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
        EXPECT_EQ(token.text, "0.1");
        EXPECT_DOUBLE_EQ(token.value->as_float_value(), 0.1);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
        EXPECT_EQ(token.text, "1e-1");
        EXPECT_DOUBLE_EQ(token.value->as_float_value(), 0.1);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
        EXPECT_EQ(token.text, "1.0f32");
        EXPECT_DOUBLE_EQ(token.value->as_float_value(), 1.0);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, StringLiterals)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "\"hello\" \"hello\\nworld\" \"hex\\x41\"";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::StringLiteral);
        EXPECT_EQ(token.text, "\"hello\"");
        EXPECT_EQ(token.value->as_string_value(), "hello");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::StringLiteral);
        EXPECT_EQ(token.text, "\"hello\\nworld\"");
        EXPECT_EQ(token.value->as_string_value(), "hello\nworld");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::StringLiteral);
        EXPECT_EQ(token.text, "\"hex\\x41\"");
        EXPECT_EQ(token.value->as_string_value(), "hexA");

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, CharLiterals)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "'a' '\\n' '\\x41' '\\u{1F600}'";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::CharLiteral);
        EXPECT_EQ(token.text, "'a'");
        EXPECT_EQ(token.value->as_char_value(), 'a');

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::CharLiteral);
        EXPECT_EQ(token.text, "'\\n'");
        EXPECT_EQ(token.value->as_char_value(), '\n');

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::CharLiteral);
        EXPECT_EQ(token.text, "'\\x41'");
        EXPECT_EQ(token.value->as_char_value(), 0x41);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::CharLiteral);
        EXPECT_EQ(token.text, "'\\u{1F600}'");
        EXPECT_EQ(token.value->as_char_value(), 0x1F600);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, Punctuation)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "()[]{}.,;:!?";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LParen);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::RParen);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LBracket);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::RBracket);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LBrace);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::RBrace);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Dot);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Comma);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Semicolon);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Colon);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Bang);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Question);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, Operators)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "+ - * / % & | ^ ! && || = == != < > <= >= << >> += -= *= /= %= &= |= ^= <<= >>= ++ --";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Plus);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Minus);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Star);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Slash);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Percent);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Amp);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Pipe);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Caret);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Bang);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::AmpAmp);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::PipePipe);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::EqEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::BangEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Lt);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Gt);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LtEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::GtEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LtLt);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::GtGt);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::PlusEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::MinusEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::StarEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::SlashEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::PercentEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::AmpEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::PipeEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::CaretEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::LtLtEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::GtGtEq);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Increment);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Decrement);
        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, Comments)
    {
        SourceManager sm;
        StringInterner interner;
        std::string source = "// comment\n/* block comment */";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eof);
    }

    TEST(Lexer, ErrorStatesAndEdgeCases)
    {
        SourceManager sm;
        StringInterner interner;

        {
            auto id = sm.add_synthetic("test_file1", "\"unterminated string");
            const SourceFile& file = *sm.get(id);
            Lexer lexer(file, interner);

            Token token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Invalid);
        }

        {
            auto id = sm.add_synthetic("test_file2", "'a");
            const SourceFile& file = *sm.get(id);
            Lexer lexer(file, interner);

            Token token = lexer.next();
            EXPECT_EQ(token.kind, TokenKind::Invalid);
        }
    }

    TEST(Lexer, SourceRanges)
    {
        SourceManager sm;
        StringInterner interner;

        std::string source = "  int a = 1;";
        auto id = sm.add_synthetic("test_file", source);
        const SourceFile& file = *sm.get(id);
        Lexer lexer(file, interner);

        Token token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Identifier);
        EXPECT_EQ(token.range.begin.offset, 2);
        EXPECT_EQ(token.range.end.offset, 5);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Identifier);
        EXPECT_EQ(token.range.begin.offset, 6);
        EXPECT_EQ(token.range.end.offset, 7);

        token = lexer.next();
        EXPECT_EQ(token.kind, TokenKind::Eq);
        EXPECT_EQ(token.range.begin.offset, 8);
        EXPECT_EQ(token.range.end.offset, 9);
    }

} // namespace dcc::test
