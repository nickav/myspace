void string_advance(String *str, i64 amount)
{
    if (amount > str->count) amount = str->count;

    str->data += amount;
    str->count -= amount;
}

String string_from_month(Month month)
{
    static String string_table[] = {
        S(""),
        S("January"),
        S("February"),
        S("March"),
        S("April"),
        S("May"),
        S("June"),
        S("July"),
        S("August"),
        S("September"),
        S("October"),
        S("November"),
        S("December")
    };

    String result = {0};
    if (month < count_of(string_table))
    {
        result = string_table[month];
    }
    return result;
}

String minify_css(String str)
{
    u8 *data = PushArray(temp_arena(), u8, str.count);
    u8 *at = data;
    u8 *end = data + str.count;

    bool did_write_char = false;
    while (str.count)
    {
        char it = str[0];

        // NOTE(nick): eat comments
        if (it == '/' && str[1] == '*')
        {
            string_advance(&str, 2);
            while (str.count && !string_starts_with(str, S("*/")))
            {
                string_advance(&str, 1);
            }
            string_advance(&str, 2);
            continue;
        }

        // NOTE(nick): eat whitespace
        if (it == '\r' || it == '\n')
        {
            str = string_eat_whitespace(str);
            continue;
        }

        // NOTE(nick): handle strings
        if (it == '"' || it == '\'')
        {
            char closing_char = it;

            *at++ = it;
            string_advance(&str, 1);

            while (str.count > 0 && str.data[0] != closing_char)
            {
                *at++ = str.data[0];
                string_advance(&str, 1);
            }

            assert(str.count > 0); // @Robustness

            *at++ = str.data[0];
            string_advance(&str, 1);
            continue;
        }

        char last_written_char = did_write_char ? at[-1] : '\0';

        if (it == ' ')
        {
            // NOTE(nick): only emit max 1 consecutive space
            if (last_written_char == ' ')
            {
                string_advance(&str, 1);
                continue;
            }

            if (last_written_char == ':')
            {
                string_advance(&str, 1);
                continue;
            }

            if (last_written_char == ';')
            {
                string_advance(&str, 1);
                continue;
            }

            if (last_written_char == ',')
            {
                string_advance(&str, 1);
                continue;
            }

            if (last_written_char == '>')
            {
                string_advance(&str, 1);
                continue;
            }

            if (last_written_char == '{')
            {
                string_advance(&str, 1);
                continue;
            }
        }

        if (last_written_char == ' ')
        {
            if (it == '{')
            {
                at --;
            }

            if (it == '>')
            {
                at --;
            }
        }

        if (it == '}' && last_written_char == ';')
        {
            at --;
        }

        string_advance(&str, 1);
        *at++ = it;
        did_write_char = true;
    }

    return string_make(data, at - data);
}

String minify_js(String str)
{
    u8 *data = PushArray(temp_arena(), u8, str.count);
    u8 *at = data;
    u8 *end = data + str.count;

    bool did_write_char = false;
    while (str.count)
    {
        char it = str[0];

        if (it == '\r' || it == '\n')
        {
            str = string_eat_whitespace(str);
            continue;
        }

        char last_written_char = did_write_char ? at[-1] : '\0';

        if (it == ' ')
        {
            // NOTE(nick): only emit max 1 consecutive space
            if (last_written_char == ' ')
            {
                string_advance(&str, 1);
                continue;
            }
        }


        string_advance(&str, 1);
        *at++ = it;
        did_write_char = true;
    }

    return string_make(data, at - data);
}

u64 ParsePostID(String name)
{
    name = path_strip_extension(name);
    name = path_filename(name);

    i64 idx = string_find(name, S("_"), 0, 0);
    name = string_slice(name, 0, idx);
    return (i64)string_to_i64(name, 10);
}

Date_Time ParsePostDate(String str)
{
    Date_Time result = {};

    str = string_trim_whitespace(str);

    String part0 = str;
    String part1 = {};

    i64 space_index = string_index(str, S(" "), 0);
    if (space_index >= 0)
    {
        part0 = string_slice(str, 0, space_index);
        part1 = string_trim_whitespace(string_slice(str, space_index, str.count));
    }

    if (part0.count > 0)
    {
        // @Robustness: handle spaces between date separators

        if (part0.count == 10 && part0[4] == '-' && part0[7] == '-')
        {
            // NOTE(nick): SQL date format
            auto yyyy = string_slice(part0, 0, 4);
            auto mm   = string_slice(part0, 5, 7);
            auto dd   = string_slice(part0, 8, 10);

            result.year = string_to_i64(yyyy, 10);
            result.mon  = string_to_i64(mm, 10);
            result.day  = string_to_i64(dd, 10);
        }
        else if (part0.count == 10 && !char_is_digit(part0[2]) && !char_is_digit(part0[5]))
        {
            // NOTE(nick): american date format
            auto mm   = string_slice(part0, 0, 2);
            auto dd   = string_slice(part0, 3, 5);
            auto yyyy = string_slice(part0, 6, 10);

            result.year = string_to_i64(yyyy, 10);
            result.mon  = string_to_i64(mm, 10);
            result.day  = string_to_i64(dd, 10);
        }
    }

    if (part1.count > 0)
    {
        i64 i0 = string_index(part1, S(":"), 0);
        if (i0 > 0)
        {
            auto hh = string_slice(part1, 0, i0);
            auto mm = String{};
            auto ss = String{};

            i64 i1 = string_index(part1, S(":"), i0 + 1);
            if (i1 > 0)
            {
                mm = string_slice(part1, i0 + 1, i1);
                ss = string_slice(part1, i1 + 1, part1.count);
            }
            else
            {
                mm = string_slice(part1, i0, part1.count);
            }

            result.hour = string_to_i64(hh, 10);
            result.min  = string_to_i64(mm, 10);
            result.sec  = string_to_i64(ss, 10);
        }
    }

    return result;
}

String to_rss_date_string(Date_Time it)
{
    auto mon = string_slice(string_from_month((Month)it.mon), 0, 3);
    return sprint("%02d %.*s %d %02d:%02d:%02d +0000", it.day, LIT(mon), it.year, it.hour, it.min, it.sec);
}

String pretty_date(Date_Time it)
{
    auto mon = string_from_month(cast(Month)it.mon);
    // 02 January, 2021
    return sprint("%02d %S %d", it.day, mon, it.year);
}

void write(Arena *arena, char *format, ...)
{
    va_list args;
    va_start(args, format);
    string_printv(arena, format, args);
    va_end(args);
}

static char unsigned OverhangMask[32] =
{
    255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,
    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
};

static char unsigned DefaultSeed[16] =
{
    178, 201, 95, 240, 40, 41, 143, 216,
    2, 209, 178, 114, 232, 4, 176, 188
};

// TODO(nick): port this to MacOS

struct Asset_Hash
{
    __m128i value;
};

static Asset_Hash ComputeAssetHash(char unsigned *At, size_t Count, char unsigned *Seedx16)
{
    /* TODO(casey):
        Consider and test some alternate hash designs.  The hash here
        was the simplest thing to type in, but it is not necessarily
        the best hash for the job.  It may be that less AES rounds
        would produce equivalently collision-free results for the
        problem space.  It may be that non-AES hashing would be
        better.  Some careful analysis would be nice.
    */

    // TODO(casey): Double-check exactly the pattern
    // we want to use for the hash here

    Asset_Hash Result = {0};

    // TODO(casey): Should there be an IV?
    __m128i HashValue = _mm_cvtsi64_si128(Count);
    HashValue = _mm_xor_si128(HashValue, _mm_loadu_si128((__m128i *)Seedx16));

    size_t ChunkCount = Count / 16;
    while(ChunkCount--)
    {
        __m128i In = _mm_loadu_si128((__m128i *)At);
        At += 16;

        HashValue = _mm_xor_si128(HashValue, In);
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    }

    size_t Overhang = Count % 16;

#if 0
    __m128i In = _mm_loadu_si128((__m128i *)At);
#else
    // TODO(casey): This needs to be improved - it's too slow, and the #if 0 branch would be nice but can't
    // work because of overrun, etc.
    char Temp[16];
    
    #if OS_WINDOWS
    __movsb((unsigned char *)Temp, At, Overhang);
    #else
    memcpy(Temp, At, Overhang);
    #endif

    __m128i In = _mm_loadu_si128((__m128i *)Temp);
#endif
    In = _mm_and_si128(In, _mm_loadu_si128((__m128i *)(OverhangMask + 16 - Overhang)));
    HashValue = _mm_xor_si128(HashValue, In);
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());

    Result.value = HashValue;

    return Result;
}

static bool AssetHashesAreEqual(Asset_Hash a, Asset_Hash b)
{
    __m128i compare = _mm_cmpeq_epi32(a.value, b.value);
    int result = _mm_movemask_epi8(compare) == 0xffff;
    return result;
}