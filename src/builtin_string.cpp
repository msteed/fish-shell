/** \file builtin_string.cpp
  Implementation of the string builtin.
*/

// XXX string match --regex
// XXX string replace --regex
// XXX include what you use
// XXX consider changes to commands
//  - match -r -n: need to report start & end indices
//  - split: split on individual characters if SEP is empty?
// XXX docs
//  - review for completeness
//  - check formatting
//  - match -n: docs: index is actually a match/nomatch result
//  - does help work as expected?
// XXX misc
//  - configure.ac looks for regex.h but this header is not included anywhere

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#include "pcre2.h"

enum
{
    BUILTIN_STRING_OK = STATUS_BUILTIN_OK,
    BUILTIN_STRING_ERROR = STATUS_BUILTIN_ERROR
};

static void string_fatal_error(const wchar_t *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    wcstring errstr = vformat_string(fmt, va);
    va_end(va);

    if (!errstr.empty() && errstr.at(errstr.length() - 1) != L'\n')
    {
        errstr += L'\n';
    }

    stderr_buffer += errstr;
}

static const wchar_t *string_get_arg_stdin()
{
    static wcstring arg;

    arg.clear();

    bool eof = false;
    bool gotarg = false;

    for (;;)
    {
        wchar_t wch = L'\0';
        mbstate_t state = {};
        for (;;)
        {
            char ch = '\0';
            if (read_blocked(builtin_stdin, &ch, 1) <= 0)
            {
                eof = true;
                break;
            }
            else
            {
                size_t n = mbrtowc(&wch, &ch, 1, &state);
                if (n == size_t(-1))
                {
                    // invalid multibyte sequence
                    // XXX communicate error to caller
                    memset(&state, 0, sizeof(state));
                }
                else if (n == size_t(-2))
                {
                    // incomplete sequence, continue reading
                }
                else
                {
                    // got a complete char (could be L'\0')
                    break;
                }
            }
        }

        if (eof)
        {
            break;
        }

        if (wch == L'\n')
        {
            gotarg = true;
            break;
        }

        arg += wch;
    }

    return gotarg ? arg.c_str() : 0;
}

static const wchar_t *string_get_arg_argv(int *argidx, wchar_t **argv)
{
    return (argv && argv[*argidx]) ? argv[(*argidx)++] : 0;
}

static inline const wchar_t *string_get_arg(int *argidx, wchar_t **argv)
{
    if (isatty(builtin_stdin))
    {
        return string_get_arg_argv(argidx, argv);
    }
    else
    {
        return string_get_arg_stdin();
    }
}

static int string_escape(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"n";
    const struct woption long_options[] =
    {
        { L"no-quoted", no_argument, 0, 'n' },
        { 0, 0, 0, 0 }
    };

    escape_flags_t flags = ESCAPE_ALL;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'n':
                flags |= ESCAPE_NO_QUOTED;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nesc = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        stdout_buffer += escape(arg, flags);
        stdout_buffer += L'\n';
        nesc++;
    }

    return (nesc > 0) ? 0 : 1;
}

static int string_join(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"q";
    const struct woption long_options[] =
    {
        { L"quiet", no_argument, 0, 'q'},
        { 0, 0, 0, 0 }
    };

    bool quiet = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'q':
                quiet = true;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int njoin = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        if (!quiet)
        {
            stdout_buffer += arg;
            stdout_buffer += sep;
        }
        njoin++;
    }
    if (njoin > 0 && !quiet)
    {
        stdout_buffer.resize(stdout_buffer.length() - wcslen(sep));
        stdout_buffer += L'\n';
    }

    return (njoin > 0) ? 0 : 1;
}

static int string_length(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"q";
    const struct woption long_options[] =
    {
        { L"quiet", no_argument, 0, 'q'},
        { 0, 0, 0, 0 }
    };

    bool quiet = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'q':
                quiet = true;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int nnonempty = 0;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        size_t n = wcslen(arg);
        if (n > 0)
        {
            nnonempty++;
        }
        if (!quiet)
        {
            stdout_buffer += to_string(int(n));
            stdout_buffer += L'\n';
        }
    }

    return (nnonempty > 0) ? 0 : 1;
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
                // skip redundant *
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

static int string_report_re_match(
        const wchar_t *argv0,
        const wchar_t *arg,
        pcre2_match_data *match,
        int pcre2_rc)
{
    if (pcre2_rc == PCRE2_ERROR_NOMATCH)
    {
        return 0;
    }
    if (pcre2_rc < 0)
    {
        // see http://www.pcre.org/current/doc/html/pcre2api.html#SEC30
        string_fatal_error(_(L"%ls: Regular expression match error %d"), argv0, pcre2_rc);
        return -1;
    }
    if (pcre2_rc == 0)
    {
        // The output vector wasn't big enough. Should not happen.
        string_fatal_error(_(L"%ls: Regular expression internal error"), argv0);
        return -1;
    }
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
    for (int j = 0; j < pcre2_rc; j++)
    {
        PCRE2_SIZE begin = ovector[2*j];
        PCRE2_SIZE end = ovector[2*j + 1];
        // XXX index
        // XXX quiet
        stdout_buffer += wcstring(&arg[begin], end - begin);
        stdout_buffer += L'\n';
    }
    return 1;
}

static int string_match(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"im:nqr";
    const struct woption long_options[] =
    {
        { L"ignore-case", no_argument, 0, 'i'},
        { L"index", no_argument, 0, 'n'},
        { L"max", required_argument, 0, 'm'},
        { L"quiet", no_argument, 0, 'q'},
        { L"regex", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    int max = 0;
    bool ignore_case = false;
    bool index = false;
    bool quiet = false;
    bool regex = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'i':
                ignore_case = true;
                break;

            case 'm':
                max = int(wcstol(w.woptarg, 0, 10));
                break;

            case 'n':
                index = true;
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                regex = true;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nmatch = 0;

    if (regex)
    {
        int err = 0;
        PCRE2_SIZE eoff = 0;
        pcre2_code *re = pcre2_compile(
            PCRE2_SPTR(pattern),
            PCRE2_ZERO_TERMINATED,
            ignore_case ? PCRE2_CASELESS : 0,
            &err,
            &eoff,
            0);
        if (re == 0)
        {
            string_fatal_error(_(L"%ls: Regular expression compilation failed at offset %d"),
                argv[0], int(eoff));
            return BUILTIN_STRING_ERROR;
        }
        pcre2_match_data *match = pcre2_match_data_create_from_pattern(re, 0);
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
        if (match == 0)
        {
            DIE_MEM();
        }

        const wchar_t *arg;
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            PCRE2_SIZE arglen = wcslen(arg);
            int rc = pcre2_match(re, PCRE2_SPTR(arg), arglen, 0, 0, match, 0);
            rc = string_report_re_match(argv[0], arg, match, rc);
            if (rc < 0)
            {
                // XXX free
                return BUILTIN_STRING_ERROR;
            }
            if (rc == 0)
            {
                continue;
            }
            nmatch++;

            // Report any additional matches
            for (;;)
            {
                uint32_t options = 0; // Normally no options
                PCRE2_SIZE offset = ovector[1]; // Start at end of previous match

                // If the previous match was for an empty string, we are
                // finished if we are at the end of the subject. Otherwise,
                // arrange to run another match at the same point to see if a
                // non-empty match can be found.

                if (ovector[0] == ovector[1])
                {
                    if (ovector[0] == arglen)
                    {
                        break;
                    }
                    options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
                }

                // Run the next matching operation

                rc = pcre2_match(re, PCRE2_SPTR(arg), arglen, offset, options, match, 0);
                rc = string_report_re_match(argv[0], arg, match, rc);

                // This time, a result of NOMATCH isn't an error. If the value
                // in "options" is zero, it just means we have found all
                // possible matches, so the loop ends. Otherwise, it means we
                // have failed to find a non-empty-string match at a point
                // where there was a previous empty-string match. In this case,
                // we do what Perl does: advance the matching position by one
                // character, and continue. We do this by setting the "end of
                // previous match" offset, because that is picked up at the top
                // of the loop as the point at which to start again.

                if (rc < 0)
                {
                    // XXX free
                    return BUILTIN_STRING_ERROR;
                }
                if (rc == 0)
                {
                    if (options == 0)
                    {
                        // All matches found
                        break;
                    }
                    ovector[1] = offset + 1; // Advance one code unit
                    continue;
                }

                nmatch++;
            }

        }
        pcre2_match_data_free(match);
        pcre2_code_free(re);
    }
    else
    {
        const wchar_t *arg;
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            if (max == 0 || nmatch < max)
            {
                bool match = string_match_wildcard(pattern, arg, ignore_case);
                if (match)
                {
                    nmatch++;
                }
                if (!quiet)
                {
                    if (index)
                    {
                        stdout_buffer += match ? L'1' : L'0';
                        stdout_buffer += L'\n';
                    }
                    else if (match)
                    {
                        stdout_buffer += arg;
                        stdout_buffer += L'\n';
                    }
                }
            }
        }
    }

    return (nmatch > 0) ? 0 : 1;
}

static int string_replace(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":im:qr";
    const struct woption long_options[] =
    {
        { L"ignore-case", required_argument, 0, 'i'},
        { L"max", required_argument, 0, 'm'},
        { L"quiet", no_argument, 0, 'q'},
        { L"regex", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    int max = 0;
    bool ignore_case = false;
    bool quiet = false;
    bool regex = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'i':
                ignore_case = true;
                break;

            case 'm':
                max = int(wcstol(w.woptarg, 0, 10));
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                regex = true;
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern, *replacement;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }
    if ((replacement = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nreplace = 0;
    const wchar_t *arg;
    if (regex)
    {
        string_fatal_error(L"string replace --regex: Not yet implemented");
        return BUILTIN_STRING_ERROR;
    }
    else
    {
        int patlen = wcslen(pattern);
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            wcstring replaced;
            if (patlen == 0)
            {
                replaced = arg;
            }
            else
            {
                const wchar_t *cur = arg;
                while (*cur != L'\0')
                {
                    if ((max == 0 || nreplace < max) &&
                        (ignore_case ? wcsncasecmp(cur, pattern, patlen) : wcsncmp(cur, pattern, patlen)) == 0)
                    {
                        replaced += replacement;
                        cur += patlen;
                        nreplace++;
                    }
                    else
                    {
                        replaced += *cur;
                        cur++;
                    }
                }
            }
            if (!quiet)
            {
                stdout_buffer += replaced;
                stdout_buffer += L'\n';
            }
        }
    }

    return (nreplace > 0) ? 0 : 1;
}

static int string_split(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":m:qr";
    const struct woption long_options[] =
    {
        { L"max", required_argument, 0, 'm'},
        { L"quiet", no_argument, 0, 'q'},
        { L"right", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    int max = 0;
    bool quiet = false;
    bool right = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'm':
                max = int(wcstol(w.woptarg, 0, 10));
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                right = true;
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int seplen = wcslen(sep);
    int nsplit = 0;
    const wchar_t *arg;
    if (right)
    {
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            std::list<wcstring> splits;
            const wchar_t *end = arg + wcslen(arg);
            const wchar_t *cur = end - seplen;
            if (seplen > 0)
            {
                while (cur >= arg && (max == 0 || nsplit < max))
                {
                    if (wcsncmp(cur, sep, seplen) == 0)
                    {
                        splits.push_front(wcstring(cur + seplen, end - cur - seplen).c_str());
                        end = cur;
                        nsplit++;
                        cur -= seplen;
                    }
                    else
                    {
                        cur--;
                    }
                }
                splits.push_front(wcstring(arg, end - arg).c_str());
            }
            std::list<wcstring>::const_iterator si = splits.begin();
            while (si != splits.end())
            {
                if (!quiet)
                {
                    stdout_buffer += *si;
                    stdout_buffer += L'\n';
                }
                si++;
            }
        }
    }
    else
    {
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            const wchar_t *cur = arg;
            while (cur != 0)
            {
                const wchar_t *ptr = (seplen == 0 || (max > 0 && nsplit >= max)) ? 0 : wcsstr(cur, sep);
                if (ptr == 0)
                {
                    if (!quiet)
                    {
                        stdout_buffer += cur;
                        stdout_buffer += L'\n';
                    }
                    cur = 0;
                }
                else
                {
                    if (!quiet)
                    {
                        stdout_buffer += wcstring(cur, ptr - cur);
                        stdout_buffer += L'\n';
                    }
                    cur = ptr + seplen;
                    nsplit++;
                }
            }
        }
    }

    return (nsplit > 0) ? 0 : 1;
}

static int string_sub(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":l:qs:";
    const struct woption long_options[] =
    {
        { L"length", required_argument, 0, 'l'},
        { L"quiet", required_argument, 0, 'q'},
        { L"start", required_argument, 0, 's'},
        { 0, 0, 0, 0 }
    };

    int start = 0;
    int length = -1;
    bool quiet = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'l':
                length = int(wcstol(w.woptarg, 0, 10));
                if (length < 0)
                {
                    string_fatal_error(_(L"%ls: Invalid length value\n"), argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case 'q':
                quiet = true;
                break;

            case 's':
                start = int(wcstol(w.woptarg, 0, 10));
                if (start == 0)
                {
                    string_fatal_error(_(L"%ls: Invalid start value\n"), argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nsub = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        wcstring::size_type pos = 0;
        wcstring::size_type count = wcstring::npos;
        wcstring s(arg);
        if (start > 0)
        {
            pos = start - 1;
        }
        else if (start < 0)
        {
            wcstring::size_type n = -start;
            pos = n > s.length() ? 0 : s.length() - n;
        }
        if (pos > s.length())
        {
            pos = s.length();
        }

        if (length >= 0)
        {
            count = length;
        }
        if (pos + count > s.length())
        {
            count = wcstring::npos;
        }

        if (!quiet)
        {
            stdout_buffer += s.substr(pos, count);
            stdout_buffer += L'\n';
        }
        nsub++;
    }

    return (nsub > 0) ? 0 : 1;
}

static int string_trim(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"c:lqr";
    const struct woption long_options[] =
    {
        { L"chars", required_argument, 0, 'c'},
        { L"left", no_argument, 0, 'l'},
        { L"quiet", no_argument, 0, 'q'},
        { L"right", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    int which = 0;
    bool quiet = false;
    wcstring chars = L" \f\n\r\t";
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'c':
                chars = w.woptarg;
                break;

            case 'l':
                which |= 1;
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                which |= 2;
                break;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int ntrim = 0;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        const wchar_t *begin = arg;
        const wchar_t *end = arg + wcslen(arg);
        if (!which || (which & 1))
        {
            while (begin != end && chars.find_first_of(begin, 0, 1) != wcstring::npos)
            {
                begin++;
                ntrim++;
            }
        }
        if (!which || (which & 2))
        {
            while (begin != end && chars.find_first_of(end - 1, 0, 1) != wcstring::npos)
            {
                end--;
                ntrim++;
            }
        }
        if (!quiet)
        {
            stdout_buffer += wcstring(begin, end - begin);
            stdout_buffer += L'\n';
        }
    }

    return (ntrim > 0) ? 0 : 1;
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
    { L"trim", &string_trim },
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
