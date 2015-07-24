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

// XXX verify test coverage including edge cases like empty strings
// XXX documentation & help - finish & fix formatting
// XXX string_match --regex
// XXX string_replace
// XXX finalize & document return values
// XXX split: default to splitting on whitespace?
// XXX accept arguments from stdin if !isatty(stdin)

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
    for (;;)
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
    const wchar_t *short_options = L"";
    const struct woption long_options[] = { 0, 0, 0, 0 };

    woptind = 0;
    for (;;)
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

    if (argc - woptind < 1)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int i = woptind;
    const wchar_t *sep = argv[i++];
    for (; i < argc - 1; i++)
    {
        append_format(stdout_buffer, L"%ls%ls", argv[i], sep);
    }
    if (i < argc)
    {
        append_format(stdout_buffer, L"%ls\n", argv[i]);
    }

    return BUILTIN_STRING_OK;
}

static int string_length(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"";
    const struct woption long_options[] = { 0, 0, 0, 0 };

    woptind = 0;
    for (;;)
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

static bool string_match_wildcard(const wchar_t *pattern, const wchar_t *string, bool ignore_case)
{
    for (; *string != L'\0'; string++, pattern++)
    {
        switch (*pattern)
        {
            case L'?':
                break;

            case L'*':
                // skip repeated *
                while (*pattern == L'*')
                {
                    pattern++;
                }

                // * at end matches whatever follows
                if (*pattern == L'\0')
                {
                    return true;
                }

                while (*string != L'\0')
                {
                    if (string_match_wildcard(pattern, string++, ignore_case))
                    {
                        return true;
                    }
                }
                return false;

            case L'[':
            {
                bool negate = false;
                if (*++pattern == L'^')
                {
                    negate = true;
                    pattern++;
                }

                bool match = false;
                wchar_t strch = ignore_case ? towlower(*string) : *string;
                wchar_t patch, patch2;
                while ((patch = *pattern++) != L']')
                {
                    if (patch == L'\0')
                    {
                        return false; // no closing ]
                    }
                    if (*pattern == L'-' && (patch2 = *(pattern + 1)) != L'\0' && patch2 != L']')
                    {
                        if (ignore_case ? towlower(patch) <= strch && strch <= towlower(patch2)
                                        : patch <= strch && strch <= patch2)
                        {
                            match = true;
                        }
                        pattern += 2;
                    }
                    else if (patch == strch)
                    {
                        match = true;
                    }
                }
                if (match == negate)
                {
                    return false;
                }
                pattern--;
                break;
            }

            case L'\\':
                if (*(pattern + 1) != L'\0')
                {
                    pattern++;
                }
                // fall through

            default:
                if (ignore_case ? towlower(*string) != towlower(*pattern) : *string != *pattern)
                {
                    return false;
                }
                break;
        }
    }
    // string is exhausted - it's a match only if pattern is as well
    while (*pattern == L'*')
    {
        pattern++;
    }
    return *pattern == L'\0';
}

static int string_match(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"ainqr";
    const struct woption long_options[] =
    {
        { L"all", no_argument, 0, 'a'},
        { L"ignore-case", no_argument, 0, 'i'},
        { L"index", no_argument, 0, 'n'},
        { L"query", no_argument, 0, 'q'},
        { L"regex", no_argument, 0, 'r'},
        0, 0, 0, 0
    };

    bool opt_all = false;
    bool opt_ignore_case = false;
    bool opt_index = false;
    bool opt_query = false;
    bool opt_regex = false;
    woptind = 0;
    for (;;)
    {
        int c = wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'a':
                opt_all = true;
                break;

            case 'i':
                opt_ignore_case = true;
                break;

            case 'n':
                opt_index = true;
                break;

            case 'q':
                opt_query = true;
                break;

            case 'r':
                opt_regex = true;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = woptind;
    wchar_t *pattern = argv[i++];
    int args = argc - i;
    int matches = 0;
    for (; i < argc; i++)
    {
        if (opt_regex)
        {
            string_fatal_error(_(L"string match --regex: not yet implemented"));
            return BUILTIN_STRING_ERROR;
        }
        else
        {
            bool match = string_match_wildcard(pattern, argv[i], opt_ignore_case);
            if (match)
            {
                matches++;
            }
            if (!opt_query)
            {
                if (opt_index)
                {
                    if (match || opt_all)
                    {
                        append_format(stdout_buffer, L"%lc\n", match ? L'1' : L'0');
                    }
                }
                else if (match)
                {
                    append_format(stdout_buffer, L"%ls\n", argv[i]);
                }
            }
            if (match && !opt_all)
            {
                break;
            }
        }
    }

    return opt_all ? (matches < args) : (matches == 0);
}

static int string_replace(parser_t &parser, int argc, wchar_t **argv)
{
    string_fatal_error(_(L"string replace: not yet implemented"));
    return BUILTIN_STRING_ERROR;
}

static int string_split(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":m:r";
    const struct woption long_options[] =
    {
        { L"max", required_argument, 0, 'm'},
        { L"right", no_argument, 0, 'r'},
        0, 0, 0, 0
    };

    int max = 0;
    bool right = false;
    woptind = 0;
    for (;;)
    {
        int c = wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'm':
                max = int(wcstol(woptarg, 0, 10));
                break;

            case 'r':
                right = true;
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    if (argc < woptind + 1)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int i = woptind;
    const wchar_t *sep = argv[i++];
    int seplen = wcslen(sep);
    int scount = 0;
    if (right)
    {
        for (; i < argc; i++)
        {
            std::list<wcstring> splits;
            wchar_t *end = argv[i] + wcslen(argv[i]);
            wchar_t *cur = end - seplen;
            if (seplen > 0)
            {
                while (cur >= argv[i] && (max == 0 || scount < max))
                {
                    if (wcsncmp(cur, sep, seplen) == 0)
                    {
                        splits.push_front(wcstring(cur + seplen, end - cur - seplen).c_str());
                        end = cur;
                        scount++;
                    }
                    cur--;
                }
                splits.push_front(wcstring(argv[i], end - argv[i]).c_str());
            }
            std::list<wcstring>::const_iterator si = splits.begin();
            while (si != splits.end())
            {
                append_format(stdout_buffer, L"%ls\n", (*si).c_str());
                si++;
            }
        }
    }
    else
    {
        for (; i < argc; i++)
        {
            wchar_t *cur = argv[i];
            while (cur != 0)
            {
                wchar_t *ptr = (seplen == 0 || (max > 0 && scount >= max)) ? 0 : wcsstr(cur, sep);
                if (ptr == 0)
                {
                    append_format(stdout_buffer, L"%ls\n", cur);
                    cur = 0;
                }
                else
                {
                    append_format(stdout_buffer, L"%ls\n", wcstring(cur, ptr - cur).c_str());
                    cur = ptr + seplen;
                    scount++;
                }
            }
        }
    }

    return BUILTIN_STRING_OK;
}

static int string_sub(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":l:s:";
    const struct woption long_options[] =
    {
        { L"length", required_argument, 0, 'l'},
        { L"start", required_argument, 0, 's'},
        0, 0, 0, 0
    };

    int start = 0;
    int length = -1;
    woptind = 0;
    for (;;)
    {
        int c = wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'l':
                length = int(wcstol(woptarg, 0, 10));
                if (length < 0)
                {
                    string_fatal_error(L"%ls: Invalid length value\n", argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case 's':
                start = int(wcstol(woptarg, 0, 10));
                if (start == 0)
                {
                    string_fatal_error(L"%ls: Invalid start value\n", argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int result = 0;
    for (int i = woptind; i < argc; i++)
    {
        wcstring::size_type pos = 0;
        wcstring::size_type count = wcstring::npos;
        wcstring s(argv[i]);
        if (start > 0)
        {
            pos = start - 1;
        }
        else if (start < 0)
        {
            wcstring::size_type n = -start;
            pos = n > s.size() ? 0 : s.size() - n;
        }
        if (pos > s.size())
        {
            pos = s.size();
        }

        if (length >= 0)
        {
            count = length;
        }
        if (pos + count > s.size())
        {
            count = wcstring::npos;
        }

        append_format(stdout_buffer, L"%ls\n", s.substr(pos, count).c_str());
    }

    return result;
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
/*static*/ int builtin_string(parser_t &parser, wchar_t **argv)
{
    int argc = builtin_count_args(argv);
    if (argc <= 1)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
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
