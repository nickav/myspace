#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "deps/stb_sprintf.h"
#include "na.h"

#include "assets.cpp"
#include "parser.cpp"

struct Build_Config {
    String asset_dir;
    String output_dir;
};

struct Html_Meta {
    String site_name;
    String twitter_handle;

    String title;
    String description;
    String image;
    String og_type;
    String url;
};

struct Social_Icon {
    String name;
    String icon_name;
    String link_url;
};

static Build_Config config = {};


void Write(Arena *arena, char *format, ...)
{
    va_list args;
    va_start(args, format);
    string_printv(arena, format, args);
    string_print(arena, "\n");
    va_end(args);
}

void BeginHtmlPage(Arena *arena, Html_Meta meta, String style = {}, String head = {})
{
    Write(arena, "<!doctype html>");
    Write(arena, "<html lang='en'>");
    Write(arena, "<head>");
    Write(arena, "<meta charset='utf-8' />");
    Write(arena, "<meta name='viewport' content='width=device-width, initial-scale=1' />");

    Write(arena, "<title>%S</title>", meta.title);
    Write(arena, "<meta name='description' content='%S' />", meta.description);

    Write(arena, "<meta itemprop='name' content='%S'>", meta.title);
    Write(arena, "<meta itemprop='description' content='%S'>", meta.description);
    Write(arena, "<meta itemprop='image' content='%S'>", meta.image);

    Write(arena, "<meta property='og:title' content='%S' />", meta.title);
    Write(arena, "<meta property='og:description' content='%S' />", meta.description);
    Write(arena, "<meta property='og:type' content='%S' />", meta.og_type);
    Write(arena, "<meta property='og:url' content='%S' />", meta.url);
    Write(arena, "<meta property='og:site_name' content='%S' />", meta.site_name);
    Write(arena, "<meta property='og:locale' content='en_us' />");

    Write(arena, "<meta name='twitter:card' content='summary' />");
    Write(arena, "<meta name='twitter:title' content='%S' />", meta.title);
    Write(arena, "<meta name='twitter:description' content='%S' />", meta.description);
    Write(arena, "<meta name='twitter:image' content='%S' />", meta.image);
    Write(arena, "<meta name='twitter:site' content='%S' />", meta.twitter_handle);

    if (style.count) Write(arena, "<style type='text/css'>%S</style>", style);

    if (head.count) Write(arena, "%S", head);

    Write(arena, "</head>");
    Write(arena, "<body>");
}

void EndHtmlPage(Arena *arena)
{
    Write(arena, "</body>");
    Write(arena, "</html>");
}

String MinifyCSS(String str)
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

String to_rss_date_string(Date_Time it)
{
    auto mon = string_slice(string_from_month(cast(Month)it.mon), 0, 3);
    return sprint("%02d %S %d %02d:%02d:%02d +0000", it.day, mon, it.year, it.hour, it.min, it.sec);
}

struct RSS_Entry {
    String title;
    String description;
    Date_Time pub_date;
    String link;
    String category;
};

// <link rel="alternate" type="application/rss+xml" title="Nick Aversano" href="http://nickav.co/feed.xml" />
String GenerateRSSFeed(Html_Meta meta, Array<RSS_Entry> items)
{
    auto arena = arena_make_from_backing_memory(os_virtual_memory(), megabytes(1));

    auto pub_date = to_rss_date_string(os_get_current_time_in_utc());

    Write(&arena, "<?xml version='1.0' encoding='UTF-8'?>");

    Write(&arena, "<rss version='2.0' xmlns:atom='http://www.w3.org/2005/Atom'>");
    Write(&arena, "<channel>");
    Write(&arena, "\n");

    Write(&arena, "<title>%S</title>", meta.title);
    Write(&arena, "<link>%S</link>", meta.url);
    Write(&arena, "<description>%S</description>", meta.description);
    Write(&arena, "<pubDate>%S</pubDate>", pub_date);
    Write(&arena, "<lastBuildDate>%S</lastBuildDate>", pub_date);
    Write(&arena, "<language>en-us</language>");
    Write(&arena, "<image><url>%S</url></image>", meta.image);
    Write(&arena, "\n");

    For (items)
    {
        Write(&arena, "<item>");
        Write(&arena, "<title>%S</title>", it.title);
        Write(&arena, "<description>%S</description>", it.description);
        Write(&arena, "<pubDate>%S</pubDate>", to_rss_date_string(it.pub_date));
        Write(&arena, "<link>%S</link>", it.link);
        Write(&arena, "<guid isPermaLink='true'>%S</guid>", it.link);
        Write(&arena, "<category>%S</category>", it.category);
        Write(&arena, "</item>");
        Write(&arena, "\n");
    }

    Write(&arena, "</channel>");
    Write(&arena, "</rss>");

    return arena_to_string(&arena);
}

// @Robustnes: should we add __FILE__ to this too?
#define local_label(name) __FUNCTION__ ## name

String GenerateStringFromTemplate(String html_template, Slice<String> replacement_pairs)
{
    Arena arena = arena_make_from_backing_memory(os_virtual_memory(), megabytes(1));

    String at = html_template;

    while (true)
    {
        local_label(loop):
        if (!(at.count > 2)) break;

        if (at.data[0] == '{' && at.data[1] == '{')
        {
            for (i64 i = 0; i < replacement_pairs.count; i += 2)
            {
                auto rfrom = replacement_pairs.data[i + 0];
                auto rto = replacement_pairs.data[i + 1];

                if (string_starts_with(at, rfrom))
                {
                    arena_write(&arena, rto.data, rto.count);
                    string_advance(&at, rfrom.count);
                    goto local_label(loop);
                }
            }
        }

        arena_write(&arena, at.data, 1);
        string_advance(&at, 1);
    }

    arena_write(&arena, at.data, at.count);
    string_advance(&at, at.count);

    return arena_to_string(&arena);
}


void WriteSocialIcons(Arena *arena, Slice<Social_Icon> icons)
{
    Write(arena, "<div class='flex-row csx-16'>");

    for (int i = 0; i < icons.count; i++)
    {
        auto it = icons[i];
        auto svg = FindAssetByName(it.icon_name);

        if (!svg) {
            print("[warning] Could not find social icon: %S\n", it.icon_name);
            continue;
        }

        Write(arena, "<a href='%S' title='%S' target='_blank'>", it.link_url, it.name);
        Write(arena, "<div style='width: 24px; height: 24px;'>%S</div>", svg->data);
        Write(arena, "</a>");
    }

    Write(arena, "</div>");
}

void WriteHeader(Arena *arena, String site_name, Slice<Social_Icon> icons)
{
    Write(arena, "<header class='flex-row h-64 center padx-16' style='background: #333;'>");

    Write(arena, "<div class='flex-row center-y w-1280'>");
    Write(arena, "<div class='flex-full'><h1>%S</h1></div>", site_name);
    WriteSocialIcons(arena, icons);
    Write(arena, "</div>");

    Write(arena, "</header>");
}

String HighlightCode(String at)
{
    Arena arena = arena_make_from_backing_memory(os_virtual_memory(), megabytes(1));

    auto tokens = tokenize(at);
    dump(tokens.count);
    For (tokens) {
        print("Token { type=%d, text=%S }\n", it.type, it.value);
    }

    return arena_to_string(&arena);
}

void WriteCodeBlock(Arena *arena, String code)
{
    Write(arena, "<pre class='pad-8' style='background: #222; color: #fff; overflow-x: auto'><code>%S</code></pre>", HighlightCode(code));
}

static String base64_encoder_src = S(R"STR(
// Groups of 6 bits (6 bits have a maximum of 26 = 64 different binary values)
/*
    Hey this is a multiline comment
    and stuff
*/
String base64_encode(String buffer)
{
    static char encode_table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    u8 *data = push_array(temp_arena(), u8, 4 * ((buffer.count + 2) / 3));

    int i = 0;
    for (; i < buffer.count - 2; i += 3)
    {
        u8 b0 = buffer.data[i + 0];
        u8 b1 = buffer.data[i + 1];
        u8 b2 = buffer.data[i + 2];

        *at++ = encode_table[(b0 >> 2)];
        *at++ = encode_table[((b0 & 0x3) << 4) | (b1 >> 4)];
        *at++ = encode_table[((b1 & 0xf) << 2) | (b2 >> 6)];
        *at++ = encode_table[(b2 & 0x3f)];
    }

    // Remainder
    if (i < buffer.count)
    {
        u8 b0 = buffer.data[i + 0];
        *at++ = encode_table[(b0 >> 2)];

        if (i == buffer.count - 2)
        {
            u8 b1 = buffer.data[i + 1];
            *at++ = encode_table[((b0 & 0x3) << 4) | (b1 >> 4)];
            *at++ = encode_table[((b1 & 0xf) << 2)];
        }
        else
        {
            *at++ = encode_table[((b0 & 0x3) << 4)];
            *at++ = '=';
        }

        *at++ = '=';
    }

    return make_string(data, at - data);
}
)STR");


int main() {
    os_init();
    auto start_time = os_time_in_miliseconds();

    auto build_dir = os_get_executable_directory();
    auto project_root = path_dirname(build_dir);

    // Config
    config = {};
    config.asset_dir  = path_join(project_root, S("assets"));
    config.output_dir = path_join(build_dir, S("bin"));

    // Site
    Html_Meta meta = {};
    meta.site_name = S("Nick Aversano");
    meta.twitter_handle = S("@nickaversano");

    meta.title = S("Nick Aversano");
    meta.description = S("Nicks cool home page");
    meta.image = S(" ");
    meta.og_type = S("article");
    meta.url = S("/");

    Array<RSS_Entry> items = {};
    RSS_Entry item0 = {};
    item0.title = S("hello test");
    item0.description = S("some cool description");
    item0.pub_date = os_get_current_time_in_utc();
    item0.link = S("/foo/bar");
    item0.category = S("article");
    array_push(&items, item0);


    Social_Icon twitch_icon = {
        S("Twitch"),
        S("twitch.svg"),
        S("https://twitch.tv/naversano")
    };

    Social_Icon twitter_icon = {
        S("Twitter"),
        S("twitter.svg"),
        S("https://www.twitter.com/nickaversano")
    };

    Social_Icon github_icon = {
        S("Github"),
        S("github.svg"),
        S("https://www.github.com/nickav")
    };

    Social_Icon social_icons[] = {
        twitch_icon,
        twitter_icon,
        github_icon,
    };

    //auto rss = GenerateRSSFeed(meta, items);
    //dump(rss);

    String replacement_pairs[] = {S("{{rep}}"), S("BOOM!")};
    GenerateStringFromTemplate(S("{{rep}}"), slice_of(replacement_pairs));

    LoadAssetsFromDisk(config.asset_dir);

    auto style = FindAssetByName(S("style.css"));
    assert(style);

    auto arena = arena_make_from_backing_memory(os_virtual_memory(), megabytes(1));

    BeginHtmlPage(&arena, meta, MinifyCSS(style->data));
    {
        WriteHeader(&arena, meta.site_name, slice_of(social_icons));
        Write(&arena, "<main class='flex-col center-x'>");
        Write(&arena, "<div class='center-x pad-16 w-800' style='background: #fff; color: black'>");
        Write(&arena, "<p>Hello, Sailor!</p>");

        WriteCodeBlock(&arena, base64_encoder_src);
        Write(&arena, "</div>");
        Write(&arena, "</main>");
    }
    EndHtmlPage(&arena);

    auto result = arena_to_string(&arena);

    os_write_entire_file(path_join(config.output_dir, S("index.html")), result);

    arena_reset(&arena);

    auto posts = GetAllPosts();
    For (posts) {
        print("Post %S\n", it.name);
        print("  id: %lld\n", it.id);
        print("  title: %S\n", it.title);
        print("  description: %S\n", it.description);
        print("  date: %S\n", to_rss_date_string(it.date));
        print("  body: %S\n", it.body);
    }


    #if 0
    dump(parse_post_date(S("2021-11-19 10:13:03")));
    dump(parse_post_date(S("11/19/2021    10:13:03")));
    dump(parse_post_date(S("11.19.2021    10:13")));
    #endif

    auto end_time = os_time_in_miliseconds();

    print("Done! Took %.2fms\n", end_time - start_time);

    return 0;
}