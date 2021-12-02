#include <stdio.h>
#include <assert.h>
#include <intrin.h>

#include "nja.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "deps/stb_image.h"

/* TODO(nick):
  - process image assets (resize)
  - create blog files structure

  Bonus fun:
  - custom PNG parser
  - custom JPEG parser
  - custom image resizer (downsample only)
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

String EscapeSingleQuotes(String str) {
  Arena *arena = &global_temporary_storage;
  auto parts = string_list_split(arena, str, S("'"));
  return string_list_join(arena, &parts, S("\\'"));
}

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

void ParsePostMeta(String contents) {
  if (string_starts_with(contents, S("---"))) {
    contents.data  += 3;
    contents.count -= 3;
  }
}

bool WriteHtmlPage(String path, Html_Site &site, Html_Meta &meta, String body) {
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
  i32 image_sizes[] = {1050, 525, 263, 132, 16, 32, 48, 65, 57, 60, 72, 76, 96, 114, 128, 144, 152, 160, 167, 180, 196, 228};
  for (int i = 0; i < count_of(image_sizes); i ++) {
    i32 size = image_sizes[i];
    fprintf(file, "<link rel='icon' type='image/png' href='/r/logo.png' sizes='%dx%d' />\n", size, size);
    fprintf(file, "<link rel='apple-touch-icon' href='/r/logo.png' sizes='%dx%d' />\n", size, size);
  }
  
  // styles
  fprintf(file, "<style type='text/css'>\n");
  fprintf(file, "html{margin:0;background:#000;font-family:Consolas,Menlo-Regular,monospace;font-size:16px;min-height:100vh}");
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

  Arena *arena = &global_temporary_storage;
  String_List list = make_string_list();

  string_list_print(arena, &list, "Hello");
  string_list_print(arena, &list, ", Sailor!");

  String body = string_list_join(arena, &list, {});

  print("%.*s\n", LIT(body));


  #if 0
  unsigned char buf[16] = {0};
  auto hash = ComputeAssetHash(buf, count_of(buf), DefaultSeed);
  print_bytes((char *)&hash.value, 8);

  for (int i = 0; i < 26 * 3; i += 1) {
    print("%s\n", cn(i));
  }
  #endif

  auto content = os_read_entire_file(S("C:/dev/myspace/assets/blog/post_0001.txt"));
  ParsePostMeta(content);
  print("%.*s\n", LIT(content));

  PushStyleRule(S("font-size:14px"));

  String out = S("bin/index.html");
  WriteHtmlPage(out, site, meta, S("Hello, Sailor!"));

  return 0;
}