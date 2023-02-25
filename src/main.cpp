#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"

#include "na.h"
#define NA_NET_IMPLEMENTATION
#include "na_net.h"

#include "helpers.h"
#include "code_parser.h"

struct Link
{
    String title;
    String href;
    String desc;

    Link *next;
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
    String og_type;

    Link *social_icons;
    Link *featured;
    Link *authors;
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

struct Page
{
    Page_Meta meta;
    String slug;
    String content;
    String type;

    Page *next;
    Page *prev;
};

struct Build_Context
{
    String data_dir;
    String output_dir;

    Site_Meta site;

    Page *pages;
    Page *last_page;

    Page *posts;
    Page *last_post;

    Page *projects;
    Page *last_project;
};

struct Next_Prev_Pages
{
    Page *next;
    Page *prev;
};

#define Each_Node(it, list) auto *it = list; it != NULL; it = it->next

#define Each_Node_Reverse(it, list) auto *it = list; it != NULL; it = it->prev

#define Each_Page(it, list) Page *it = list; it != NULL; it = it->next

#define Each_Link(it, list) Link *it = list; it != NULL; it = it->next


Build_Context ctx = {};


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

Link *parse_yaml_links_array(String str)
{
    Link *result = NULL;
    Link *last = NULL;

    i64 depth = 0;

    for (i64 i = 0; i < str.count; i += 1)
    {
        char it = str.data[i];

        if (char_is_whitespace(it)) continue;
        if (it == '[') depth += 1;
        if (it == ']') {
            depth -= 1;
            if (depth <= 0) break;
        }

        // NOTE(nick): if the user made a mistake and forgot to add a closing bracket
        // the second colon would be a new key
        if (it == ':') break;

        if (depth == 2)
        {
            i64 closing_index = string_find(str, S("]"), i + 1);
            auto list = string_slice(str, i, closing_index + 1);

            auto str = string_trim_whitespace(string_slice(list, 1, list.count - 1));
            auto parts = string_split(str, S(","));
            if (parts.count > 0)
            {
                Link *item = PushStruct(temp_arena(), Link);
                if (parts.count > 0) item->title = PushStringCopy(temp_arena(), yaml_to_string(parts[0]));
                if (parts.count > 1) item->href  = PushStringCopy(temp_arena(), yaml_to_string(parts[1]));
                if (parts.count > 2) item->desc  = PushStringCopy(temp_arena(), yaml_to_string(parts[2]));
                QueuePush(result, last, item);
            }

            i += list.count - 1;
            depth -= 1;
        }
    }

    return result;
}


Site_Meta parse_site_info(String yaml)
{
    Site_Meta result = {};

    i64 offset = 0;
    auto lines = string_split(yaml, S("\n"));
    For_Index (lines)
    {
        auto it = lines[index];

        i64 colon_index = string_find(it, S(":"));
        if (colon_index >= it.count)
        {
            offset += it.count + 1;
            continue;
        }

        auto key   = string_trim_whitespace(string_slice(it, 0, colon_index));
        auto value = string_trim_whitespace(string_slice(it, colon_index + 1));

        auto str = yaml_to_string(value);

        if (false) {}
        else if (string_equals(key, S("title")))          { result.name = str; }
        else if (string_equals(key, S("desc")) ||
                    string_equals(key, S("description"))) { result.description = str; }
        else if (string_equals(key, S("site_url")))       { result.url = str; }
        else if (string_equals(key, S("site_image")))     { result.image = str; }
        else if (string_equals(key, S("site_icon")))      { result.icon = str; }
        else if (string_equals(key, S("author")))         { result.author = str; }
        else if (string_equals(key, S("twitter_handle"))) { result.twitter_handle = str; } 
        else if (string_equals(key, S("theme_color")))    { result.theme_color = str; } 
        else if (string_equals(key, S("og_type")))        { result.og_type = str; } 

        else if (string_equals(key, S("social_icons"))) {
            result.social_icons = parse_yaml_links_array(string_slice(yaml, offset + colon_index + 1));
        }
        else if (string_equals(key, S("author_links"))) {
            result.authors = parse_yaml_links_array(string_slice(yaml, offset + colon_index + 1));
        }
        else if (string_equals(key, S("featured_links"))) {
            result.featured = parse_yaml_links_array(string_slice(yaml, offset + colon_index + 1));
        }

        offset += it.count + 1;
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
        else if (string_equals(key, S("desc")) ||
                    string_equals(key, S("description"))) { result.description = str; }
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

Page *find_page_by_slug(Page *page, Page *array)
{
    for (Each_Node(it, array))
    {
        if (string_equals(it->slug, page->slug)) return it;
    }
    return NULL;
}

Next_Prev_Pages find_next_and_prev_pages(Page *page)
{
    Next_Prev_Pages result = {};

    Page *next = page->next;
    Page *prev = page->prev;

    while (next && next->meta.draft) next = next->next;
    while (prev && prev->meta.draft) prev = prev->prev;

    result.next = next;
    result.prev = prev;
    return result;
}

Next_Prev_Pages find_next_and_prev_pages_by_type(Page *page)
{
    Next_Prev_Pages result = {};

    String type = page->type;

    Page *next = page->next;
    Page *prev = page->prev;

    while (next && (!string_equals(type, next->type) || next->meta.draft)) next = next->next;
    while (prev && (!string_equals(type, prev->type) || prev->meta.draft)) prev = prev->next;

    result.next = next;
    result.prev = prev;
    return result;
}

Link *find_link_by_title(String title, Link *links)
{
    for (Each_Link(it, links))
    {
        if (string_equals(it->title, title)) return it;
    }
    return NULL;
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
        if (it->meta.draft) continue;

        auto post_slug  = it->slug;

        auto post = it->meta;
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

// NOTE(nick): in the future maybe we could let the sign of the argument
// determine the order in which we output posts
// e.g. @posts(-5) could be the last 5 posts
void write_page_card_list(Arena *arena, Page *items, Page *last_item, i64 limit)
{
    i64 count = 0;

    //~nja: blog list
    write(arena, "<div class='flex-y csy-16'>\n");

    bool reverse = false;
    if (limit < 0)
    {
        limit = -limit;
        reverse = true;
    }

    // NOTE(nick): iterate forwards or backwards
    for (auto *it = (reverse ? last_item : items); it != NULL; (reverse ? it = it->prev : it = it->next))
    {
        if (it->meta.draft) continue;
        if (count++ >= limit) break;

        auto post_title = it->meta.title;
        auto post_slug  = it->slug;

        auto post = it->meta;
        auto date = pretty_date(ParsePostDate(post.date));
        auto link = post_link(it);

        //~nja: article
        write(arena, "<a class='no-hover' href='%S'>\n", link);
            write(arena, "<div class='flex-1 flex-y center-y h-128 round-2 crop' style='position:relative'>\n");
                if (post.image.count)
                {
                write_image(arena, post.image, S(""), S("class='bg bg-light cover'"));
                }
                write(
                    arena,
                    "<div class='flex-y padx-32 pady-16'><div class='font-bold'>%S</div><div class='c-gray' style='font-size: 0.8rem;'>%S</div></div>\n",
                    post.title,
                    post.description
                );
            write(arena, "</div>\n");
        write(arena, "</a>\n");
    }

    write(arena, "</div>\n");

    print("Done!\n");
}

void write_custom_tag(Arena *arena, String tag_name, Array<String> args)
{
    String arg0 = args.count > 0 ? yaml_to_string(args[0]) : String{};
    String arg1 = args.count > 1 ? yaml_to_string(args[1]) : String{};

    if (false) {}
    else if (string_match(tag_name, S("link"), MatchFlags_IgnoreCase))
    {
        auto text = arg0;
        auto href = arg1;

        if (!href.count) href = text;

        write_link(arena, text, href);
    }
    else if (string_match(tag_name, S("img"), MatchFlags_IgnoreCase))
    {
        auto src = arg0;
        auto alt = arg1;
        if (!alt.count) alt = S("");

        write_image(arena, src, alt);
    }
    else if (string_match(tag_name, S("code"), MatchFlags_IgnoreCase))
    {
        auto str = arg0;
        write(arena, "<code class='inline_code'>%S</code>", str);
    }
    else if (string_match(tag_name, S("quote"), MatchFlags_IgnoreCase))
    {
        auto str = arg0;
        write_quote(arena, str);
    }
    else if (string_match(tag_name, S("iframe"), MatchFlags_IgnoreCase))
    {
        auto src = arg0;
        write(arena, "<div class='video'><iframe src='%S' allowfullscreen='' frameborder='0'></iframe></div>", src);
    }
    else if (string_match(tag_name, S("hr"), MatchFlags_IgnoreCase))
    {
        auto str = arg0;
        write(arena, "<hr/>", str);
    }
    else if (string_match(tag_name, S("posts"), MatchFlags_IgnoreCase))
    {
        i64 limit = I64_MAX;
        if (arg0.count > 0) limit = string_to_i64(arg0);

        write_page_card_list(arena, ctx.posts, ctx.last_post, limit);
    }
    else if (string_match(tag_name, S("projects"), MatchFlags_IgnoreCase))
    {
        i64 limit = I64_MAX;
        if (arg0.count > 0) limit = string_to_i64(arg0);

        write_page_card_list(arena, ctx.projects, ctx.last_project, limit);
    }
    else if (string_match(tag_name, S("post_list"), MatchFlags_IgnoreCase))
    {
        i64 limit = I64_MAX;
        if (arg0.count > 0) limit = string_to_i64(arg0);
        i64 count = 0;

        //~nja: blog list
        write(arena, "<div class='flex-y csy-16'>\n");
        for (Each_Node_Reverse(it, ctx.last_post))
        {
            if (it->meta.draft) continue;
            if (count++ >= limit) break;

            auto title = it->meta.title;
            auto post_slug  = it->slug;

            auto post = it->meta;
            auto date = pretty_date(ParsePostDate(post.date));
            auto link = post_link(it);

            //~nja: article
            write(arena, "<a href='%S'>\n", link);
                write(arena, "<div class='flex-1 flex-y center-y' style='position:relative'>\n");
                    write(arena, "<div class='flex-y'>");
                        write(
                            arena,
                            "<div class='font-bold'>%S</div><div class='c-gray' style='font-size: 0.8rem;'>%S</div>",
                            title,
                            date
                        );
                    write(arena, "</div>");
                write(arena, "</div>\n");
            write(arena, "</a>\n");
        }
        write(arena, "</div>\n");
    }
    else if (string_match(tag_name, S("featured"), MatchFlags_IgnoreCase))
    {
        i64 limit = I64_MAX;
        if (arg0.count > 0) limit = string_to_i64(arg0);
        i64 count = 0;

        write(arena, "<div class='grid marb-32'>");

        for (Each_Node(it, ctx.site.featured))
        {
            if (count++ >= limit) break;

            auto title = it->title;
            auto desc  = it->desc;
            auto link  = it->href;

            write(arena, "<a class='flex-y center-y padx-32 pady-16 bg-light round' href='%S' target='_blank'>", link);
            write(arena, "<div class='flex-y'>");
            write(arena, "<div class='font-bold'>%S</div>", title);
            write(arena, "<div class='c-gray' style='font-size: 0.8rem;'>%S</div>", desc);
            write(arena, "</div>");
            write(arena, "</a>");
        }

        write(arena, "</div>\n");
    }
    else
    {
        print("[warning] Unhandled custom tag: @%S\n", tag_name);
    }
}

// @Incomplete: supported nested tags
// @Speed: arena_print is actually sort of slower than you might think (especially when doing for each character)
String markdown_to_html(String text)
{
    Arena *arena = arena_alloc_from_memory(gigabytes(1));

    text = string_normalize_newlines(text);

    bool was_line_break = true;
    i64 html_scope_depth = 0;

    for (i64 i = 0; i < text.count; i += 1)
    {
        char it = text.data[i];

        char prev_it = i > 0 ? text.data[i - 1] : '\0';
        bool start_of_line = prev_it == '\n';
        bool escaped = it == '\\';

        if (escaped)
        {
            i += 1;
            if (i < text.count)
            {
                arena_print(arena, "%c", text.data[i]);
            }
            continue;
        }

        if (start_of_line)
        {
            // line breaks (cheap <p> trick)
            if (it == '\n')
            {
                if (!was_line_break && html_scope_depth <= 0)
                {
                    arena_print(arena, "<p></p>");
                    was_line_break = true;
                }
                continue;
            }

            was_line_break = false;

            // hr
            if (it == '-' && i < text.count - 2 && text.data[i + 1] == '-' && text.data[i + 2] == '-')
            {
                i += 2;
                while (i < text.count && text.data[i] == '-') i += 1;

                arena_print(arena, "<hr/>");
                continue;
            }

            // code blocks / quote blocks
            char bc = it == '`' ? '`' : '>';
            if (it == bc && i < text.count - 2 && text.data[i + 1] == bc && text.data[i + 2] == bc)
            {
                bool is_code_block = bc == '`';
                i += 3;

                i64 tag_start = i;
                while (text.data[i] != '\n') i += 1;

                auto tag = string_slice(text, tag_start, i);

                i += 1;

                i64 code_start = i;
                i64 code_end = 0;

                while (true)
                {
                    if (text.data[i] == bc && text.data[i + 1] == bc && text.data[i + 2] == bc)
                    {
                        code_end = i;
                        i += 3;
                        break;
                    }

                    if (i >= text.count)
                    {
                        code_end = i;
                        break;
                    }

                    i += 1;
                }

                auto str = string_slice(text, code_start, code_end);
                if (is_code_block)
                {
                    if (tag.count) {
                        write_code_block(arena, str);
                    } else {
                        arena_print(arena, "<pre class='code'>%S</pre>", str);
                    }
                }
                else
                {
                    write_quote(arena, str);
                }

                continue;
            }

            // headers
            if (it == '#')
            {
                i64 count = 1;
                while (i < text.count && text.data[i + count] == '#')
                {
                    count += 1;
                }

                if (count >= 1 && count <= 6 && char_is_whitespace(text.data[i + count]))
                {
                    i += count;
                    i += 1; // eat whitespace

                    i64 start_index = i;
                    while (text.data[i] != '\n') i += 1;

                    auto header_text = string_slice(text, start_index, i);
                    arena_print(arena, "<h%d>%S</h%d>", count, header_text, count);

                    continue;
                }
            }

            // lists
            if (i < text.count - 1)
            {
                // bullet list
                if (it == '-' && char_is_whitespace(text.data[i + 1]))
                {
                    arena_print(arena, "<ul>");

                    while (i < text.count && text.data[i] == '-' && char_is_whitespace(text.data[i + 1]))
                    {
                        i64 start = i + 2;
                        while (i < text.count && text.data[i] != '\n') i += 1;

                        auto item_text = string_slice(text, start, i);
                        arena_print(arena, "<li>%S</li>", item_text);

                        i += 1;
                    }

                    arena_print(arena, "</ul>");
                    continue;
                }

                // number list
                if (char_is_digit(it) && text.data[i + 1] == '.' && char_is_whitespace(text.data[i + 2]))
                {
                    arena_print(arena, "<ol>");

                    while (i < text.count)
                    {
                        if (!(char_is_digit(it) && text.data[i + 1] == '.' && char_is_whitespace(text.data[i + 2])))
                        {
                            break;
                        }

                        i64 start = i + 3;
                        while (i < text.count && text.data[i] != '\n') i += 1;

                        auto item_text = string_slice(text, start, i);
                        arena_print(arena, "<li>%S</li>", item_text);

                        i += 1;
                    }

                    arena_print(arena, "</ol>");
                    continue;
                }

                // markdown-style quotes
                if (it == '>' && char_is_whitespace(text.data[i + 1]))
                {
                    Array<String> lines = {};

                    while (i < text.count && text.data[i] == '>' && char_is_whitespace(text.data[i + 1]))
                    {
                        i64 start = i + 2;
                        while (i < text.count && text.data[i] != '\n') i += 1;

                        auto item_text = string_slice(text, start, i);
                        array_push(&lines, item_text);

                        i += 1;
                    }

                    write_quote(arena, string_join(lines, S("\n")));
                    continue;
                }
            }
        }

        // html tags and comments
        if (it == '<' && i < text.count - 1)
        {
            char next = text.data[i + 1];
            // html tag
            if (char_is_alpha(next) || next == '/')
            {
                i64 start_index = i;
                i += 2; // <a
                while (i < text.count && text.data[i] != '>') i += 1;

                arena_print(arena, "%S", string_slice(text, start_index, i + 1));

                if (next == '/') {
                    html_scope_depth -= 1;
                } else {
                    html_scope_depth += 1;
                }

                continue;
            }

            // html comment
            if (next == '!' && i < text.count - 3 && text.data[i + 2] == '-' && text.data[i + 3] == '-')
            {
                i64 start_index = i;
                i += 4; // <!--

                while (i < text.count)
                {
                    // -->
                    if (
                        i < text.count - 3 &&
                        (text.data[i] == '-' && text.data[i + 1] == '-' && text.data[i + 2] == '>'))
                    {
                        i += 2;
                        break;
                    }

                    i += 1;
                }

                arena_print(arena, "%S", string_slice(text, start_index, i + 3));
                continue;
            }
        }

        // em dash
        if (it == '-')
        {
            // NOTE(nick): markdown actually uses 2 and 3 dashes to be en and em dashes respectively
            // but we don't really care about that for now...
            if (i < text.count - 1 && text.data[i + 1] == '-')
            {
                i += 1;
                arena_print(arena, "—");
                continue;
            }
        }

        // links
        if (it == '[')
        {
            i += 1;
            i64 text_start = i;
            while (i < text.count && text.data[i] != ']' && text.data[i] != '\n') i += 1;

            i += 1;

            if (text.data[i - 1] == ']' && text.data[i] == '(')
            {
                i64 text_end = i - 1;
                i += 1;
                auto link_text = string_slice(text, text_start, text_end);

                i64 link_start = i;
                while (i < text.count && text.data[i] != ')' && text.data[i] != '\n') i += 1;
                if (text.data[i] == ')')
                {
                    auto link_href = string_slice(text, link_start, i);

                    write_link(arena, link_text, link_href);
                    i += 1;
                    continue;
                }
                else
                {
                    i = text_start + 1;
                }
            }
        }


        // custom expressions
        if (it == '@')
        {
            i += 1;
            i64 tag_start = i;

            while (i < text.count && (char_is_alpha(text.data[i]) || text.data[i] == '_'))
            {
                i += 1;
            }

            i64 tag_end = i;

            char bracket = text.data[i];
            if (bracket == '(' || bracket == '[' || bracket == '{')
            {
                char closing_bracket = bracket == '(' ? ')' : (bracket == '[' ? ']' : '}');

                i += 1;
                while (i < text.count && text.data[i] != closing_bracket) i += 1;

                auto tag_name = string_slice(text, tag_start, tag_end);
                auto arg_str  = string_slice(text, tag_end + 1, i);

                auto args = string_split(arg_str, S(","));
                For_Index(args) {
                    args[index] = string_trim_whitespace(args[index]);
                }

                write_custom_tag(arena, tag_name, args);
                continue;
            }
            else
            {
                i = tag_start - 1;
            }
        }


        // @Incomplete @Robustness: what happens if these are not matched on a line??
        // markdown just _doesn't_ do anything for the given expression

        // bold
        if (it == '*')
        {
            i64 closing_index = string_find(text, S("*"), i + 1);
            if (closing_index < text.count)
            {
                arena_print(arena, "<b>%S</b>", string_slice(text, i + 1, closing_index));
                i = closing_index;
                continue;
            }
        }

        // italic
        if (it == '_')
        {
            i64 closing_index = string_find(text, S("_"), i + 1);
            if (closing_index < text.count)
            {
                arena_print(arena, "<i>%S</i>", string_slice(text, i + 1, closing_index));
                i = closing_index;
                continue;
            }
        }
        
        // strike
        if (it == '~')
        {
            i64 closing_index = string_find(text, S("~"), i + 1);
            if (closing_index < text.count)
            {
                arena_print(arena, "<s>%S</s>", string_slice(text, i + 1, closing_index));
                i = closing_index;
                continue;
            }
        }

        // inline code
        if (it == '`')
        {
            i64 closing_index = string_find(text, S("`"), i + 1);
            if (closing_index < text.count)
            {
                arena_print(arena, "<code class='inline_code'>%S</code>", string_slice(text, i + 1, closing_index));
                i = closing_index;
                continue;
            }
        }

        // output character
        u8 *output = PushStruct(arena, u8);
        *output = it;
    }

    String result = arena_to_string(arena);

    if (string_ends_with(result, S("<p></p>")))
    {
        result.count -= S("<p></p>").count;
    }

    return result;
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

    ctx.data_dir   = data_dir;
    ctx.output_dir = output_dir;

    os_make_directory(output_dir);

    auto yaml = os_read_entire_file(path_join(data_dir, S("site.yaml")));

    Site_Meta site = parse_site_info(yaml);
    ctx.site = site;

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


    //~nja: site pages
    {
        auto dir = path_join(data_dir, S("pages"));
        auto iter = os_file_list_begin(temp_arena(), dir);
        File_Info it = {};
        while (os_file_list_next(&iter, &it))
        {
            if (file_is_directory(it)) continue;
            if (!string_ends_with(it.name, S(".md"))) continue;

            auto page_file = path_join(dir, it.name);
            auto content   = os_read_entire_file(page_file);
            auto yaml      = find_yaml_frontmatter(content);

            string_advance(&content, yaml.count);

            Page *page    = PushStruct(temp_arena(), Page);
            page->slug    = PushStringCopy(temp_arena(), path_strip_extension(path_filename(it.name)));
            page->content = content;
            page->meta    = parse_page_meta(yaml);
            page->type    = S("page");

            DLLPushBack(ctx.pages, ctx.last_page, page);
        }

        os_file_list_end(&iter);
    }

    // @Cleanup: make this dynamic?!
    // It would be cool if we didn't have to specify these things every time!
    // maybe it just reads from the folder name or something?
    // :DynamicPageTypes

    //~nja: site posts
    {
        auto dir = path_join(data_dir, S("posts"));
        auto iter = os_file_list_begin(temp_arena(), dir);
        File_Info it = {};
        while (os_file_list_next(&iter, &it))
        {
            if (file_is_directory(it)) continue;
            if (!string_ends_with(it.name, S(".md"))) continue;

            auto post_file = path_join(dir, it.name);
            auto content   = os_read_entire_file(post_file);
            auto yaml      = find_yaml_frontmatter(content);

            string_advance(&content, yaml.count);

            Page *page    = PushStruct(temp_arena(), Page);
            page->slug    = path_join(temp_arena(), S("posts"), path_strip_extension(it.name));
            page->content = content;
            page->meta    = parse_page_meta(yaml);
            page->type    = S("post");

            DLLPushBack(ctx.pages, ctx.last_page, page);

            Page *copy = PushStruct(temp_arena(), Page);
            memory_copy(page, copy, sizeof(Page));
            DLLPushBack(ctx.posts, ctx.last_post, copy);
        }

        os_file_list_end(&iter);
    }

    // @Copypaste:
    // :DynamicPageTypes

    //~nja: site projects
    {
        auto dir = path_join(data_dir, S("projects"));
        auto iter = os_file_list_begin(temp_arena(), dir);
        File_Info it = {};
        while (os_file_list_next(&iter, &it))
        {
            if (file_is_directory(it)) continue;
            if (!string_ends_with(it.name, S(".md"))) continue;

            auto post_file = path_join(dir, it.name);
            auto content   = os_read_entire_file(post_file);
            auto yaml      = find_yaml_frontmatter(content);

            string_advance(&content, yaml.count);

            Page *page    = PushStruct(temp_arena(), Page);
            page->slug    = path_join(temp_arena(), S("projects"), path_strip_extension(it.name));
            page->content = content;
            page->meta    = parse_page_meta(yaml);
            page->type    = S("project");

            DLLPushBack(ctx.pages, ctx.last_page, page);

            Page *copy = PushStruct(temp_arena(), Page);
            memory_copy(page, copy, sizeof(Page));
            DLLPushBack(ctx.projects, ctx.last_project, copy);
        }

        os_file_list_end(&iter);
    }


    //~nja: generate RSS feed
    auto rss_feed = generate_blog_rss_feed(site, ctx.posts);
    os_write_entire_file(path_join(output_dir, S("feed.xml")), rss_feed);

    //~nja: output site pages
    os_make_directory(path_join(output_dir, S("posts")));
    os_make_directory(path_join(output_dir, S("projects")));

    print("[time] %.2fms\n", os_time_in_miliseconds());
    print("Generating Pages...\n");

    for (Each_Page(it, ctx.pages))
    {
        print("  %S\n", it->slug);

        auto page = it->meta;
        auto meta = it->meta;

        if (!meta.title.count)       meta.title = site.name;
        if (!meta.description.count) meta.description = site.description;
        if (!meta.image.count)       meta.image = site.image;
        if (!meta.og_type.count)     meta.og_type = site.og_type;

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

            for (Each_Link(it, ctx.site.social_icons))
            {
                auto name  = it->title;
                auto image = it->desc;
                auto url   = it->href;

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
        }

        // @Incomplete: we can actually make this work for other types of things too!
        //~nja: post next / prev links
        auto post = find_page_by_slug(it, ctx.posts);
        if (post)
        {
            auto links = find_next_and_prev_pages(post);
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
                    Link *author = find_link_by_title(page.author, ctx.site.authors);
                    if (author)
                    {
                        write(arena, "<div>By <a class='font-bold link' href='%S'>%S</a></div>\n",
                            author->href, author->title);
                    }
                    else
                    {
                        write(arena, "<div>By <span class='font-bold'>%S</span></div>\n", page.author);
                    }
                }
            write(arena, "</div>\n", page.title);
            }

            write(arena, "%S", markdown_to_html(it->content));

        write(arena, "</div>\n");

        //~nja: footer
        write(arena, "<div class='content pad-64 w-800 sm:pad-32 flex-x center-x'>\n");
        write(arena, "<a class='pad-16' onclick='toggle()'>💡</a>\n");
        write(arena, "</div>\n");

        write(arena, "<script>%S</script>\n", js);
        write(arena, "<script src='/lightning.js'></script>\n");

        write(arena, "</body>\n");

        write(arena, "</html>\n");

        auto html = arena_to_string(arena);
        assert(os_write_entire_file(path_join(output_dir, sprint("%S.html", it->slug)), html));
    }


    // TODO(nick): we can go wide here and write all the files in parallel
    // @Speed: go wide on writing all output files

    print("Done! Took %.2fms\n", os_time_in_miliseconds());

    if (argc >= 3)
    {
        // TODO(nick): allow the browser to be customized
        
        auto arg3 = string_from_cstr(argv[3]);

        if (string_equals(arg3, S("--serve")))
        {
            os_shell_execute(S("firefox.exe"), S("http://localhost:3000"));
            
            auto public_path = string_alloc(os_allocator(), output_dir);
            run_server(S("127.0.0.1:3000"), public_path);
        }

        if (string_equals(arg3, S("--open")))
        {
            os_shell_execute(S("firefox.exe"), path_join(S("file://"), output_dir, S("index.html")));
        }
    }

    return 0;
}