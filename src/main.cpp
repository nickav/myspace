#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"
#include "na.h"

#include "meta.h"

#define NA_HOTLOADER_IMPLEMENTATION
#include "na_hotloader.h"

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
};

#include "helpers.h"

Site_Meta parse_site_info(Node *root)
{
    Site_Meta result = {};

    result.pages = nil_node();
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
        auto post_title = node_get_child(it, 0)->string;
        auto post_slug  = node_get_child(it, 1)->string;
        auto post_root  = node_get_child(it, 2);

        auto post = parse_page_meta(post_root);
        auto date = ParsePostDate(post.date);
        auto link = path_join(site.url, post_slug);

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

    return arena_to_string(&arena);
}

HOTLOADER_CALLBACK(my_hotloader_callback) {
        String relative_name = change.relative_name;
        print("[hotloader] Changed file: %S\n", relative_name);
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

    //~nja: parse site meta
    auto site_root = parse_entire_file(temp_arena(), path_join(data_dir, S("site.meta")));

    auto site = parse_site_info(site_root);
    if (!site.image.count) site.image = site.icon;

    auto css = os_read_entire_file(path_join(data_dir, S("style.css")));
    css = minify_css(css);

    //~nja: site pages
    auto output_pages = empty_node();
    for (Each_Node(node, site.pages))
    {
        auto page_title = node_get_child(node, 0);
        auto page_slug  = node_get_child(node, 1);

        auto meta_file = path_join(data_dir, sprint("%S.meta", page_slug->string));
        auto page_root = parse_entire_file(temp_arena(), meta_file);

        auto array = make_array_node(page_title, page_slug, page_root);
        node_push_child(output_pages, array);
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
                auto title = meta.title;
                auto slug = path_strip_extension(path_join(S("posts"), it->name));
                post_root->string = slug;

                auto array = make_array_node(node_from_string(title), node_from_string(slug), post_root);
                node_push_child(output_pages, array);
                node_push_child(posts, array);
            }
        }
    }

    //~nja: generate RSS feed
    auto rss_feed = generate_blog_rss_feed(site, posts);
    os_write_entire_file(path_join(output_dir, S("feed.xml")), rss_feed);

    //~nja: output site pages
    os_make_directory(path_join(output_dir, S("posts")));

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
                write(arena, "<h1><a href='%S'>%S</a></h1>\n", S("/"), site.name);
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
                    auto post_title = node_get_child(it, 0)->string;
                    auto post_slug  = node_get_child(it, 1)->string;
                    auto post_root  = node_get_child(it, 2);

                    auto post = parse_page_meta(post_root);
                    auto date = pretty_date(ParsePostDate(post.date));
                    auto link = string_concat(post_slug, S(".html"));

                    write(arena, "<div class='flex-y csy-8'>\n");
                    write(arena, "<a href='%S'>\n", link);
                    if (post.image.count)
                    {
                    write(arena, "<img src='%S' />\n", post.image);
                    }
                    write(arena, "<div><b>%S</b></div> <div>%S</div>\n", post.title, date);
                    write(arena, "</a>\n");
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

    print("Done! Took %.2fms\n", os_time_in_miliseconds());

    os_shell_execute(S("firefox.exe"), string_concat(S("file://"), path_join(output_dir, S("index.html"))));

    #if 0
    //~nja: poll for changes

    print("Watching for changes...\n");

    static Hotloader hot;
    hotloader_init(&hot, path_join(os_get_executable_directory(), S("../src")));
    hotloader_register_callback(&hot, my_hotloader_callback);

    // NOTE(nick): main loop

    while (1)
    {
        auto change = hotloader_poll_events(&hot);
        if (change)
        {
            os_shell_execute(S("cmd"), S("/K 'C:/dev/myspace/build.bat'"));

        }

        os_sleep(100);
    }
    #endif
}