#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "deps/stb_sprintf.h"
#include "na.h"

#if 0
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "deps/stb_image_resize.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "deps/stb_image_write.h"
#endif

#include "assets.cpp"

struct Build_Config {
    String asset_dir;
    String output_dir;
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

String GenerateSiteHTMLFromTemplate(String html_template, String *replacement_pairs, u64 replacement_pairs_count)
{
    Arena arena;
    arena_init_from_backing_memory(&arena, os_virtual_memory(), megabytes(1));

    String at = html_template;

    while (at.count > 2)
    {
        String token = {};

        // NOTE(nick): we don't need to do the whole "string starts with" thing every time
        // though, I do wonder _why_ that particular call is so slow...
        if (at.data[0] == '{' && at.data[1] == '{')
        {
            for (u64 i = 0; i < replacement_pairs_count; i += 2)
            {
                auto rfrom = replacement_pairs[i + 0];
                auto rto = replacement_pairs[i + 1];

                if (string_starts_with(at, rfrom))
                {
                    arena_write(&arena, rto);
                    string_advance(&at, rfrom.count);
                    continue;
                }
            }
        }

        arena_write(&arena, at.data, 1);
        string_advance(&at, 1);
    }

    arena_write(&arena, at.data, at.count);
    string_advance(&at, at.count);

    return arena_to_string(&arena);
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

int main() {
    os_init();
    auto start_time = os_time_in_miliseconds();

    auto build_dir = os_get_executable_directory();
    auto project_root = path_dirname(build_dir);

    config.asset_dir  = path_join(project_root, S("assets"));
    config.output_dir = path_join(build_dir, S("bin"));

    auto style = os_read_entire_file(path_join(config.asset_dir, S("style.css")));
    auto t1 = os_time_in_miliseconds();

    Arena arena;
    arena_init_from_backing_memory(&arena, os_virtual_memory(), megabytes(1));

    #define W(str, ...) string_print(&arena, str "\n", __VA_ARGS__)

    W("<!doctype html>");
    W("<html lang='en'>");
    W("<head>");
    W("<meta charset='utf-8' />");
    W("<meta name='viewport' content='width=device-width, initial-scale=1' />");

    W("<title>%S</title>", S("Nick Aversano"));
    W("<meta name='description' content='%S' />", S("This is my new website"));
    
    W("<meta itemprop='name' content='{{title}}'>");
    W("<meta itemprop='description' content='{{description}}'>");
    W("<meta itemprop='image' content='{{image}}'>");

    W("<meta property='og:title' content='{{title}}' />");
    W("<meta property='og:description' content='{{description}}' />");
    W("<meta property='og:type' content='{{og_type}' />");
    W("<meta property='og:url' content='{{url}}' />");
    W("<meta property='og:site_name' content='{{site_name}}' />");
    W("<meta property='og:locale' content='en_us' />");

    W("<meta name='twitter:card' content='summary' />");
    W("<meta name='twitter:title' content='{{title}}' />");
    W("<meta name='twitter:description' content='{{description}}' />");
    W("<meta name='twitter:image' content='{{image}}' />");
    W("<meta name='twitter:site' content='{{twitter_handle}}' /> ");

    W("<style type='text/css'>%S</style>", style);
    W("</head>");
    W("<body>");
    W("<header>Nick Aversano</header>");
    W("<div>Hello, Sailor!</div>");
    W("</body>");
    W("</html>");

    #undef W

    auto result = arena_to_string(&arena);

    //print("%S\n", result);

    auto t2 = os_time_in_miliseconds();
    print("Style 1 Took %.2fms\n", t2 - t1);

    os_write_entire_file(path_join(config.output_dir, S("index.html")), result);

    //OS_WriteEntireFile(path_join(config.output_dir, S("index.html")), result);


    auto html = os_read_entire_file(path_join(config.asset_dir, S("index.html")));
    auto t3 = os_time_in_miliseconds();

    String replacement_pairs[] = {
        S("{{title}}"), S("Nick Aversano"),
        S("{{description}}"), S("A place to do things"),
        S("{{image}}"), S(""),
        S("{{os_type}}"), S("article"),
        S("{{site_name}}"), S("Nick Aversano"),
        S("{{twitter_handle}}"), S("@nickaversano"),


        S("{{head}}"), sprint("<style type='text/css'>%S</style>", style),
        S("{{body}}"), S("Hey!"),
    };

    html = GenerateSiteHTMLFromTemplate(html, replacement_pairs, count_of(replacement_pairs));

    auto t4 = os_time_in_miliseconds();
    print("Style 2 Took %.2fms\n", t4 - t3);
    os_write_entire_file(path_join(config.output_dir, S("index2.html")), html);

    #if 0
    auto content = os_read_entire_file(path_join(config.asset_dir, S("blog/post_0001.txt")));
    auto post = ParsePost(content);
    print("title: %S\n", post.title);
    print("description: %S\n", post.description);
    print("date: %S\n", post.date);
    print("body: %S\n", post.body);

    auto files = os_scan_directory(os_allocator(), path_join(config.asset_dir, S("blog")));

    For (files) {
        print("%S\n", it.name);
    }
    #endif

    auto end_time = os_time_in_miliseconds();

    print("Done! Took %.2fms\n", end_time - start_time);

    return 0;
}