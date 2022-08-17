struct RSS_Entry
{
    String title;
    String description;
    Date_Time pub_date;
    String link;
    String category;
};

String minify_css(String str)
{
    u8 *data = push_array(temp_arena(), u8, str.count);
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
            string_eat_whitespace(&str);
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

    return make_string(data, at - data);
}

u64 ParsePostID(String name) {
    i64 i0 = string_index(name, S("_"));
    if (i0 >= 0)
    {
        i64 i1 = string_index(name, S("."), i0 + 1);
        if (i1 >= 0)
        {
            auto str = string_slice(name, i0 + 1, i1);
            return (i64)string_to_i64(str);
        }
    }

    return 0;
}

Date_Time ParsePostDate(String str)
{
    Date_Time result = {};

    string_trim_whitespace(&str);

    String part0 = str;
    String part1 = {};

    i64 space_index = string_index(str, S(" "));
    if (space_index >= 0)
    {
        part0 = string_slice(str, 0, space_index);
        part1 = string_trim_whitespace(string_slice(str, space_index));
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

            result.year = string_to_i64(yyyy);
            result.mon  = string_to_i64(mm);
            result.day  = string_to_i64(dd);
        }
        else if (part0.count == 10 && !char_is_digit(part0[2]) && !char_is_digit(part0[5]))
        {
            // NOTE(nick): american date format
            auto mm   = string_slice(part0, 0, 2);
            auto dd   = string_slice(part0, 3, 5);
            auto yyyy = string_slice(part0, 6, 10);

            result.year = string_to_i64(yyyy);
            result.mon  = string_to_i64(mm);
            result.day  = string_to_i64(dd);
        }
    }

    if (part1.count > 0)
    {
        i64 i0 = string_index(part1, ':');
        if (i0 > 0)
        {
            auto hh = string_slice(part1, 0, i0);
            auto mm = String{};
            auto ss = String{};

            i64 i1 = string_index(part1, ':', i0 + 1);
            if (i1 > 0)
            {
                mm = string_slice(part1, i0 + 1, i1);
                ss = string_slice(part1, i1 + 1);
            }
            else
            {
                mm = string_slice(part1, i0);
            }

            result.hour = string_to_i64(hh);
            result.min  = string_to_i64(mm);
            result.sec  = string_to_i64(ss);
        }
    }

    return result;
}

String to_rss_date_string(Date_Time it)
{
    auto mon = string_slice(string_from_month(cast(Month)it.mon), 0, 3);
    return sprint("%02d %S %d %02d:%02d:%02d +0000", it.day, mon, it.year, it.hour, it.min, it.sec);
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

String generate_rss_feed(Page_Meta meta, Array<RSS_Entry> items)
{
    auto arena = arena_make_from_memory(megabytes(1));

    auto pub_date = to_rss_date_string(os_get_current_time_in_utc());

    write(&arena, "<?xml version='1.0' encoding='UTF-8'?>\n");

    write(&arena, "<rss version='2.0' xmlns:atom='http://www.w3.org/2005/Atom'>\n");
    write(&arena, "<channel>\n");
    write(&arena, "\n");

    write(&arena, "<title>%S</title>\n", meta.title);
    write(&arena, "<link>%S</link>\n", meta.url);
    write(&arena, "<description>%S</description>\n", meta.description);
    write(&arena, "<pubDate>%S</pubDate>\n", pub_date);
    write(&arena, "<lastBuildDate>%S</lastBuildDate>\n", pub_date);
    write(&arena, "<language>en-us</language>\n");
    write(&arena, "<image><url>%S</url></image>\n", meta.image);
    write(&arena, "\n");

    For (items)
    {
        write(&arena, "<item>\n");
        write(&arena, "<title>%S</title>\n", it.title);
        write(&arena, "<description>%S</description>\n", it.description);
        write(&arena, "<pubDate>%S</pubDate>\n", to_rss_date_string(it.pub_date));
        write(&arena, "<link>%S</link>\n", it.link);
        write(&arena, "<guid isPermaLink='true'>%S</guid>\n", it.link);
        write(&arena, "<category>%S</category>\n", it.category);
        write(&arena, "</item>\n");
        write(&arena, "\n");
    }

    write(&arena, "</channel>\n");
    write(&arena, "</rss>\n");

    return arena_to_string(&arena);
}
