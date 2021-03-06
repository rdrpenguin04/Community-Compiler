#include <fstream>
#include <iostream>
#include <vector>
#include "AstPrettyPrinter.h"
#include "CodeGen.h"
#include "Parser.h"
#include "TokenStream.h"
#include "Terminal.h"

#ifdef _WIN32
#include <windows.h>
#endif

std::string load_text_from_file(std::string filepath)
{
    std::ifstream stream(filepath);
    std::string str(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    return str;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Missing filename in args.\n");
        return 1;
    }

#ifdef _WIN32
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return GetLastError();
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return GetLastError();
    }
#endif

    std::vector<TokenStream> toks;
    std::vector<Ast> asts;

    bool errors_occurred = false;

    for (int i = 2; i < argc; i++)
    {
        std::string file_contents = load_text_from_file(argv[i]);

        TokenStream stream;
        stream.lex(file_contents);
        toks.push_back(stream);

        if (!stream.errors.empty())
        {
            errors_occurred = true;
            for (Error error : stream.errors)
            {
                printf("\n%s%s @ %s%s%d%s:%s%d%s\n",
                       term_fg[TermColour::Yellow],
                       error.message.c_str(),
                       term_reset,
                       term_fg[TermColour::Blue], error.line, term_reset,
                       term_fg[TermColour::Blue], error.column, term_reset);
                syntax_highlight_print_error(
                    file_contents, stream,
                    error.line, error.offset, error.count);
            }
        }
        else
        {
            Parser parser;
            Ast ast = parser.parse(stream.tokens);
            delete ast.root;

            if (!parser.errors.empty())
            {
                errors_occurred = true;
                for (Error error : parser.errors)
                {
                    printf("\n-----------------------------\n\n");
                    printf("\n%s%s @ %s%s%d%s:%s%d%s\n",
                        term_fg[TermColour::Yellow],
                        error.message.c_str(),
                        term_reset,
                        term_fg[TermColour::Blue], error.line, term_reset,
                        term_fg[TermColour::Blue], error.column, term_reset
                    );
                    syntax_highlight_print_error(
                        file_contents, stream,
                        error.line, error.offset, error.count);
                }
            }
        }
    }

    if (errors_occurred)
    {
        for (auto &ast : asts)
        {
            delete ast.root;
        }

        printf("\n------------------------\nErrors occurred, exiting\n");
        return 1;
    }

    for(auto &stream : toks) {
        Parser parser;
        asts.push_back(parser.parse(stream.tokens));
    }

    Semantics sem;

    for (size_t i = 0; i < asts.size(); i++)
    {
        sem.pass1(asts[i]);
    }

    for (size_t i = 0; i < asts.size(); i++)
    {
        sem.pass2(asts[i]);
    }

    for (size_t i = 0; i < asts.size(); i++)
    {
        sem.pass3(asts[i]);
        //  pretty_print_ast(asts[i]);
    }

    if (!sem.errors.empty())
    {
        for (Error error : sem.errors)
        {
            printf("%s\n", error.message.c_str());
        }
        return 1;
    }

    scope.clear();
    args.clear();

    while (!scope_stack.empty())
    {
        scope_stack.pop();
    }

    while (!arg_stack.empty())
    {
        arg_stack.pop();
    }

    ILemitter il;

    for (size_t i = 0; i < asts.size(); i++)
    {
        generate_il(asts[i].root, il, sem);
    }

    FILE *file = fopen(argv[1], "wb");
    size_t size = il.stream.size();
    fwrite(&il.stream[0], size, 1, file);
    fclose(file);

    for (auto &ast : asts)
    {
        delete ast.root;
    }

    return 0;
}
