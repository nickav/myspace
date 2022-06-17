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

struct Asset_Hash {
    __m128i value;
};

struct Image {
    u32 width;
    u32 height;
    u32 *pixels;
};

struct Post {
    String title;
    String description;
    String date;
    String image;

    String body;
};

struct Asset {
    u32 type;
    u64 hash;
    String name;
    String data;
};


struct Assets {
    Array<Asset> assets;
    Hash_Table<String, u64> name_to_index;
};

static Assets global_assets = {};


// NOTE(nick): order should match Post fields
static String post_keywords[] = {S("title:"), S("description:"), S("date:"), S("image:")};


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
        
        #if OS_WINDOWS
        __movsb((unsigned char *)Temp, At, Overhang);
        #else
        memcpy(Temp, At, Overhang);
        #endif

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

#if 0
u32 GetAssetTypeFromName(String name)
{
    String ext = path_get_extension(name);
    if (ext.count)
    {
        if (false) {}
        else if (string_equals(ext, S(".txt"))) { return AssetType_Text; }
        else if (string_equals(ext, S(".png"))) { return AssetType_Image; }
        else if (string_equals(ext, S(".css"))) { return AssetType_CSS; }
        else if (string_equals(ext, S(".html"))) { return AssetType_HTML; }
        else if (string_equals(ext, S(".svg"))) { return AssetType_Text; }
    }

    return AssetType_None;
}
#endif


void AddAsset(String name, String data)
{
    auto it = &global_assets;

    Asset asset = {};
    asset.type = 0;
    asset.name = name; // @Memory?
    asset.data = data; // @Memory?

    array_push(&it->assets, asset);
    u64 index = it->assets.count - 1;

    // @Memory
    table_add(&it->name_to_index, name, index);
}

void LoadAssetsFromDisk(String dir)
{
    auto files = os_scan_files_recursive(temp_allocator(), dir, 10);

    For (files) {
        auto path = path_join(dir, it.name);
        auto name = path_filename(it.name);
        auto data = os_read_entire_file(path);
        AddAsset(name, data);
    }
}

Asset *FindAssetByName(String name)
{
    auto assets = &global_assets;

    auto *index = table_find_pointer(&assets->name_to_index, name);

    if (index != null)
    {
        return &assets->assets[*index];
    }

    return null;
}

Array<Asset *> GetAllPosts() {
    auto assets = &global_assets;

    Array<Asset *> results = {};
    results.allocator = temp_allocator();

    Forp (assets->assets)
    {
        if (string_starts_with(it->name, S("post_")) && string_ends_with(it->name, S(".txt")))
        {
            array_push(&results, it);
        }
    }
    return results;
}


Post ParsePost(String contents) {
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

u64 ParsePostID(String name) {
    i64 i0 = string_index(name, S("_"));
    if (i0 >= 0)
    {
        i64 i1 = string_index(name, S("."), i0 + 1);
        if (i1 >= 0)
        {
            auto str = string_slice(name, i0 + 1, i1);
            return (i64)string_to_i64(str);
        }
    }

    return 0;
}

Image ParseImage(String image) {
    Image result = {};

    int width = 0;
    int height = 0;
    int channels = 0;
    result.pixels = (u32 *)stbi_load_from_memory(image.data, image.count, &width, &height, &channels, 4);

    result.width = (u32) width;
    result.height = (u32) height;

    return result;
}

Image ReadImageFromDisk(String image_path) {
    auto raw_image = os_read_entire_file(image_path);
    auto image = ParseImage(raw_image);
    return image;
}

bool WriteImageToDisk(String image_path, Image image) {
    String buf = {};

    int length = 0;
    u8 *data = stbi_write_png_to_mem((u8 *)image.pixels, 0, image.width, image.height, 4, &length);
    buf.data = data;
    buf.count = (u32)length;

    return os_write_entire_file(image_path, buf);
}
