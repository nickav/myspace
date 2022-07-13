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

    auto text = os_read_entire_file(path_join(exe_dir, S("../data/site.meta")));
    auto tokens = tokenize(text);

    print("--- Tokenize ---\n");

    For (tokens)
    {
        print("%S: %S\n", token_type_to_string(it.type), it.value);
    }

    auto root = parse(temp_arena(), tokens);

    print("\n");
    print("--- Parse ---\n");

    for (Each_Node(it, root->first_child))
    {
        print("Node: %S ", it->string);

        for (Each_Node(tag, it->first_tag))
        {
            print("@%S ", tag->string);
        }

        print("\n");

        for (Each_Node(child, it->first_child))
        {
            print("  Child: %S\n", child->string);
        }
    }

    auto title = find_by_name(root->first_child, S("title"));
    dump(node_to_string(title->first_child));
}