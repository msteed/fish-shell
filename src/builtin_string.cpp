/** \file builtin_string.cpp
  Implementation of the string builtin.
*/

// XXX string replace --regex
// XXX include what you use
// XXX consider changes to commands
//  - match -n: need to report start & length
//  - split: split on individual characters if SEP is empty?
// XXX docs
//  - review for completeness
//  - check formatting
//  - does help work as expected?
// XXX misc
//  - configure.ac looks for regex.h but this header is not included anywhere

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
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

struct match_options_t
{
    bool ignore_case;
    bool index;
    bool quiet;
    int max;

    match_options_t(): ignore_case(false), index(false), quiet(false), max(0) { }
};

class string_matcher_t
{
protected:
    match_options_t opts;
    const wchar_t *argv0;
    int nmatch;

public:
    string_matcher_t(const wchar_t *argv0_, const match_options_t &opts_)
        : opts(opts_), argv0(argv0_), nmatch(0)
    { }

    virtual ~string_matcher_t() {}
    virtual bool report_matches(const wchar_t *arg) = 0;
    int match_count() { return nmatch; }
};

class wildcard_matcher_t: public string_matcher_t
{
    const wchar_t *pattern;

public:
    wildcard_matcher_t(const wchar_t *argv0_, const match_options_t &opts_, const wchar_t *pattern_)
        : string_matcher_t(argv0_, opts_),
          pattern(pattern_)
    { }

    bool arg_matches(const wchar_t *pat, const wchar_t *arg)
    {
        for (; *arg != L'\0'; arg++, pat++)
        {
            switch (*pat)
            {
                case L'?':
                    break;

                case L'*':
                    // skip redundant *
                    while (*pat == L'*')
                    {
                        pat++;
                    }

                    // * at end matches whatever follows
                    if (*pat == L'\0')
                    {
                        return true;
                    }

                    while (*arg != L'\0')
                    {
                        if (arg_matches(pat, arg++))
                        {
                            return true;
                        }
                    }
                    return false;

                case L'[':
                {
                    bool negate = false;
                    if (*++pat == L'^')
                    {
                        negate = true;
                        pat++;
                    }

                    bool match = false;
                    wchar_t argch = opts.ignore_case ? towlower(*arg) : *arg;
                    wchar_t patch, patch2;
                    while ((patch = *pat++) != L']')
                    {
                        if (patch == L'\0')
                        {
                            return false; // no closing ]
                        }
                        if (*pat == L'-' && (patch2 = *(pat + 1)) != L'\0' && patch2 != L']')
                        {
                            if (opts.ignore_case ? towlower(patch) <= argch && argch <= towlower(patch2)
                                                 : patch <= argch && argch <= patch2)
                            {
                                match = true;
                            }
                            pat += 2;
                        }
                        else if (patch == argch)
                        {
                            match = true;
                        }
                    }
                    if (match == negate)
                    {
                        return false;
                    }
                    pat--;
                    break;
                }

                case L'\\':
                    if (*(pat + 1) != L'\0')
                    {
                        pat++;
                    }
                    // fall through

                default:
                    if (opts.ignore_case ? towlower(*arg) != towlower(*pat) : *arg != *pat)
                    {
                        return false;
                    }
                    break;
            }
        }
        // arg is exhausted - it's a match only if pattern is as well
        while (*pat == L'*')
        {
            pat++;
        }
        return *pat == L'\0';
    }

    bool report_matches(const wchar_t *arg)
    {
        if (opts.max == 0 || nmatch < opts.max)
        {
            bool match = arg_matches(pattern, arg);
            if (match)
            {
                nmatch++;
            }
            if (!opts.quiet)
            {
                if (opts.index)
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
        return true;
    }
};

class pcre2_matcher_t: public string_matcher_t
{
    pcre2_code *regex;
    pcre2_match_data *match;

public:
    pcre2_matcher_t(const wchar_t *argv0_, const match_options_t &opts_, const wchar_t *pattern)
        : string_matcher_t(argv0_, opts_),
          regex(0), match(0)
    {
        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        // Disable some sequences that can lead to security problems
        uint32_t pcre2_options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        regex = pcre2_compile(
            PCRE2_SPTR(pattern),
            PCRE2_ZERO_TERMINATED,
            pcre2_options | (opts.ignore_case ? PCRE2_CASELESS : 0),
            &err_code,
            &err_offset,
            0);
        if (regex == 0)
        {
            string_fatal_error(_(L"%ls: Regular expression compilation failed at offset %d"),
                argv0, int(err_offset));
            return;
        }

        match = pcre2_match_data_create_from_pattern(regex, 0);
        if (match == 0)
        {
            DIE_MEM();
        }
    }

    ~pcre2_matcher_t()
    {
        if (match != 0)
        {
            pcre2_match_data_free(match);
        }
        if (regex != 0)
        {
            pcre2_code_free(regex);
        }
    }

    int report_match(const wchar_t *arg, int pcre2_rc)
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
            if (!opts.quiet)
            {
                if (begin != PCRE2_UNSET && end != PCRE2_UNSET)
                {
                    if (opts.index)
                    {
                        stdout_buffer += to_string(begin + 1);
                    }
                    else if (end > begin) // may have end < begin if \K is used
                    {
                        stdout_buffer += wcstring(&arg[begin], end - begin);
                    }
                    stdout_buffer += L'\n';
                }
            }
        }
        return 1;
    }

    bool report_matches(const wchar_t *arg)
    {
        // A return value of true means all is well (even if no matches were
        // found), false indicates an unrecoverable error.
        if (regex == 0)
        {
            // pcre2_compile() failed
            return false;
        }

        if (opts.max > 0 && nmatch >= opts.max)
        {
            return true;
        }

        // See pcre2demo.c for an explanation of this logic
        PCRE2_SIZE arglen = wcslen(arg);
        int rc = report_match(arg, pcre2_match(regex, PCRE2_SPTR(arg), arglen, 0, 0, match, 0));
        if (rc < 0)
        {
            // pcre2 match error
            return false;
        }
        if (rc == 0)
        {
            // no match
            return true;
        }
        nmatch++;

        // Report any additional matches
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
        while (opts.max == 0 || nmatch < opts.max)
        {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1]; // Start at end of previous match
            PCRE2_SIZE old_offset = pcre2_get_startchar(match);
            if (offset <= old_offset)
            {
                offset = old_offset + 1;
            }

            if (ovector[0] == ovector[1])
            {
                if (ovector[0] == arglen)
                {
                    break;
                }
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex, PCRE2_SPTR(arg), arglen, offset, options, match, 0));
            if (rc < 0)
            {
                return false;
            }
            if (rc == 0)
            {
                if (options == 0)
                {
                    // All matches found
                    break;
                }
                ovector[1] = offset + 1;
                continue;
            }
            nmatch++;
        }
        return true;
    }
};

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

    match_options_t opts;
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
                opts.ignore_case = true;
                break;

            case 'm':
                opts.max = int(wcstol(w.woptarg, 0, 10));
                break;

            case 'n':
                opts.index = true;
                break;

            case 'q':
                opts.quiet = true;
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

    string_matcher_t *matcher;
    if (regex)
    {
        matcher = new pcre2_matcher_t(argv[0], opts, pattern);
    }
    else
    {
        matcher = new wildcard_matcher_t(argv[0], opts, pattern);
    }

    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        if (!matcher->report_matches(arg))
        {
            delete matcher;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = matcher->match_count() > 0 ? 0 : 1;
    delete matcher;
    return rc;
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
