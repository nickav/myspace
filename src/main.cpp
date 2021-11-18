#include <stdio.h>
#include <assert.h>
#include <intrin.h>

#include "nja.h"

/*
TODO(nick):
  - process image assets (resize)
  - create asset hash function (SIMD)
  - actually design the website
  - create blog files structure
*/

struct Html_Site {
  String name;
  String twitter_handle;
  String locale;
  String logo_path;
};

struct Html_Meta {
  String title;
  String description;
  String image_url;
  String url;
  String og_type;
};

String EscapeSingleQuotes(String str) {
  // @Incomplete
  return str;
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

int main(int argc, char **argv)
{
  FILE *file;
  file = fopen("index.html", "w+");

  Html_Site site = {};
  site.name = S("Nick Aversano");
  site.twitter_handle = S("@nickaversano");
  site.locale = S("en_us");

  Html_Meta meta = {};
  meta.title = S("Nick Aversano");
  meta.description = S("A place to put things.");
  meta.image_url = S("/");
  meta.url = S("http://nickav.co");
  meta.og_type = S("site");

  fprintf(file, R""""(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name='viewport' content='width=device-width, initial-scale=1' />
)"""");

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

  i32 image_sizes[] = {1050, 525, 263, 132, 16, 32, 48, 65, 57, 60, 72, 76, 96, 114, 128, 114, 152, 160, 167, 180, 196, 228};
  for (int i = 0; i < count_of(image_sizes); i ++) {
    i32 size = image_sizes[i];
    fprintf(file, "<link rel='icon' type='image/png' href='/r/logo.png' sizes='%dx%d' />\n", size, size);
    fprintf(file, "<link rel='apple-touch-icon' href='/r/logo.png' sizes='%dx%d' />\n", size, size);
  }

  fprintf(file, "</head>\n<body>\n");

  fprintf(file, "Hello, sailor!\n");

  fprintf(file, "</body>\n</html>");

  fclose(file);

  unsigned char buf[16] = {0};
  auto hash = ComputeAssetHash(buf, count_of(buf), DefaultSeed);
  print_bytes((char *)&hash.value, 8);

  #if 0
  String contents = OS_ReadEntireFile(S("C:/Users/nick/dev/myspace/src/stb_sprintf.h"));
  print("%.*s\n", LIT(contents));
  bool success = OS_WriteEntireFile(S("C:/Users/nick/dev/myspace/bin/stb_sprintf.h"), contents);
  print("%d\n", success);
  #endif

  return 0;
}