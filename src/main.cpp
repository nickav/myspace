#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"
#include "na.h"

#include "parser.h"

struct Build_Config
{
    String asset_dir;
    String output_dir;
};

struct Page_Meta
{
    String title;
    String description;
    String image;
    String og_type;
    String url;
};

struct Site_Meta
{
    String site_name;
    String site_url;
    String twitter_handle;
};

#include "helpers.h"

int main(int argc, char **argv)
{
    os_init();

    auto exe_dir = os_get_executable_directory();

    auto text = os_read_entire_file(path_join(exe_dir, S("../src/post.meta")));
    auto tokens = tokenize(text);

    print("--- Tokenize ---\n");

    For (tokens)
    {
        print("%S: %S\n", token_type_name_lookup[it.type], it.value);
    }

    auto root = parse(temp_arena(), tokens);

    print("--- Parse ---\n");

    for (Node *it = root->first_child; it != NULL; it = it->next)
    {
        print("Node: %S ", it->string);

        for (Node *tag = it->first_tag; tag != NULL; tag = tag->next)
        {
            print("@%S ", tag->string);
        }

        print("\n");

        for (Node *child = it->first_child; child != NULL; child = child->next)
        {
            print("  Child: %S\n", child->string);
        }
    }

    auto title = find_child_by_name(root, S("title"));
    dump(node_to_string(node_first_child(title)));
}