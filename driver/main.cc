#include <cstdio>
#include <cstring>
#include <diagnostics.hh>
#include <iostream>
#include <lex/lexer.hh>
#include <parse/arena.hh>
#include <parse/parser.hh>
#include <sema/sema.hh>
#include <util/si.hh>
#include <util/source_manager.hh>

static void usage(const char* prog)
{
    std::fprintf(stderr, "Usage: %s [options] <input.dcc>\n", prog);
    std::fprintf(stderr, "Options:\n");
    std::fprintf(stderr, "  -o <file>    Output file (default: a.out)\n");
    std::fprintf(stderr, "  -O<n>        Optimization level (0-3, default: 0)\n");
    std::fprintf(stderr, "  -h, --help   Show this help\n");
}

int main(int argc, char** argv)
{
    const char* input_file = nullptr;
    const char* output_file = "a.out";
    int opt_level = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output_file = argv[++i];
        }
        else if (std::strncmp(argv[i], "-O", 2) == 0 && argv[i][2] >= '0' && argv[i][2] <= '3')
        {
            opt_level = argv[i][2] - '0';
            (void)opt_level;
        }
        else if (argv[i][0] != '-')
        {
            if (input_file)
            {
                std::fprintf(stderr, "error: multiple input files not yet supported\n");
                return 1;
            }
            input_file = argv[i];
        }
        else
        {
            std::fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!input_file)
    {
        std::fprintf(stderr, "error: no input file\n");
        usage(argv[0]);
        return 1;
    }

    dcc::sm::SourceManager sm;
    dcc::si::StringInterner interner;
    dcc::parse::AstArena arena;

    auto fid_result = sm.load(input_file);
    if (!fid_result)
    {
        std::fprintf(stderr, "error: cannot open '%s': %s\n", input_file, std::string{dcc::sm::to_string(fid_result.error())}.c_str());
        return 1;
    }

    auto* file = sm.get(*fid_result);

    dcc::lex::Lexer lexer{*file, interner};
    dcc::diag::DiagnosticPrinter printer{sm, std::cerr};
    dcc::parse::Parser parser{lexer, arena, printer};

    auto* tu = parser.parse();
    if (parser.had_error() || !tu)
    {
        std::fprintf(stderr, "error: parse failed\n");
        return 1;
    }

    dcc::sema::Sema sema{sm, interner, printer};
    if (!sema.analyze(*tu))
    {
        std::fprintf(stderr, "error: semantic analysis failed\n");
        return 1;
    }

    // TODO: AST → IR lowering
    // TODO: IR optimization passes
    // TODO: backend emission (LLVM / assembly / object)
    (void)output_file;

    std::fprintf(stderr, "note: compilation successful (IR emission not yet implemented)\n");
    return 0;
}
