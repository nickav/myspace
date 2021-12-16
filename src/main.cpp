#include <stdio.h>
#include <assert.h>
#include <intrin.h>

#include "nja.h"

#define STBI_NO_BMP
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "deps/stb_image_resize.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "deps/stb_image_write.h"

/* TODO(nick):
  - create site look / homepage
  - 404 page
  - create blog files structure
*/

struct Html_Site {
  String name;
  String twitter_handle;
  String locale;
  String logo_path;
  String theme_color;
};

struct Html_Meta {
  String title;
  String description;
  String image_url;
  String url;
  String og_type;
};

struct Build_State {
  String asset_dir;
  String output_dir;
};

struct Image {
  u32 width;
  u32 height;
  u8 *pixels;
};

struct Asset_Hash {
  __m128i value;
};

static char unsigned OverhangMask[32] =
{
  255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,
  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
};

static char unsigned DefaultSeed[16] =
{
  178, 201, 95, 240, 40, 41, 143, 216,
  2, 209, 178, 114, 232, 4, 176, 188
};

static Asset_Hash ComputeAssetHash(char unsigned *At, size_t Count, char unsigned *Seedx16)
{
    /* TODO(casey):
      Consider and test some alternate hash designs.  The hash here
      was the simplest thing to type in, but it is not necessarily
      the best hash for the job.  It may be that less AES rounds
      would produce equivalently collision-free results for the
      problem space.  It may be that non-AES hashing would be
      better.  Some careful analysis would be nice.
    */

    // TODO(casey): Double-check exactly the pattern
    // we want to use for the hash here

    Asset_Hash Result = {0};

    // TODO(casey): Should there be an IV?
    __m128i HashValue = _mm_cvtsi64_si128(Count);
    HashValue = _mm_xor_si128(HashValue, _mm_loadu_si128((__m128i *)Seedx16));

    size_t ChunkCount = Count / 16;
    while(ChunkCount--)
    {
        __m128i In = _mm_loadu_si128((__m128i *)At);
        At += 16;

        HashValue = _mm_xor_si128(HashValue, In);
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
        HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    }

    size_t Overhang = Count % 16;

#if 0
    __m128i In = _mm_loadu_si128((__m128i *)At);
#else
    // TODO(casey): This needs to be improved - it's too slow, and the #if 0 branch would be nice but can't
    // work because of overrun, etc.
    char Temp[16];
    __movsb((unsigned char *)Temp, At, Overhang);
    __m128i In = _mm_loadu_si128((__m128i *)Temp);
#endif
    In = _mm_and_si128(In, _mm_loadu_si128((__m128i *)(OverhangMask + 16 - Overhang)));
    HashValue = _mm_xor_si128(HashValue, In);
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());
    HashValue = _mm_aesdec_si128(HashValue, _mm_setzero_si128());

    Result.value = HashValue;

    return Result;
}

static bool AssetHashesAreEqual(Asset_Hash a, Asset_Hash b) {
  __m128i compare = _mm_cmpeq_epi32(a.value, b.value);
  int result = _mm_movemask_epi8(compare) == 0xffff;
  return result;
}

String EscapeSingleQuotes(String str) {
  Arena *arena = &temporary_allocator;
  auto parts = string_list_split(arena, str, S("'"));
  return string_list_join(arena, &parts, S("\\'"));
}

static void fprint_bytes(FILE *handle, char *bytes, int length)
{
  for (int i = 0; i < length; i ++) {
    fprintf(handle, "%02x", (unsigned char)bytes[i]);
  }
}

static void print_bytes(char *bytes, int length)
{
  fprint_bytes(stdout, bytes, length);
}

static char *ShortClassName(i32 i) {
  static char result[5] = {};

  i32 j = 0;
  do {
    int index = 4 - j - 1;
    int zero_offset = (j > 0 ? -1 : 0);
    result[index] = 'A' + (i % 52) + zero_offset;

    if (result[index] > (char)'Z') result[index] += ('a' - 'A' - 26);

    i /= 52;
    j += 1;
    assert(j < 5);
  } while (i > 0);

  return result + (4 - j);
}

struct Style {
  String selector;
  String rule;

  Style *next;
};

struct Style_List {
  i32 count;
  Style *first;
  Style *last;
};

static Style_List styles = {};

#define New(struct) (struct *)talloc(sizeof(struct));
#define Alloc(size) talloc(size);

Style *PushStyle() {
  auto it = New(Style);
  *it = {};

  if (styles.last)
  {
    if (!styles.first->next) styles.first->next = styles.last;

    styles.last->next = it;
    styles.last = it;
  }
  else
  {
    styles.first = it;
    styles.last = it;
  }

  styles.count += 1;

  return it;
}

String PushStringCopy(String src) {
  u8 *data = (u8 *)talloc(src.count);
  memory_copy(src.data, data, src.count);
  return make_string(data, src.count);
}

Style *PushStyleRule(String rule) {
  char *cn = ShortClassName(styles.count);
  String selector = string_join(S("."), string_from_cstr(cn));

  auto style = PushStyle();
  style->selector = selector;
  style->rule = PushStringCopy(rule);
  return style;
}

struct Post {
  String title;
  String description;
  String date;
  String image;

  String body;
};

// NOTE(nick): order should match Post fields
static String post_keywords[] = {S("title:"), S("description:"), S("date:"), S("image:")};

Post parse_post(String contents) {
  Post result = {};

  String at = contents;

  if (string_starts_with(at, S("---"))) {
    at.data  += 3;
    at.count -= 3;

    while (at.count > 0) {
      if (char_is_whitespace(at.data[0])) {
        at.count -= 1;
        at.data += 1;
        continue;
      }

      if (string_starts_with(at, S("---"))) {
        at.count -= 3;
        at.data += 3;

        while (at.count > 0 && char_is_whitespace(at.data[0])) {
          at.count -= 1;
          at.data += 1;
        }

        break;
      }

      for (int i = 0; i < count_of(post_keywords); i ++) {
        auto keyword = post_keywords[i];

        if (string_starts_with(at, keyword)) {
          i64 index = string_index(at, S("\n"));
          if (index >= 0) {
            String it = string_trim_whitespace(string_slice(at, keyword.count, index));

            assert(i < offset_of(Post, body) / sizeof(String));
            ((String *)&result)[i] = it;
          }

          at.data += index;
          at.count -= index;
          continue;
        }
      }

      at.count -= 1;
      at.data += 1;
    }
  }

  result.body = at;

  return result;
}

Image parse_image(String image) {
  Image result = {};

  int width = 0;
  int height = 0;
  int channels = 0;
  result.pixels = stbi_load_from_memory(image.data, image.count, &width, &height, &channels, 4);

  result.width = (u32) width;
  result.height = (u32) height;

  return result;
}

Image read_image(String image_path) {
  auto raw_image = os_read_entire_file(image_path);
  auto image = parse_image(raw_image);
  return image;
}

bool write_image(String image_path, Image image) {
  String buf = {};

  int length = 0;
  u8 *data = stbi_write_png_to_mem(image.pixels, 0, image.width, image.height, 4, &length);
  buf.data = data;
  buf.count = (u32)length;

  return os_write_entire_file(image_path, buf);
}

String get_asset_path(Build_State *state, String asset_name) {
  return path_join(state->asset_dir, asset_name);
}

String get_output_path(Build_State *state, String asset_name) {
  return path_join(state->output_dir, asset_name);
}

Image find_image_asset(Build_State *state, String asset_name) {
  auto src_path = path_join(state->asset_dir, asset_name);
  auto raw_image = os_read_entire_file(src_path);

  if (!raw_image.data) {
    return {};
  }

  auto image = parse_image(raw_image);
  return image;
}

int stbir_resize_uint8_pixel_perfect(     const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                           unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                     int num_channels)
{
    return stbir__resize_arbitrary(NULL, input_pixels, input_w, input_h, input_stride_in_bytes,
        output_pixels, output_w, output_h, output_stride_in_bytes,
        0,0,1,1,NULL,num_channels,-1,0, STBIR_TYPE_UINT8, STBIR_FILTER_BOX, STBIR_FILTER_BOX,
        STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP, STBIR_COLORSPACE_LINEAR);
}

String write_image_size_variant(Build_State *state, String asset_name, Image image, u32 width, u32 height, bool want_pixel_perfect) {
  auto name = path_strip_extension(asset_name);
  auto ext = path_get_extension(asset_name);

  Image resized_image = {};
  if (image.width == width && image.height == height) {
    resized_image = image;
  } else {
    u8 *output_pixels = (u8 *)malloc(width * height * sizeof(u32));

    if (want_pixel_perfect) {
      stbir_resize_uint8_pixel_perfect(image.pixels, image.width, image.height, 0, output_pixels, width, height, 0, 4);
    } else {
      stbir_resize_uint8(image.pixels, image.width, image.height, 0, output_pixels, width, height, 0, 4);
    }

    resized_image.width = width;
    resized_image.height = height;
    resized_image.pixels = output_pixels;
  }

  auto hash = ComputeAssetHash(resized_image.pixels, resized_image.width * resized_image.height * sizeof(u32), DefaultSeed);
  u8 *bytes = (u8 *)&hash.value;

  auto postfix = sprint("_%02x%02x%02x%02x%02x%02x%02x%02x", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
  auto variant_name = string_join(name, postfix, ext);

  auto relative_path = path_join(S("r"), variant_name);

  auto out_path = path_join(state->output_dir, relative_path);
  write_image(out_path, resized_image);

  return relative_path;
}

bool WriteHtmlPage(Build_State *state, String page_name, Html_Site &site, Html_Meta &meta, String body) {
  String path = path_join(state->output_dir, page_name);

  FILE *file;
  file = fopen(string_to_cstr(path), "w+");

  // head
  fprintf(file, R""""(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name='viewport' content='width=device-width, initial-scale=1' />
)"""");

  // meta
  fprintf(file, "<title>%.*s</title>\n", LIT(meta.title));
  fprintf(file, "<meta name='description' content='%.*s' />\n", LIT(meta.description));

  fprintf(file, "<meta itemprop='name' content='%.*s' />\n", LIT(meta.title));
  fprintf(file, "<meta itemprop='description' content='%.*s' />\n", LIT(meta.description));
  fprintf(file, "<meta itemprop='image' content='%.*s' />\n", LIT(meta.image_url));

  fprintf(file, "<meta property='og:title' content='%.*s' />\n", LIT(meta.title));
  fprintf(file, "<meta property='og:description' content='%.*s' />\n", LIT(meta.description));
  fprintf(file, "<meta property='og:image' content='%.*s' />\n", LIT(meta.image_url));
  fprintf(file, "<meta property='og:type' content='%.*s' />\n", LIT(meta.og_type));
  fprintf(file, "<meta property='og:url' content='%.*s' />\n", LIT(meta.url));
  fprintf(file, "<meta property='og:site_name' content='%.*s' />\n", LIT(site.name));
  fprintf(file, "<meta property='og:locale' content='%.*s' />\n", LIT(site.locale));

  fprintf(file, "<meta name='twitter:card' content='summary' />\n");
  fprintf(file, "<meta name='twitter:title' content='%.*s' />\n", LIT(meta.title));
  fprintf(file, "<meta name='twitter:description' content='%.*s' />\n", LIT(meta.description));
  fprintf(file, "<meta name='twitter:image' content='%.*s' />\n", LIT(meta.image_url));
  fprintf(file, "<meta name='twitter:site' content='%.*s' />\n", LIT(site.twitter_handle));
  fprintf(file, "<meta name='twitter:creator' content='%.*s' />\n", LIT(site.twitter_handle));

  fprintf(file, "<meta name='theme-color' content='%.*s' />\n", LIT(site.theme_color));
  fprintf(file, "<meta name='msapplication-TileColor' content='%.*s' />\n", LIT(site.theme_color));

  // icons
  i32 image_sizes[] = {1050, 525, 263, 132, 16, 32, 48, 64, 57, 60, 72, 76, 96, 114, 120, 128, 144, 152, 160, 167, 180, 196, 228};
  Image logo = find_image_asset(state, S("logo.png"));
  for (int i = 0; i < count_of(image_sizes); i ++) {
    i32 size = image_sizes[i];
    String asset_path = write_image_size_variant(state, S("logo.png"), logo, size, size, true);

    fprintf(file, "<link rel='icon' type='image/png' href='%.*s' sizes='%dx%d' />\n", LIT(asset_path), size, size);
    fprintf(file, "<link rel='apple-touch-icon' sizes='%dx%d' href='%.*s' />\n", size, size, LIT(asset_path));
  }
  
  // styles
  fprintf(file, "<style type='text/css'>\n");
  fprintf(file, "html{margin:0;background:#fff;font-family:Consolas,Menlo-Regular,monospace;font-size:16px;min-height:100vh}");
  fprintf(file, "body{margin:0;padding:0;display:flex;flex-direction:column;min-height:100vh}");

  Style *it = styles.first;
  while (it != NULL) {
    fprintf(file, "%.*s{%.*s}", LIT(it->selector), LIT(it->rule));
    it = it->next;
  }

  fprintf(file, "\n");
  fprintf(file, "</style>\n");

  // body
  fprintf(file, "</head>\n<body>\n");

  fprintf(file, "%.*s\n", LIT(body)),

  fprintf(file, "</body>\n</html>");

  return fclose(file) == 0;
}

int main(int argc, char **argv)
{
  #if 0
  unsigned char buf[16] = {0};
  auto hash = ComputeAssetHash(buf, count_of(buf), DefaultSeed);
  print_bytes((char *)&hash.value, 8);

  for (int i = 0; i < 26 * 3; i += 1) {
    print("%s\n", cn(i));
  }
  #endif

  //PushStyleRule(S("font-size:14px"));

  auto build_dir = os_get_executable_directory();
  auto project_root = path_dirname(build_dir);
  auto asset_dir = path_join(project_root, S("assets"));

  auto output_dir = path_join(build_dir, S("bin"));
  auto resource_dir = path_join(output_dir, S("r"));

  print("output_dir: %S\n", output_dir);


  Html_Site site = {};
  site.name = S("Nick Aversano");
  site.twitter_handle = S("@nickaversano");
  site.locale = S("en_us");
  site.theme_color = S("#000000");

  Html_Meta meta = {};
  meta.title = S("Nick Aversano");
  meta.description = S("A place to put things.");
  meta.image_url = S("/");
  meta.url = S("http://nickav.co");
  meta.og_type = S("site");

  Build_State state = {};
  state.asset_dir = asset_dir;
  state.output_dir = output_dir;


  //os_delete_entire_directory(output_dir);
  //os_make_directory(output_dir);
  //os_create_directory(resource_dir);

  #if 0
  auto image_path = path_join(asset_dir, S("logo.png"));

  auto raw_image = os_read_entire_file(image_path);
  auto image = parse_image(raw_image);

  write_image(path_join(resource_dir, S("logo2.png")), image);
  #endif

  WriteHtmlPage(&state, S("index.html"), site, meta, S("Hello, Sailor!"));

  auto content = os_read_entire_file(path_join(asset_dir, S("blog/post_0001.txt")));
  auto post = parse_post(content);
  print("title: %S\n", post.title);
  print("description: %S\n", post.description);
  print("date: %S\n", post.date);
  print("body: %S\n", post.body);

  auto post_dir = path_join(asset_dir, S("blog"));

  #if 0
  auto result = os_scan_directory(&temporary_allocator, post_dir);

  for (int i = 0; i < result.count; i ++) {
    auto it = result.files[i];
    auto path = path_join(post_dir, it.name);

    print("%S\n", path);
    print("  name:         %S\n", it.name);
    print("  size:         %d\n", it.size);
    print("  date:         %d\n", it.date);
    print("  is_directory: %d\n", it.is_directory);
  }
  #endif

  //stbir_resize_uint8(pixels);

  return 0;
}