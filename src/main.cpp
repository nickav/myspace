#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "deps/stb_sprintf.h"
#include "na.h"

#include "assets.cpp"

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

static Build_Config config = {};

#if 0
nja_internal WCHAR * win32_UTF16FromUTF8(Arena *arena, char *buffer)
{
  int count = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
  if (!count)
  {
    return NULL;
  }

  arena_set_alignment(arena, sizeof(WCHAR));
  WCHAR *result = cast(WCHAR *)arena_push(arena, count * sizeof(WCHAR));

  if (!MultiByteToWideChar(CP_UTF8, 0, buffer, -1, result, count))
  {
    arena_pop(arena, count * sizeof(WCHAR));
    return NULL;
  }

  return result;
}
#endif


bool arena_write(Arena *arena, String buffer) {
  return arena_write(arena, cast(u8 *)buffer.data, buffer.count);
}

bool OS_WriteEntireFile(String path, String buffer)
{
    bool result = false;
    auto scratch = begin_scratch_memory();

    String16 path_w = string16_from_string(scratch.arena, path);
    HANDLE handle = CreateFileW(cast(WCHAR *)path_w.data, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    // :Win32_64BitFileIO
    assert(buffer.count <= U32_MAX);
    u32 size32 = cast(u32)buffer.count;

    DWORD bytes_written;
    if (WriteFile(handle, buffer.data, size32, &bytes_written, NULL) && (size32 == bytes_written)) {
        result = true;
    } else {
        print("[OS] Faile to write entire file!\n");
    }

    end_scratch_memory(scratch);
    return result;
}

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
    Write(arena, "<meta name='twitter:site' content='%S' /> ", meta.twitter_handle);

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

        // NOTE(nick): only emit max 1 consecutive space
        if (it == ' ' && last_written_char == ' ')
        {
            string_advance(&str, 1);
            continue;
        }

        if (it == ' ' && last_written_char == ':')
        {
            string_advance(&str, 1);
            continue;
        }

        if (it == '{' && last_written_char == ' ')
        {
            at --;
        }

        if (it == ' ' && last_written_char == ';')
        {
            string_advance(&str, 1);
            continue;
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

Date_Time parse_post_date(String str)
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
            auto mm = string_slice(part0, 5, 7);
            auto dd = string_slice(part0, 8, 10);

            result.year = string_to_i64(yyyy);
            result.mon = string_to_i64(mm);
            result.day = string_to_i64(dd);
        }
        else if (part0.count == 10 && !char_is_digit(part0[2]) && !char_is_digit(part0[5]))
        {
            // NOTE(nick): american date format
            auto mm = string_slice(part0, 0, 2);
            auto dd = string_slice(part0, 3, 5);
            auto yyyy = string_slice(part0, 6, 10);

            result.year = string_to_i64(yyyy);
            result.mon = string_to_i64(mm);
            result.day = string_to_i64(dd);
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
            result.min = string_to_i64(mm);
            result.sec = string_to_i64(ss);
        }
    }

    return result;
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

    auto rss = GenerateRSSFeed(meta, items);
    dump(rss);

    LoadAssetsFromDisk(config.asset_dir);

    auto style = FindAssetByName(S("style.css"));
    assert(style);

    auto arena = arena_make_from_backing_memory(os_virtual_memory(), megabytes(1));

    BeginHtmlPage(&arena, meta, MinifyCSS(style->data));

    auto svg = FindAssetByName(S("twitch.svg"));
    if (svg) {
        Write(&arena, "<img width='32' height='32' src='data:image/svg+xml,%S' />", svg->data);
    }
    Write(&arena, "<header>Nick Aversano</header>");
    Write(&arena, "<div>Hello, Sailor!</div>");

    EndHtmlPage(&arena);

    auto result = arena_to_string(&arena);

    os_write_entire_file(path_join(config.output_dir, S("index.html")), result);

    arena_reset(&arena);

    auto posts = GetAllPosts();
    For (posts) {
        auto id   = ParsePostID(it->name);
        auto post = ParsePost(it->data);

        print("Post %S\n", it->name);
        print("  id: %lld\n", id);
        print("  title: %S\n", post.title);
        print("  description: %S\n", post.description);
        print("  date: %S\n", post.date);
        print("  body: %S\n", post.body);

        dump(to_rss_date_string(parse_post_date(post.date)));
    }


    dump(parse_post_date(S("2021-11-19 10:13:03")));
    dump(parse_post_date(S("11/19/2021    10:13:03")));
    dump(parse_post_date(S("11.19.2021    10:13")));

    auto end_time = os_time_in_miliseconds();

    print("Done! Took %.2fms\n", end_time - start_time);

    return 0;
}