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

    /*
    Node *pages;
    Node *projects;
    Node *links;
    Node *social_icons;
    */
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


String markdown_to_html(String text)
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
    u8 *data = PushArray(temp_arena(), u8, count);
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

String find_yaml_frontmatter(String str) {
    if (string_starts_with(str, S("---"))) {

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


String apply_basic_markdown_styles(String text)
{
    print("1\n");
    Arena *arena = arena_alloc_from_memory(gigabytes(1));
    
    print("2\n");
    arena_print(arena, "%S", text);

    return arena_to_string(arena);
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

    auto contents = os_read_entire_file(path_join(data_dir, S("simple.md")));

    auto yaml = find_yaml_frontmatter(contents);

    string_advance(&contents, yaml.count);

    auto out = apply_basic_markdown_styles(S("hello  *bold* _italic_ ~strike~ `code`  _*bold italic*_"));

    print("%S", out);

    print("Done!\n");
    return 0;
}