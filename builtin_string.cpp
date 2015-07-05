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
// XXX documentation
// XXX remaining subcommands

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
    string_fatal_error(_(L"string escape: not yet implemented"));
    return BUILTIN_STRING_OK;
}

static int string_match(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string match: not yet implemented"));
    return BUILTIN_STRING_OK;
}

static int string_replace(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string replace: not yet implemented"));
    return BUILTIN_STRING_OK;
}

static int string_split(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string split: not yet implemented"));
    return BUILTIN_STRING_OK;
}

static int string_length(parser_t &parser, int argc, wchar_t **argv)
{
    if (argc > 1)
    {
        string_fatal_error(_(L"string length: too many arguments"));
        return BUILTIN_STRING_ERROR;
    }

    int len = (argc == 0) ? 0 : wcslen(argv[0]);

    append_format(stdout_buffer, L"%d\n", len);
    return BUILTIN_STRING_OK;
}

static int string_sub(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string sub: not yet implemented"));
    return BUILTIN_STRING_OK;
}

static const struct string_subcommand
{
    const wchar_t *name;
    int (*handler)(parser_t &, int argc, wchar_t **argv);
}
string_subcommands[] =
{
    { L"escape", &string_escape },
    { L"match", &string_match },
    { L"replace", &string_replace },
    { L"split", &string_split },
    { L"length", &string_length },
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
        string_fatal_error(_(L"string: not enough arguments"));
        return BUILTIN_STRING_ERROR;
    }

    wchar_t *subcmd_arg = argv[1];
    argc -= 2;
    argv += 2;

    const string_subcommand *subcmd = &string_subcommands[0];
    while (subcmd->name != 0 && wcscmp(subcmd->name, subcmd_arg) != 0)
    {
        subcmd++;
    }
    if (subcmd->handler == 0)
    {
        string_fatal_error(_(L"string: unknown subcommand: %ls"), subcmd_arg);
        return BUILTIN_STRING_ERROR;
    }

    return subcmd->handler(parser, argc, argv);
}
