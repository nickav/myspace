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
    String site_name;
    String site_url;
    String description;
    String author;
    String twitter_handle;

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
};

#include "helpers.h"

Site_Meta parse_site_info(Node *root)
{
    Site_Meta result = {};

    for (Each_Node(it, root->first_child))
    {
        auto key   = it->string;
        auto value = it->first_child->string;

        if (false) {}
        else if (string_equals(key, S("title")))          { result.site_name = value; }
        else if (string_equals(key, S("description")))    { result.description = value; }
        else if (string_equals(key, S("site_url")))       { result.site_url = value; }
        else if (string_equals(key, S("author")))         { result.author = value; }
        else if (string_equals(key, S("twitter_handle"))) { result.twitter_handle = value; } 
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
    }
    return result;
}

int main(int argc, char **argv)
{
    os_init();

    auto exe_dir = os_get_executable_directory();

    auto data_dir   = path_join(exe_dir, S("../data"));
    auto output_dir = path_join(exe_dir, S("bin"));

    auto css = os_read_entire_file(path_join(data_dir, S("style.css")));
    css = minify_css(css);

    auto site_root = parse_entire_file(temp_arena(), path_join(exe_dir, S("../data/site.meta")));

    auto site = parse_site_info(site_root);

    for (Each_Node(page, site.pages))
    {
        auto page_title = node_get_child(page, 0)->string;
        auto page_slug  = node_get_child(page, 1)->string;

        auto meta_file = path_join(exe_dir, sprint("../data/%S.meta", page_slug));
        auto page_root = parse_entire_file(temp_arena(), meta_file);

        if (node_is_nil(page_root)) {
            print("Warning, page '%S' is empty: %S\n", page_slug, meta_file);
            continue;
        }

        auto meta = parse_page_meta(page_root);

        Arena __arena = arena_make_from_memory(megabytes(1));
        Arena *arena = &__arena;

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
        write(arena, "<meta property='og:site_name' content='%S' />\n", site.site_name);
        write(arena, "<meta property='og:locale' content='en_us' />\n");

        write(arena, "<meta name='twitter:card' content='summary' />\n");
        write(arena, "<meta name='twitter:title' content='%S' />\n", meta.title);
        write(arena, "<meta name='twitter:description' content='%S' />\n", meta.description);
        write(arena, "<meta name='twitter:image' content='%S' />\n", meta.image);
        write(arena, "<meta name='twitter:site' content='%S' />\n", site.twitter_handle);

        if (css.count) write(arena, "<style type='text/css'>%S</style>\n", css);

        write(arena, "</head>\n");
        write(arena, "<body>\n");

        write(arena, "<div class='flex-x pad-64  md:flex-y md:csy-8 sm:pad-32'>\n");
            write(arena, "<div class='csx-16 flex-1 flex-x center-y'>\n");
                write(arena, "<h1><a href='%S'>%S</a></h1>\n", site.site_url, site.site_name);
            write(arena, "</div>\n");

            write(arena, "<div class='csx-16 flex-x'>\n");

            for (Each_Node(it, site.social_icons))
            {
                auto name  = node_get_child(it, 0)->string;
                auto image = node_get_child(it, 1)->string;
                auto url   = node_get_child(it, 2)->string;

                auto content = os_read_entire_file(path_join(exe_dir, sprint("../data/icons/%S", image)));

                write(arena, "<a title='%S' href='%S' target='_blank' class='inline-flex center pad-8'><div class='inline-block size-24'>%S</div></a>\n", name, url, content);
            }
            write(arena, "</div>\n");
        write(arena, "</div>\n");

        write(arena, "<div class='pad-64 w-800 sm:pad-32'>\n");
            for (Each_Node(it, page_root->first_child))
            {
                auto key   = it->string;
                auto value = it->first_child->string;

                if (value.count == 0)
                {
                    write(arena, "%S\n", key);
                }
            }
        write(arena, "</div>\n");

        write(arena, "</body>\n");
        write(arena, "</html>\n");

        auto html = arena_to_string(arena);
        os_write_entire_file(path_join(output_dir, sprint("%S.html", page_slug)), html);
    }

    print("Done! Took %.2fms", os_time_in_miliseconds());
}