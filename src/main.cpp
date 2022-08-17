#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"
#include "na.h"

#include "meta.h"

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

    Node *social_icons;
    Node *pages;
};

struct Page_Meta
{
    String title;
    String description;
    String image;
    String og_type;
    String url;
    String date;
};

#include "helpers.h"

Site_Meta parse_site_info(Node *root)
{
    Site_Meta result = {};

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

        else if (string_equals(key, S("social_icons")))   { result.social_icons = it->first_child; } 
        else if (string_equals(key, S("pages")))          { result.pages = it->first_child; } 
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

    write(&arena, "</channel>\n");
    write(&arena, "</rss>\n");

    for (Each_Node(it, posts->first_child))
    {
        // @Cleanup: root root situation
        auto post = parse_page_meta(it->first_child);
        auto date = ParsePostDate(post.date);

        if (!post.og_type.count) post.og_type = S("article");

        auto link = S("http://nickav.co/post/1");

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

    return arena_to_string(&arena);
}

int main(int argc, char **argv)
{
    os_init();

    if (argc < 2) {
        char *arg0 = argv[0];
        print("Usage: %s <data> <bin>\n", arg0);
        return -1;
    }

    auto exe_dir = os_get_executable_directory();

    char *arg0 = argv[0];
    char *arg1 = argv[1];
    char *arg2 = argv[2];

    auto data_dir   = path_resolve(exe_dir, string_from_cstr(arg1));
    auto output_dir = path_resolve(exe_dir, string_from_cstr(arg2));

    auto css = os_read_entire_file(path_join(data_dir, S("style.css")));
    css = minify_css(css);

    auto site_root = parse_entire_file(temp_arena(), path_join(data_dir, S("site.meta")));

    auto site = parse_site_info(site_root);
    if (!site.image.count) site.image = site.icon;

    auto posts = nil_node();
    {
        auto posts_dir = path_join(data_dir, S("posts"));
        auto post_files = os_scan_directory(posts_dir);
        File_Info *it;
        Array_Foreach_Pointer_Reverse (it, post_files)
        {
            if (file_is_directory(it)) continue;

            auto fp = path_join(posts_dir, it->name);
            auto post = parse_entire_file(temp_arena(), fp);

            if (!node_is_nil(post))
            {
                node_push_key_value(posts, it->name, post);

                auto mname = path_strip_extension(path_join(S("posts"), it->name));
                auto array = make_array_node(node_from_string(it->name), node_from_string(mname));
                node_push_sibling(site.pages, array);
            }
        }
    }

    auto rss_feed = generate_blog_rss_feed(site, posts);
    os_write_entire_file(path_join(output_dir, S("feed.xml")), rss_feed);

    os_make_directory(path_join(output_dir, S("posts")));

    for (Each_Node(node, site.pages))
    {
        auto page_title = node_get_child(node, 0)->string;
        auto page_slug  = node_get_child(node, 1)->string;

        dump(page_slug);

        auto meta_file = path_join(data_dir, sprint("%S.meta", page_slug));
        auto page_root = parse_entire_file(temp_arena(), meta_file);

        if (node_is_nil(page_root)) {
            print("Warning, page '%S' is empty: %S\n", page_slug, meta_file);
            continue;
        }

        auto page = parse_page_meta(page_root);

        if (!page.title.count)       page.title = site.name;
        if (!page.description.count) page.description = site.description;
        if (!page.image.count)       page.image = site.image;

        Arena __arena = arena_make_from_memory(megabytes(1));
        Arena *arena = &__arena;

        write(arena, "<!DOCTYPE html>\n");
        write(arena, "<html lang='en'>\n");

        write(arena, "<head>\n");
        write(arena, "<meta charset='utf-8' />\n");
        write(arena, "<meta name='viewport' content='width=device-width, initial-scale=1' />\n");

        write(arena, "<title>%S</title>\n", page.title);
        write(arena, "<meta name='description' content='%S' />\n", page.description);

        write(arena, "<meta itemprop='name' content='%S'>\n", page.title);
        write(arena, "<meta itemprop='description' content='%S'>\n", page.description);
        write(arena, "<meta itemprop='image' content='%S'>\n", page.image);

        write(arena, "<meta property='og:title' content='%S' />\n", page.title);
        write(arena, "<meta property='og:description' content='%S' />\n", page.description);
        write(arena, "<meta property='og:type' content='%S' />\n", page.og_type);
        write(arena, "<meta property='og:url' content='%S' />\n", page.url);
        write(arena, "<meta property='og:site_name' content='%S' />\n", site.name);
        write(arena, "<meta property='og:locale' content='en_us' />\n");

        write(arena, "<meta name='twitter:card' content='summary' />\n");
        write(arena, "<meta name='twitter:title' content='%S' />\n", page.title);
        write(arena, "<meta name='twitter:description' content='%S' />\n", page.description);
        write(arena, "<meta name='twitter:image' content='%S' />\n", page.image);
        write(arena, "<meta name='twitter:site' content='%S' />\n", site.twitter_handle);

        //write(arena, "<link rel='icon' type='image/png' href='%S' sizes='%dx%d' />\n", asset_path, size, size);

        if (site.theme_color.count)
        {
        write(arena, "<meta name='theme-color' content='%S' />\n", site.theme_color);
        write(arena, "<meta name='msapplication-TileColor' content='%S' />\n", site.theme_color);
        }

        if (css.count)
        {
        write(arena, "<style type='text/css'>%S</style>\n", css);
        }

        if (rss_feed.count)
        {
        write(arena, "<link rel='alternate' type='application/rss+xml' title='Nick Aversano' href='%S/feed.xml' />\n", site.url);
        }

        write(arena, "</head>\n");
        write(arena, "<body>\n");

        write(arena, "<div class='flex-x pad-64  md:flex-y md:csy-8 sm:pad-32'>\n");
            write(arena, "<div class='csx-16 flex-1 flex-x center-y'>\n");
                write(arena, "<h1><a href='%S'>%S</a></h1>\n", site.url, site.name);
            write(arena, "</div>\n");

            write(arena, "<div class='csx-16 flex-x'>\n");

            for (Each_Node(it, site.social_icons))
            {
                auto name  = node_get_child(it, 0)->string;
                auto image = node_get_child(it, 1)->string;
                auto url   = node_get_child(it, 2)->string;

                auto content = os_read_entire_file(path_join(data_dir, S("icons"), image));

                write(arena, "<a title='%S' href='%S' target='_blank' class='inline-flex center pad-8'><div class='inline-block size-24'>%S</div></a>\n", name, url, content);
            }
            write(arena, "</div>\n");
        write(arena, "</div>\n");

        write(arena, "<div class='pad-64 w-800 sm:pad-32'>\n");

            for (Each_Node(it, page_root->first_child))
            {
                if (node_has_children(it)) continue;

                auto lines = string_split(it->string, S("\n"));
                For (lines)
                {
                    if (!string_trim_whitespace(it).count) continue;
                    write(arena, "<p>%S</p>\n", it);
                }
            }

            // @Incomplete: make this feed into some sort of custom tag parser thing
            if (string_equals(page_slug, S("index")))
            {
                write(arena, "<div class='flex-y csy-16'>\n");
                for (Each_Node(it, posts->first_child))
                {
                    // @Cleanup: root root situation
                    auto post = parse_page_meta(it->first_child);
                    auto date = pretty_date(ParsePostDate(post.date));

                    write(arena, "<div class='flex-y csy-8'>\n");
                    if (post.image.count)
                    {
                    write(arena, "<img src='%S' />", post.image);
                    }
                    write(arena, "<div><b>%S</b></div> <div>%S</div>\n", post.title, date);
                    write(arena, "</div>\n");
                }
                write(arena, "</div>\n");
            }

        write(arena, "</div>\n");

        write(arena, "</body>\n");
        write(arena, "</html>\n");

        auto html = arena_to_string(arena);
        os_write_entire_file(path_join(output_dir, sprint("%S.html", page_slug)), html);
    }

    print("Done! Took %.2fms", os_time_in_miliseconds());
}