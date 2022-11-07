#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

#include "na.h"
#define NA_NET_IMPLEMENTATION
#include "na_net.h"

#include "meta.h"
#include "helpers.h"
#include "code_parser.h"

struct Build_Config
{
    String asset_dir;
    String output_dir;
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

    Node *pages;
    Node *projects;
    Node *links;
    Node *social_icons;
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

struct Nav_Links
{
    Node *next;
    Node *prev;
};

Site_Meta parse_site_info(Node *root)
{
    Site_Meta result = {};

    result.pages = nil_node();
    result.links = nil_node();
    result.projects = nil_node();
    result.social_icons = nil_node();

    for (Each_Node(it, root->first_child))
    {
        auto key   = it->string;
        auto value = node_get_child(it, 0)->string;

        if (false) {}
        else if (string_equals(key, S("title")))          { result.name = value; }
        else if (string_equals(key, S("description")))    { result.description = value; }
        else if (string_equals(key, S("site_url")))       { result.url = value; }
        else if (string_equals(key, S("site_image")))     { result.image = value; }
        else if (string_equals(key, S("site_icon")))      { result.icon = value; }
        else if (string_equals(key, S("author")))         { result.author = value; }
        else if (string_equals(key, S("twitter_handle"))) { result.twitter_handle = value; } 
        else if (string_equals(key, S("theme_color")))    { result.theme_color = value; } 

        else if (string_equals(key, S("pages")))          { result.pages = it->first_child; } 
        else if (string_equals(key, S("links")))          { result.links = it->first_child; } 
        else if (string_equals(key, S("projects")))       { result.projects = it->first_child; } 
        else if (string_equals(key, S("social_icons")))   { result.social_icons = it->first_child; } 
    }

    return result;
}

Page_Meta parse_page_meta(Node *root)
{
    Page_Meta result = {};
    for (Each_Node(it, root->first_child))
    {
        auto key   = it->string;
        auto value = it->first_child->string;

        if (false) {}
        else if (string_equals(key, S("title")))          { result.title = value; }
        else if (string_equals(key, S("image")))          { result.image = value; }
        else if (string_equals(key, S("og_type")))        { result.og_type = value; }
        else if (string_equals(key, S("desc")) || string_equals(key, S("description"))) { result.description = value; }
        else if (string_equals(key, S("date")))        { result.date = value; }
        else if (string_equals(key, S("author")))         { result.author = value; }
        else if (string_equals(key, S("draft")))          { result.draft = node_to_bool(it->first_child); }
    }
    return result;
}

String absolute_url(String src)
{
    return string_concat(S("/"), src);
}

String res_url(String src)
{
    return absolute_url(path_join(S("r"), src));
}

String post_link(Node *post)
{
    String result = {};
    if (!node_is_nil(post))
    {
        auto post_slug = node_get_child(post, 1)->string;
        return absolute_url(post_slug);
    }
    return result;
}

String generate_blog_rss_feed(Site_Meta site, Node *posts)
{
    auto arena = arena_make_from_memory(megabytes(1));
    auto pub_date = to_rss_date_string(os_get_current_time_in_utc());

    write(&arena, "<?xml version='1.0' encoding='UTF-8'?>\n");

    write(&arena, "<rss version='2.0' xmlns:atom='http://www.w3.org/2005/Atom'>\n");
    write(&arena, "<channel>\n");
    write(&arena, "\n");

    write(&arena, "<title>%S</title>\n", site.name);
    write(&arena, "<link>%S</link>\n", site.url);
    write(&arena, "<description>%S</description>\n", site.description);
    write(&arena, "<pubDate>%S</pubDate>\n", pub_date);
    write(&arena, "<lastBuildDate>%S</lastBuildDate>\n", pub_date);
    write(&arena, "<language>en-us</language>\n");
    write(&arena, "<image><url>%S</url></image>\n", site.image);
    write(&arena, "\n");

    for (Each_Node(it, posts->first_child))
    {
        auto post_title = node_get_child(it, 0)->string;
        auto post_slug  = node_get_child(it, 1)->string;
        auto post_root  = node_get_child(it, 2);

        auto post = parse_page_meta(post_root);
        auto date = ParsePostDate(post.date);
        auto link = path_join(site.url, post_link(it));

        if (!post.og_type.count) post.og_type = S("article");

        write(&arena, "<item>\n");
        write(&arena, "<title>%S</title>\n", post.title);
        write(&arena, "<description>%S</description>\n", post.description);
        write(&arena, "<pubDate>%S</pubDate>\n", to_rss_date_string(date));
        write(&arena, "<link>%S</link>\n", link);
        write(&arena, "<guid isPermaLink='true'>%S</guid>\n", link);
        write(&arena, "<category>%S</category>\n", post.og_type);
        write(&arena, "</item>\n");
        write(&arena, "\n");
    }

    write(&arena, "</channel>\n");
    write(&arena, "</rss>\n");

    return arena_to_string(&arena);
}

Nav_Links find_next_and_prev_pages(String page_slug, Node *posts)
{
    Nav_Links result = {};
    i64 id = cast(i64)ParsePostID(path_filename(page_slug));

    i64 next_id = I64_MAX;
    i64 prev_id = 0;
    for (Each_Node(post, posts->first_child))
    {
        auto post_slug = node_get_child(post, 1)->string;
        u64 post_id = ParsePostID(post_slug);

        if (post_id > id && post_id < next_id)
        {
            next_id = post_id;
            result.next = post;
        }

        if (post_id < id && post_id > prev_id)
        {
            prev_id = post_id;
            result.prev = post;
        }
    }

    return result;
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

String string_normalize_newlines(String input)
{
    String result = {};
    u8 *data = push_array(temp_arena(), u8, input.count);
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

// @Robustness: this is a little bit off because we don't exclude: html tags, @tags
i64 page_count_estimated_words(Node *page_root)
{
    i64 count = 0;
    for (Each_Node(it, page_root->first_child))
    {
        // NOTE(nick): only emit "loose" nodes
        if (node_has_children(it)) continue;
        count += string_count_words(it->string);
    }
    return count;
}

String apply_basic_markdown_styles(String text)
{
    // NOTE(nick): header tags
    if (string_starts_with(text, S("#")))
    {
        i64 count = 0;
        while (string_starts_with(text, S("#")))
        {
            string_advance(&text, 1);
            count += 1;
        }

        if (count <= 6)
        {
            return sprint("<h%d>%S</h%d>", count, text, count);
        }
    }

    bool is_html_tag = string_starts_with(text, S("<")) && string_ends_with(text, S(">"));
    if (is_html_tag)
    {
        return text;
    }

    // @Cleanup: figure out a better memory story here
    i64 count = 16 * text.count;
    u8 *data = push_array(temp_arena(), u8, count);
    u8 *at = data;
    u8 *end = data + count;

    for (i64 i = 0; i < text.count; i ++)
    {
        char it = text.data[i];
        char prev_it = i > 0 ? text.data[i - 1] : 0;

        // bold, italic, and code blocks
        if (prev_it != '\\')
        {
            if (it == '`')
            {
                bool is_triple = i < text.count - 3 && text.data[i + 1] == '`' && text.data[i + 2] == '`';
                if (is_triple)
                {
                    i64 closing_index = string_find(text, S("```"), i + 3);
                    if (closing_index < text.count)
                    {
                        at += print_to_memory(at, end - at, "<pre class='code'>%S</pre>", string_slice(text, i + 3, closing_index));
                        i = closing_index + 3;
                        continue;
                    }
                }
                else
                {
                    i64 closing_index = string_find(text, S("`"), i + 1);
                    if (closing_index < text.count)
                    {
                        at += print_to_memory(at, end - at, "<code class='inline_code'>%S</code>", string_slice(text, i + 1, closing_index));
                        i = closing_index;
                        continue;
                    }
                }
            }

            if (it == '_')
            {
                i64 closing_index = string_find(text, S("_"), i + 1);
                if (closing_index < text.count)
                {
                    at += print_to_memory(at, end - at, "<i>%S</i>", string_slice(text, i + 1, closing_index));
                    i = closing_index;
                    continue;
                }
            }

            if (it == '*')
            {
                i64 closing_index = string_find(text, S("*"), i + 1);
                if (closing_index < text.count)
                {
                    at += print_to_memory(at, end - at, "<b>%S</b>", string_slice(text, i + 1, closing_index));
                    i = closing_index;
                    continue;
                }
            }

            if (it == '-' && text.data[i + 1] == '-')
            {
                // @Incomplete: idk we probably need bounds checks here??
                if (text.data[i + 2] == '-')
                {
                    at += print_to_memory(at, end-at, "<hr/>");
                    i += 2;
                }
                else
                {
                    at += print_to_memory(at, end-at, "—");
                    i += 1;
                }

                continue;
            }
        }

        #if 0
        if (it == 'h')
        {
            auto str = string_skip(text, i);
            if (string_starts_with(str, S("http://")) || string_starts_with(str, S("https://")))
            {
                print("LINK!!!!!!\n");
            }
        }
        #endif

        *at = it;
        at ++;
    }

    arena_pop(temp_arena(), end - at);
    return make_string(data, at - data);
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

    //~nja: parse site meta
    auto site_root = parse_entire_file(temp_arena(), path_join(data_dir, S("site.meta")));

    auto site = parse_site_info(site_root);
    if (!site.image.count) site.image = site.icon;

    // TODO(nick): we can go wide here and read all the files in parallel

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

    //~nja: site pages
    auto output_pages = empty_node();
    for (Each_Node(node, site.pages))
    {
        auto page_title = node_get_child(node, 0);
        auto page_slug  = node_get_child(node, 1);

        auto meta_file = path_join(data_dir, sprint("%S.meta", page_slug->string));
        auto page_root = parse_entire_file(temp_arena(), meta_file);

        auto meta = parse_page_meta(page_root);
        if (!meta.draft)
        {
            auto array = make_array_node(page_title, page_slug, page_root);
            node_push_child(output_pages, array);
        }
    }

    //~nja: parse posts
    auto posts = empty_node();
    {
        auto posts_dir = path_join(data_dir, S("posts"));
        auto post_files = os_scan_directory(posts_dir);
        File_Info *it;
        Array_Foreach_Pointer_Reverse (it, post_files)
        {
            if (file_is_directory(it)) continue;

            auto fp = path_join(posts_dir, it->name);
            auto post_root = parse_entire_file(temp_arena(), fp);

            if (!node_is_nil(post_root))
            {
                auto meta = parse_page_meta(post_root);

                if (!meta.draft)
                {
                    auto title = meta.title;
                    auto slug = path_strip_extension(path_join(S("posts"), it->name));
                    post_root->string = slug;

                    auto array = make_array_node(node_from_string(title), node_from_string(slug), post_root);
                    node_push_child(output_pages, array);
                    node_push_child(posts, array);
                }
            }
        }
    }

    //~nja: generate RSS feed
    auto rss_feed = generate_blog_rss_feed(site, posts);
    os_write_entire_file(path_join(output_dir, S("feed.xml")), rss_feed);

    //~nja: output site pages
    os_make_directory(path_join(output_dir, S("posts")));

    print("[time] %.2fms\n", os_time_in_miliseconds());
    print("Generating Pages...\n");

    for (Each_Node(node, output_pages->first_child))
    {
        auto page_title = node_get_child(node, 0)->string;
        auto page_slug  = node_get_child(node, 1)->string;
        auto page_root  = node_get_child(node, 2);

        if (node_is_nil(page_root)) {
            print("Warning, page '%S' is empty: %S\n", page_slug, page_title);
            continue;
        }

        print("  %S\n", page_slug);

        auto page = parse_page_meta(page_root);
        auto meta = page;

        if (!meta.title.count)       meta.title = site.name;
        if (!meta.description.count) meta.description = site.description;
        if (!meta.image.count)       meta.image = site.image;

        Arena __arena = arena_make_from_memory(megabytes(1));
        Arena *arena = &__arena;

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

            for (Each_Node(it, site.social_icons))
            {
                auto name  = node_get_child(it, 0)->string;
                auto image = node_get_child(it, 1)->string;
                auto url   = node_get_child(it, 2)->string;

                auto content = os_read_entire_file(path_join(data_dir, image));

                write(arena, "<a title='%S' href='%S' target='_blank' class='inline-flex center pad-8'><div class='inline-block size-20'>%S</div></a>\n", name, url, content);
            }
            write(arena, "</div>\n");
        write(arena, "</div>\n");

        //~nja: image header / banner
        if (page.image.count)
        {
        write(arena, "<div class='w-full h-320 sm:h-240 xl:h-480 bg-light'>\n");
            write_image(arena, page.image, S(""), S("class='cover'"));
        write(arena, "</div>\n");

        auto links = find_next_and_prev_pages(page_slug, posts);
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

        }

        //~nja: page content
        write(arena, "<div class='content pad-64 sm:pad-32'>\n");

            //~nja: page header
            if (page.title.count || page.date.count)
            {
            write(arena, "<div class='marb-32'>\n", page.title);
                if (string_includes(page_slug, S("posts/")))
                {
                    i64 words = page_count_estimated_words(page_root);
                    i64 avg_read_time_mins = (i64)((words / 300.0f) + 0.5f);
                    write(arena, "<div class='c-gray' style='font-size:0.8rem'>%d min read</div>\n", avg_read_time_mins);
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

            for (Each_Node(it, page_root->first_child))
            {
                // NOTE(nick): only emit "loose" nodes
                if (node_has_children(it)) continue;

                if (node_has_tag(it, S("code")))
                {
                    write_code_block(arena, it->string);
                    continue;
                }

                if (node_has_tag(it, S("quote")))
                {
                    write_quote(arena, it->string);
                    continue;
                }

                //~nja: custom tags
                auto text = string_normalize_newlines(it->string);
                auto lines = string_split(text, S("\n\n"));
                For (lines)
                {
                    auto line = string_trim_whitespace(it);
                    if (!line.count) continue;


                    bool skip_p_tag = string_starts_with(line, S("<")) && string_ends_with(line, S(">"));
                    if (!skip_p_tag) write(arena, "<p>");

                    i64 index = 0;
                    while (index < line.count)
                    {
                        i64 search = string_find(line, S("@"), index);
                        i64 next_index = search;

                        // write out the string as normal
                        auto slice = string_slice(line, index, next_index);
                        slice = apply_basic_markdown_styles(slice);
                        write(arena, "%S", slice);

                        if (search < line.count)
                        {
                            // @Incomplete: we should really do a "parse one node" kind of a thing here
                            i64 newline_index = string_find(line, S("\n"), search + 1);
                            String single_line = string_slice(line, 0, newline_index);

                            i64 bracket_index = string_find(single_line, S("}"), search + 1);
                            bracket_index = Min(bracket_index, string_find(single_line, S(")"), search + 1));
                            bracket_index = Min(bracket_index, string_find(single_line, S("]"), search + 1));

                            i64 end_index = bracket_index;
                            if (bracket_index >= single_line.count)
                            {
                                i64 double_newline_index = string_find(line, S("\n\n"), search + 1);
                                end_index = double_newline_index;
                            }

                            //~nja: output tags
                            String tag_str = string_trim_whitespace(string_slice(line, search, end_index + 1));
                            auto root = parse_entire_string(temp_arena(), tag_str);

                            if (!node_is_nil(root) && !node_is_nil(root->first_child))
                            {
                                auto fc = root->first_child;
                                auto tag_name = fc->first_tag->string;
                                if (!tag_name.count) {
                                    tag_name = fc->string;
                                }
                                auto args = fc->first_child;

                                if (false) {}
                                else if (string_match(tag_name, S("link"), MatchFlags_IgnoreCase))
                                {
                                    auto text = node_get_child(args, 0)->string;
                                    auto href = node_get_child(args, 1)->string;

                                    if (!href.count) href = text;

                                    write_link(arena, text, href);
                                }
                                else if (string_match(tag_name, S("img"), MatchFlags_IgnoreCase))
                                {
                                    auto src = node_get_child(args, 0)->string;
                                    auto alt = node_get_child(args, 1)->string;
                                    if (!alt.count) alt = S("");

                                    write_image(arena, src, alt);
                                }
                                else if (string_match(tag_name, S("code"), MatchFlags_IgnoreCase))
                                {
                                    auto str = node_get_child(args, 0)->string;
                                    if (str.count)
                                    {
                                        write(arena, "<code class='inline_code'>%S</code>", str);
                                    }
                                    else
                                    {
                                        write_code_block(arena, fc->string);
                                    }
                                }
                                else if (string_match(tag_name, S("quote"), MatchFlags_IgnoreCase))
                                {
                                    auto str = node_get_child(args, 0)->string;
                                    if (!str.count)
                                    {
                                        str = fc->string;
                                    }
                                    write_quote(arena, str);
                                }
                                else if (string_match(tag_name, S("iframe"), MatchFlags_IgnoreCase))
                                {
                                    auto src = node_get_child(args, 0)->string;
                                    write(arena, "<div class='video'><iframe src='%S' allowfullscreen='' frameborder='0'></iframe></div>", src);
                                }
                                else if (string_match(tag_name, S("hr"), MatchFlags_IgnoreCase))
                                {
                                    auto str = node_get_child(args, 0)->string;
                                    write(arena, "<hr/>", str);
                                }
                                else if (string_match(tag_name, S("posts"), MatchFlags_IgnoreCase))
                                {
                                    //~nja: blog list
                                    write(arena, "<div class='flex-y csy-16'>\n");
                                    for (Each_Node(it, posts->first_child))
                                    {
                                        auto post_title = node_get_child(it, 0)->string;
                                        auto post_slug  = node_get_child(it, 1)->string;
                                        auto post_root  = node_get_child(it, 2);

                                        auto post = parse_page_meta(post_root);
                                        auto date = pretty_date(ParsePostDate(post.date));
                                        auto link = post_link(it);

                                        //~nja: article
                                        write(arena, "<a class='no-hover' href='%S'>\n", link);
                                        write(arena, "<div class='flex-1 flex-y center-y h-128' style='position:relative'>\n");
                                        if (post.image.count)
                                        {
                                        write_image(arena, post.image, S(""), S("class='bg bg-light cover'"));
                                        }
                                        write(arena, "<div class='flex-y padx-32 pady-16'><div class='font-bold'>%S</div><div class='c-gray' style='font-size: 0.8rem;'>%S</div></div>\n", post.title, post.description);
                                        write(arena, "</div>\n");
                                        write(arena, "</a>\n");
                                    }
                                    write(arena, "</div>\n");
                                }
                                else if (string_match(tag_name, S("post_links"), MatchFlags_IgnoreCase))
                                {
                                    //~nja: blog list
                                    write(arena, "<div class='flex-y csy-16'>\n");
                                    for (Each_Node(it, posts->first_child))
                                    {
                                        auto post_title = node_get_child(it, 0)->string;
                                        auto post_slug  = node_get_child(it, 1)->string;
                                        auto post_root  = node_get_child(it, 2);

                                        auto post = parse_page_meta(post_root);
                                        auto date = pretty_date(ParsePostDate(post.date));
                                        auto link = post_link(it);

                                        //~nja: article
                                        write(arena, "<a href='%S'>\n", link);
                                        write(arena, "<div class='flex-1 flex-y center-y' style='position:relative'>\n");
                                        write(arena, "<div class='flex-y'><div class='font-bold'>%S</div><div class='c-gray' style='font-size: 0.8rem;'>%S</div></div>\n", post.title, post.description);
                                        write(arena, "</div>\n");
                                        write(arena, "</a>\n");
                                    }
                                    write(arena, "</div>\n");
                                }
                                else if (string_match(tag_name, S("projects"), MatchFlags_IgnoreCase))
                                {
                                    write(arena, "<div class='grid marb-32'>");

                                    for (Each_Node(project, site.projects))
                                    {
                                        auto title = node_get_child(project, 0)->string;
                                        auto desc  = node_get_child(project, 1)->string;
                                        auto link  = node_get_child(project, 2)->string;

                                        write(arena, "<a class='flex-y center-y padx-32 pady-16 bg-light' href='%S' target='_blank'>", link);
                                        write(arena, "<div class='flex-y'>");
                                        write(arena, "<div class='font-bold'>%S</div>", title);
                                        write(arena, "<div class='c-gray' style='font-size: 0.8rem;'>%S</div>", desc);
                                        write(arena, "</div>");
                                        write(arena, "</a>");
                                    }

                                    write(arena, "</div>\n");
                                }
                                else if (string_match(tag_name, S("links"), MatchFlags_IgnoreCase))
                                {
                                    for (Each_Node(link, site.links))
                                    {
                                        auto text = node_get_child(link, 0)->string;
                                        auto href = node_get_child(link, 1)->string;
                                        write_link(arena, text, href);
                                    }
                                }
                            }

                            next_index = end_index;
                        }

                        index = next_index;
                        index += 1;
                    }

                    if (!skip_p_tag) write(arena, "</p>");
                }
            }

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
        os_write_entire_file(path_join(output_dir, sprint("%S.html", page_slug)), html);
    }

    print("Done! Took %.2fms\n", os_time_in_miliseconds());

    //os_exit(0);

    os_shell_execute(S("firefox.exe"), S("http://localhost:3000"));
    
    auto public_path = string_alloc(os_allocator(), output_dir);
    run_server(S("127.0.0.1:3000"), public_path);
}