#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

#include "na.h"
#define NA_NET_IMPLEMENTATION
#include "na_net.h"

#include "helpers.h"
#include "code_parser.h"

struct Build_Config
{
    String asset_dir;
    String output_dir;
};

struct Link
{
    Link *next;

    String title;
    String href;
    String desc;
};

struct Site_Meta
{
    String name;
    String url;
    String image;
    String icon;
    String description;
    String author;
    String twitter_handle;
    String theme_color;
};

struct Page_Meta
{
    String title;
    String description;
    String image;
    String og_type;
    String url;
    String date;
    String author;
    bool draft;
};

struct Page {
    Page *next;

    Page_Meta meta;
    String slug;
    String content;
};

#define Each_Page(it, list) Page *it = list; it != NULL; it = it->next

String yaml_to_string(String str) {
    str = string_trim_whitespace(str);

    char first = str.data[0];
    if (first == '\'' || first == '"')
    {
        string_advance(&str, 1);
        if (str.data[str.count - 1] == first)
        {
            str.count -= 1;
        }
    }

    return str;
}


Site_Meta parse_site_info(String yaml)
{
    Site_Meta result = {};

    auto lines = string_split(yaml, S("\n"));
    For (lines)
    {
        i64 index = string_find(it, S(":"));
        if (index >= it.count) continue;

        auto key   = string_trim_whitespace(string_slice(it, 0, index));
        auto value = string_trim_whitespace(string_slice(it, index + 1));

        auto str = yaml_to_string(value);

        if (false) {}
        else if (string_equals(key, S("title")))          { result.name = str; }
        else if (string_equals(key, S("description")))    { result.description = str; }
        else if (string_equals(key, S("site_url")))       { result.url = str; }
        else if (string_equals(key, S("site_image")))     { result.image = str; }
        else if (string_equals(key, S("site_icon")))      { result.icon = str; }
        else if (string_equals(key, S("author")))         { result.author = str; }
        else if (string_equals(key, S("twitter_handle"))) { result.twitter_handle = str; } 
        else if (string_equals(key, S("theme_color")))    { result.theme_color = str; } 

        /*
        else if (string_equals(key, S("pages")))          { result.pages = it->first_child; } 
        else if (string_equals(key, S("links")))          { result.links = it->first_child; } 
        else if (string_equals(key, S("projects")))       { result.projects = it->first_child; } 
        else if (string_equals(key, S("social_icons")))   { result.social_icons = it->first_child; } 
        */
    }

    return result;
}

Page_Meta parse_page_meta(String yaml) {
    Page_Meta result = {};

    auto lines = string_split(yaml, S("\n"));
    For (lines)
    {
        i64 index = string_find(it, S(":"));
        if (index >= it.count) continue;

        auto key   = string_trim_whitespace(string_slice(it, 0, index));
        auto value = string_trim_whitespace(string_slice(it, index + 1));

        auto str = yaml_to_string(value);

        if (false) {}
        else if (string_equals(key, S("title")))          { result.title = str; }
        else if (string_equals(key, S("image")))          { result.image = str; }
        else if (string_equals(key, S("og_type")))        { result.og_type = str; }
        else if (string_equals(key, S("description")))    { result.description = str; }
        else if (string_equals(key, S("date")))           { result.date = str; }
        else if (string_equals(key, S("author")))         { result.author = str; }
        else if (string_equals(key, S("draft")))          { result.draft = string_to_bool(value); }
    }

    return result;
}

String absolute_url(String src)
{
    return string_concat(S("/"), src);
}

String res_url(String src)
{
    return string_concat(S("/r/"), src);
}

String post_link(Page *post)
{
    if (post != NULL)
    {
        return absolute_url(post->slug);
    }
    return {};
}


String generate_blog_rss_feed(Site_Meta site, Page *posts)
{
    auto arena = arena_alloc_from_memory(megabytes(32));
    auto pub_date = to_rss_date_string(os_get_current_time_in_utc());

    write(arena, "<?xml version='1.0' encoding='UTF-8'?>\n");

    write(arena, "<rss version='2.0' xmlns:atom='http://www.w3.org/2005/Atom'>\n");
    write(arena, "<channel>\n");
    write(arena, "\n");

    write(arena, "<title>%S</title>\n", site.name);
    write(arena, "<link>%S</link>\n", site.url);
    write(arena, "<description>%S</description>\n", site.description);
    write(arena, "<pubDate>%S</pubDate>\n", pub_date);
    write(arena, "<lastBuildDate>%S</lastBuildDate>\n", pub_date);
    write(arena, "<language>en-us</language>\n");
    write(arena, "<image><url>%S</url></image>\n", site.image);
    write(arena, "\n");

    for (Each_Page(it, posts))
    {
        auto post_slug  = it->slug;

        auto post = parse_page_meta(it->content);
        auto date = ParsePostDate(post.date);
        auto link = path_join(site.url, post_link(it));

        if (!post.og_type.count) post.og_type = S("article");

        write(arena, "<item>\n");
        write(arena, "<title>%S</title>\n", post.title);
        write(arena, "<description>%S</description>\n", post.description);
        write(arena, "<pubDate>%S</pubDate>\n", to_rss_date_string(date));
        write(arena, "<link>%S</link>\n", link);
        write(arena, "<guid isPermaLink='true'>%S</guid>\n", link);
        write(arena, "<category>%S</category>\n", post.og_type);
        write(arena, "</item>\n");
        write(arena, "\n");
    }

    write(arena, "</channel>\n");
    write(arena, "</rss>\n");

    return arena_to_string(arena);
}

String escape_html(String text)
{
    if (string_contains(text, S("<")) || string_contains(text, S(">")))
    {
        // NOTE(nick): crazy inefficient!
        // :StringReplaceHack
        auto parts = string_split(text, S("<"));
        text = string_join(parts, S("&lt;"));
        parts = string_split(text, S(">"));
        text = string_join(parts, S("&gt;"));
    }
    return text;
}

void write_image(Arena *arena, String src, String alt, String rest = {})
{
    if (string_contains(src, S("pixel"))) rest = string_concat(rest, S(" style='image-rendering:pixelated;'"));
    write(arena, "<img src='%S' alt='%S' %S/>\n", res_url(src), alt, rest);
}

void write_link(Arena *arena, String text, String href)
{
    bool is_external = string_find(href, S("://"), 0, 0) < href.count;

    if (string_equals(href, S("index"))) href = S("/");
    if (!is_external && !string_starts_with(href, S("/"))) {
        href = string_concat(S("/"), href);
    }

    auto target = is_external ? S("_blank") : S("");
    write(arena, "<a class='link' href='%S' target='%S'>%S</a>", href, target, text);
}

void write_code_block(Arena *arena, String code)
{
    string_trim_whitespace(&code);
    if (!code.count) return;

    auto tokens = c_tokenize(code);
    c_convert_tokens_to_c_like(tokens);

    write(arena, "<pre class='code'>");

    For (tokens)
    {
        auto whitespace = c_whitespace_before_token(&it, code);
        arena_write(arena, whitespace);

        if (
            it.type == C_TokenType_Identifier ||
            it.type == C_TokenType_Operator ||
            it.type == C_TokenType_Semicolon ||
            it.type == C_TokenType_Paren)
        {
            arena_write(arena, escape_html(it.value));
        }
        else
        {
            auto type = c_token_type_to_string(it.type);
            auto tok = sprint("<span class='tok-%S'>%S</span>", type, it.value);
            arena_write(arena, tok);
        }
    }

    write(arena, "</pre>");
}

void write_quote(Arena *arena, String quote)
{
    quote = string_trim_whitespace(quote);

    // @Incomplete: special handling for the citation line (last line starting with a "-")

    // NOTE(nick): sort of inefficient
    // :StringReplaceHack
    auto parts = string_split(quote, S("--"));
    quote = string_join(parts, S("—"));

    write(arena, "<blockquote class='quote'>%S</blockquote>", quote);
}


String find_yaml_frontmatter(String str) {
    if (string_starts_with(str, S("---")))
    {
        for (i32 i = 3; i < str.count - 3; i += 1) {
            if (str.data[i] == '-') {
                if (str.data[i + 1] == '-' && str.data[i + 2] == '-') {
                    return string_slice(str, 0, i + 3);
                }
            }
        }
    }

    return String{};
}


String string_normalize_newlines(String input)
{
    String result = {};
    u8 *data = PushArray(temp_arena(), u8, input.count);
    u8 *at = data;

    for (i64 i = 0; i < input.count; i ++)
    {
        char it = input.data[i];
        if (it == '\r')
        {
            continue;
        }

        *at = it;
        at ++;
    }

    return make_string(data, at - data);
}

i64 string_count_words(String str)
{
    i64 result = 0;
    while (str.count > 0)
    {
        string_eat_whitespace(&str);
        
        result += 1;

        while (str.count > 0 && !char_is_whitespace(str.data[0]))
        {
            string_advance(&str, 1);
        }
    }
    return result;
}


String markdown_to_html(String text)
{
    Arena *arena = arena_alloc_from_memory(gigabytes(1));

    text = string_normalize_newlines(text);
    return text;

    #if 0
    // @Incomplete: we also want to split by code blocks and quote blocks
    Array<String> lines = string_split(text, S("\n"));

    For (lines) {
        arena_print(arena, "<p>%S</p>", it);
    }
    #endif

    return arena_to_string(arena);
}


static String global_public_path = {};

HTTP_REQUEST_CALLBACK(request_callback)
{
    auto file = request->url;
    if (string_equals(file, S("/"))) file = S("index.html");
    if (string_ends_with(file, S("/"))) file = string_slice(file, 0, file.count - 1);

    auto ext = path_get_extension(file);
    if (!ext.count) {
        file = string_concat(file, S(".html"));
        ext = S(".html");
    }

    auto file_path = path_join(global_public_path, file);
    auto contents = os_read_entire_file(file_path);
    if (!contents.data)
    {
        response->status_code = 404;
        response->body = S("Not Found");
        return;
    }

    response->status_code = 200;
    response->body = contents;
    response->content_type = http_content_type_from_extension(ext);
}

void run_server(String server_url, String public_path)
{
    socket_init();
    
    global_public_path = public_path;
    http_server_run(server_url, request_callback);
}



int main(int argc, char **argv)
{
    os_init();

    if (argc < 2) {
        char *arg0 = argv[0];
        print("Usage: %s <data> <bin>\n", arg0);
        return -1;
    }

    //~nja: parse arguments
    auto exe_dir = os_get_executable_directory();

    char *arg0 = argv[0];
    char *arg1 = argv[1];
    char *arg2 = argv[2];

    auto data_dir   = path_resolve(exe_dir, string_from_cstr(arg1));
    auto output_dir = path_resolve(exe_dir, string_from_cstr(arg2));

    os_make_directory(output_dir);

    Site_Meta site = {};
    site.name           = S("Nick Aversano");
    site.url            = S("http://nickav.co");
    site.image          = S("");
    site.icon           = S("logo.png");
    site.description    = S("Cool new web page");
    site.author         = S("Nick Aversano");
    site.twitter_handle = S("@nickaversano");
    site.theme_color    = S("#000000");


    // @Speed: go wide on reading all data files


    auto css = os_read_entire_file(path_join(data_dir, S("style.css")));
    css = minify_css(css);

    auto js = os_read_entire_file(path_join(data_dir, S("script.js")));
    js = minify_js(js);

    //~nja: static assets
    print("[before assets] %.2fms\n", os_time_in_miliseconds());
    {
        auto public_dir = path_join(data_dir, S("public"));
        auto files = os_scan_files_recursive(public_dir);
        Forp (files)
        {
            auto from_path = path_join(public_dir, it->name);
            auto to_path = path_join(output_dir, it->name);
            auto contents = os_read_entire_file(from_path);

            os_make_directory_recursive(path_dirname(to_path));

            os_write_entire_file(to_path, contents);
        }
    }
    print("[after assets] %.2fms\n", os_time_in_miliseconds());


    // @Incomplete: parse site pages and posts
    Page *pages = NULL;
    Page *last_page = NULL;

    Page *posts = NULL;
    Page *last_post = NULL;

    //~nja: site pages
    auto pages_dir = path_join(data_dir, S("pages"));
    {
        auto iter = os_file_list_begin(temp_arena(), pages_dir);
        File_Info it = {};
        while (os_file_list_next(&iter, &it))
        {
            if (file_is_directory(it)) continue;
            if (!string_ends_with(it.name, S(".md"))) continue;

            auto page_file = path_join(pages_dir, it.name);
            auto content   = os_read_entire_file(page_file);
            auto yaml      = find_yaml_frontmatter(content);

            string_advance(&content, yaml.count);

            Page *page    = PushStruct(temp_arena(), Page);
            page->slug    = PushStringCopy(temp_arena(), path_strip_extension(path_filename(it.name)));
            page->content = content;
            page->meta    = parse_page_meta(yaml);

            QueuePush(pages, last_page, page);
        }

        os_file_list_end(&iter);
    }

    //~nja: site posts
    auto posts_dir = path_join(data_dir, S("posts"));
    {
        auto iter = os_file_list_begin(temp_arena(), posts_dir);
        File_Info it = {};
        while (os_file_list_next(&iter, &it))
        {
            if (file_is_directory(it)) continue;
            if (!string_ends_with(it.name, S(".md"))) continue;

            auto post_file = path_join(posts_dir, it.name);
            auto content   = os_read_entire_file(post_file);
            auto yaml      = find_yaml_frontmatter(content);

            string_advance(&content, yaml.count);

            Page *post    = PushStruct(temp_arena(), Page);
            post->slug    = path_join(temp_arena(), S("posts"), it.name);
            post->content = content;
            post->meta    = parse_page_meta(yaml);

            QueuePush(pages, last_page, post);
            QueuePush(posts, last_post, post);
        }

        os_file_list_end(&iter);
    }


    //~nja: generate RSS feed
    auto rss_feed = generate_blog_rss_feed(site, posts);
    os_write_entire_file(path_join(output_dir, S("feed.xml")), rss_feed);

    //~nja: output site pages
    os_make_directory(path_join(output_dir, S("posts")));

    print("[time] %.2fms\n", os_time_in_miliseconds());
    print("Generating Pages...\n");

    for (Page *it = pages; it != NULL; it = it->next)
    {
        print("  %S\n", it->slug);

        auto page = it->meta;
        auto meta = it->meta;

        if (!meta.title.count)       meta.title = site.name;
        if (!meta.description.count) meta.description = site.description;
        if (!meta.image.count)       meta.image = site.image;

        Arena *arena = arena_alloc_from_memory(megabytes(16));

        //~nja: html template

        write(arena, "<!DOCTYPE html>\n");
        write(arena, "<html lang='en'>\n");

        write(arena, "<head>\n");
        write(arena, "<meta charset='utf-8' />\n");
        write(arena, "<meta name='viewport' content='width=device-width, initial-scale=1' />\n");

        write(arena, "<title>%S</title>\n", meta.title);
        write(arena, "<meta name='description' content='%S' />\n", meta.description);

        write(arena, "<meta itemprop='name' content='%S'>\n", meta.title);
        write(arena, "<meta itemprop='description' content='%S'>\n", meta.description);
        write(arena, "<meta itemprop='image' content='%S'>\n", meta.image);

        write(arena, "<meta property='og:title' content='%S' />\n", meta.title);
        write(arena, "<meta property='og:description' content='%S' />\n", meta.description);
        write(arena, "<meta property='og:type' content='%S' />\n", meta.og_type);
        write(arena, "<meta property='og:url' content='%S' />\n", meta.url);
        write(arena, "<meta property='og:site_name' content='%S' />\n", site.name);
        write(arena, "<meta property='og:locale' content='en_us' />\n");

        write(arena, "<meta name='twitter:card' content='summary' />\n");
        write(arena, "<meta name='twitter:title' content='%S' />\n", meta.title);
        write(arena, "<meta name='twitter:description' content='%S' />\n", meta.description);
        write(arena, "<meta name='twitter:image' content='%S' />\n", meta.image);
        write(arena, "<meta name='twitter:site' content='%S' />\n", site.twitter_handle);

        //write(arena, "<link rel='icon' type='image/png' href='%S' sizes='%dx%d' />\n", asset_path, size, size);

        if (site.theme_color.count)
        {
        write(arena, "<meta name='theme-color' content='%S' />\n", site.theme_color);
        write(arena, "<meta name='msapplication-TileColor' content='%S' />\n", site.theme_color);
        }

        write(arena, "<link rel='shortcut icon' href='/favicon.png' sizes='32x32' />\n");

        if (css.count)
        {
        write(arena, "<style type='text/css'>%S</style>\n", css);
        }

        if (rss_feed.count)
        {
        write(arena, "<link rel='alternate' type='application/rss+xml' title='Nick Aversano' href='%S/feed.xml' />\n", site.url);
        }

        write(arena, "</head>\n");
        //~nja: body
        write(arena, "<body>\n");

        //~nja: header
        write(arena, "<div class='content flex-x pad-64  xs:flex-y sm:csy-8 sm:pad-32'>\n");
            write(arena, "<div class='csx-16 flex-1 flex-x center-y'>\n");
                write(arena, "<span class='font-24 font-bold'><a href='%S'>%S</a></span>\n", S("/"), site.name);
            write(arena, "</div>\n");

            write(arena, "<div class='csx-16 flex-x'>\n");

            #if 0
            for (Each_Node(it, site.social_icons))
            {
                auto name  = node_get_child(it, 0)->string;
                auto image = node_get_child(it, 1)->string;
                auto url   = node_get_child(it, 2)->string;

                auto content = os_read_entire_file(path_join(data_dir, image));

                write(arena, "<a title='%S' href='%S' target='_blank' class='inline-flex center pad-8'><div class='inline-block size-20'>%S</div></a>\n", name, url, content);
            }
            #endif
            write(arena, "</div>\n");
        write(arena, "</div>\n");

        //~nja: image header / banner
        if (page.image.count)
        {
        write(arena, "<div class='w-full h-320 sm:h-240 xl:h-480 bg-light'>\n");
            write_image(arena, page.image, S(""), S("class='cover'"));
        write(arena, "</div>\n");

        #if 0
        auto links = find_next_and_prev_pages(it->slug, posts);
        write(arena, "<div class='content padx-64 sm:padx-32 h-64 flex-x center-y csx-32' style='margin-bottom: -2rem'>");
            if (links.prev)
            {
                write(arena, "<a class='font-bold pady-16' href='%S'>← Prev</a>", post_link(links.prev));
            }
            if (links.next)
            {
                write(arena, "<a class='font-bold pady-16 align-right' href='%S'>Next →</a>", post_link(links.next));
            }
        write(arena, "</div>\n");
        #endif

        }

        //~nja: page content
        write(arena, "<div class='content pad-64 sm:pad-32'>\n");

            //~nja: page header
            if (page.title.count || page.date.count)
            {
            write(arena, "<div class='marb-32'>\n", page.title);
                if (string_contains(it->slug, S("posts/")))
                {
                    i64 words = string_count_words(it->content);
                    i64 avg_read_time_mins = (i64)((words / 300.0f) + 0.5f);
                    if (avg_read_time_mins > 0)
                    {
                        write(arena, "<div class='c-gray' style='font-size:0.8rem'>%d min read</div>\n", avg_read_time_mins);
                    }
                    else
                    {
                        write(arena, "<div class='c-gray' style='font-size:0.8rem'>&lt;1 min read</div>\n", avg_read_time_mins);
                    }
                }
                if (page.title.count)
                {
                    write(arena, "<h1>%S</h1>\n", page.title);
                }
                if (page.date.count)
                {
                    write(arena, "<div class='c-gray'>%S</div>\n", pretty_date(ParsePostDate(page.date)));
                }
                if (page.author.count)
                {
                    // @Incomplete: support other author links
                    write(arena, "<div>By <a class='font-bold link' href='/'>%S</a></div>\n", page.author);
                }
            write(arena, "</div>\n", page.title);
            }

            write(arena, "%S", it->content);

        write(arena, "</div>\n");

        //~nja: footer
        write(arena, "<div class='content pad-64 w-800 sm:pad-32 flex-x center-x'>\n");
        write(arena, "<a class='pad-16' onclick='toggle()'>💡</a>\n");
        write(arena, "</div>\n");

        write(arena, "<script>%S</script>\n", js);

        write(arena, "</body>\n");

        write(arena, "</html>\n");

        // TODO(nick): we can go wide here and write all the files in parallel
        auto html = arena_to_string(arena);
        os_write_entire_file(path_join(output_dir, sprint("%S.html", it->slug)), html);
    }


    auto out = markdown_to_html(S("hello  *bold* _italic_ ~strike~ `code`  _*bold italic*_"));
    print("%S\n", out);


    // @Speed: go wide on writing all output files


    print("Done! Took %.2fms\n", os_time_in_miliseconds());

    os_shell_execute(S("firefox.exe"), S("http://localhost:3000"));
    
    auto public_path = string_alloc(os_allocator(), output_dir);
    run_server(S("127.0.0.1:3000"), public_path);

    return 0;
}