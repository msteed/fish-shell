/** \file builtin_string.cpp
  Functions for executing the string builtin.
*/
#include "config.h"

#include <stdlib.h>

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "builtin.h"
#include "parser.h"
#include "common.h"
//#include "wgetopt.h"

// XXX tests
// XXX documentation & help
// XXX string_join
// XXX string_match
// XXX string_replace
// XXX string_split
// XXX string_sub

enum
{
    BUILTIN_STRING_OK = STATUS_BUILTIN_OK,
    BUILTIN_STRING_ERROR = STATUS_BUILTIN_ERROR
};

static void string_fatal_error(const wchar_t *fmt, ...)
{
#if 0
    // Don't error twice
    if (early_exit)
        return;
#endif

    va_list va;
    va_start(va, fmt);
    wcstring errstr = vformat_string(fmt, va);
    va_end(va);
    stderr_buffer.append(errstr);
    if (! string_suffixes_string(L"\n", errstr))
        stderr_buffer.push_back(L'\n');

#if 0
    this->exit_code = STATUS_BUILTIN_ERROR;
    this->early_exit = true;
#endif
}

static int string_escape(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"";
    const struct woption long_options[] = { 0, 0, 0, 0 };

    woptind = 0;
    while (1)
    {
        int c = wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        else if (c == '?')
        {
            builtin_unknown_option(parser, argv[0], argv[woptind - 1]);
            return BUILTIN_STRING_ERROR;
        }
    }

    for (int i = woptind; i < argc; i++)
    {
        wcstring escaped = escape(argv[i], ESCAPE_ALL);
        append_format(stdout_buffer, L"%ls\n", escaped.c_str());
    }

    return BUILTIN_STRING_OK;
}

static int string_join(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string join: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static int string_length(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"";
    const struct woption long_options[] = { 0, 0, 0, 0 };

    woptind = 0;
    while (1)
    {
        int c = wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        else if (c == '?')
        {
            builtin_unknown_option(parser, argv[0], argv[woptind - 1]);
            return BUILTIN_STRING_ERROR;
        }
    }

    for (int i = woptind; i < argc; i++)
    {
        int len = wcslen(argv[i]);
        append_format(stdout_buffer, L"%d\n", len);
    }

    return BUILTIN_STRING_OK;
}

static int string_match(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string match: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static int string_replace(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string replace: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static int string_split(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string split: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static int string_sub(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string sub: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static const struct string_subcommand
{
    const wchar_t *name;
    int (*handler)(parser_t &, int argc, wchar_t **argv);
}
string_subcommands[] =
{
    { L"escape", &string_escape },
    { L"join", &string_join },
    { L"length", &string_length },
    { L"match", &string_match },
    { L"replace", &string_replace },
    { L"split", &string_split },
    { L"sub", &string_sub },
    { 0, 0 }
};

/**
   The string builtin. Used for manipulating strings.
*/
static int builtin_string(parser_t &parser, wchar_t **argv)
{
    int argc = builtin_count_args(argv);
    if (argc <= 1)
    {
        string_fatal_error(_(L"%ls: Expected at least one argument"), argv[0]);
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_ERROR;
    }

    if (wcscmp(argv[1], L"-h") == 0 || wcscmp(argv[1], L"--help") == 0)
    {
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_OK;
    }

    const string_subcommand *subcmd = &string_subcommands[0];
    while (subcmd->name != 0 && wcscmp(subcmd->name, argv[1]) != 0)
    {
        subcmd++;
    }
    if (subcmd->handler == 0)
    {
        string_fatal_error(_(L"%ls: Unknown subcommand '%ls'"), argv[0], argv[1]);
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_ERROR;
    }

    argc--;
    argv++;
    return subcmd->handler(parser, argc, argv);
}
