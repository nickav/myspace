//
// Basic
//

#ifndef NULL
  #define NULL 0
#endif

#ifndef null
  #define null nullptr
#endif

#include <stdio.h>
#define print printf

#ifdef DEBUG
  #define assert(expr) do { if (!(expr)) { print("%s %d: assert failed: %s\r\n", __FILE__, __LINE__, #expr); fflush(stdout); *(volatile int *)0 = 0; } } while (0)
#else
  #define assert(expr)
#endif

#define cast(type) (type)

#define count_of(array) (sizeof(array) / sizeof((array)[0]))

#define kilobytes(value) (value * 1024LL)
#define megabytes(value) (value * 1024LL * 1024LL)
#define gigabytes(value) (value * 1024LL * 1024LL * 1024LL)
#define terabytes(value) (value * 1024LL * 1024LL * 1024LL * 1024LL)


#if defined(_WIN32)
  #define OS_WIN32 1
#elif defined(__APPLE__)
  #define OS_MACOS 1
#elif defined(__linux__)
  #define OS_LINUX 1
#else
  #error "[OS] Unsupported operating system!"
#endif

#ifndef OS_WIN32
  #define OS_WIN32 0
#endif

#ifndef OS_MACOS
  #define OS_MACOS 0
#endif

#ifndef OS_LINUX
  #define OS_LINUX 0
#endif

//
// Types
//
#include <stdint.h>

typedef uint8_t  u8;
typedef int8_t   i8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

#define U8_MAX  0xff
#define I8_MAX  0x7f
#define I8_MIN  0x80
#define U16_MAX 0xffff
#define I16_MAX 0x7fff
#define I16_MIN 0x8000
#define U32_MAX 0xffffffff
#define I32_MAX 0x7fffffff
#define I32_MIN 0x80000000
#define U64_MAX 0xffffffffffffffff
#define I64_MAX 0x7fffffffffffffff
#define I64_MIN 0x8000000000000000

#define F32_MIN FLT_MIN
#define F32_MAX FLT_MAX
#define F64_MIN DBL_MIN
#define F64_MAX DBL_MAX

//
// Memory
//
void *memory_copy(void *from, void *to, u64 size) {
  u8 *src = cast(u8 *)from;
  u8 *dest = cast(u8 *)to;

  while (size--) { *dest++ = *src++; }

  return to;
}

bool memory_equals(void *a_in, void *b_in, u64 size) {
  u8 *a = cast(u8 *)a_in;
  u8 *b = cast(u8 *)b_in;

  while (size--) {
    if (*a++ != *b++) {
      return false;
    }
  }

  return true;
}

void memory_set(void *ptr, u8 value, u64 size) {
  u8 *at = cast(u8 *)ptr;
  while (size--) { *at++ = value; }
}

void memory_zero(void *ptr, u64 size) {
  u8 *at = cast(u8 *)ptr;
  while (size--) { *at++ = 0; }
}

//
// Context
//

struct Arena {
  u8 *data;
  u64 offset;
  u64 size;
};

const u64 temporary_storage_size = megabytes(32);
u8 temporary_storage_buffer[temporary_storage_size];
Arena global_temporary_storage = {temporary_storage_buffer, 0, temporary_storage_size};

void *talloc(u64 size) {
  Arena *arena = &global_temporary_storage;
  void *result = NULL;

  if (arena->offset + size < arena->size) {
    result = arena->data + arena->offset;
    arena->offset += size;
  }

  return result;
}

void tpop(u64 size) {
  Arena *arena = &global_temporary_storage;

  if (size > arena->offset) {
    arena->offset = 0;
  } else {
    arena->offset -= size;
  }
}

void reset_temporary_storage() {
  Arena *arena = &global_temporary_storage;
  arena->offset = 0;
  memory_zero(arena->data, arena->size);
}

//
// String
//
struct String {
  u64 count;
  u8 *data;

  u8 &operator[](u64 i) {
    assert(i >= 0 && i < count);
    return data[i];
  }
};

#define S(x) String{sizeof(x) - 1, cast(u8 *)x}

#define LIT(str) cast(int)str.count, cast(char *)str.data

struct String16 {
  u64 count;
  u16 *data;
};

struct String32 {
  u64 count;
  u32 *data;
};

struct StringNode {
  String it;
  String *next;
};

struct StringList {
  StringNode *first;
  StringNode *last;
  u32 count;
  u32 size_in_bytes;
};

struct StringDecode {
  u32 codepoint;
  u8 size; // 1 - 4
};

String make_string(u8 *data, u64 count) {
  return String{count, data};
}

char *cstr(String str) {
  if (!str.count || !str.data) {
    return NULL;
  }

  char *result = cast(char *)talloc(str.count + 1); // size for null character
  memory_copy(str.data, result, str.count);
  result[str.count] = 0;
  return result;
}

StringDecode string_decode_utf8(u8 *str, u32 capacity) {
  static u8 high_bits_to_count[] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 3, 3, 4, 0,
  };

  StringDecode result = {'?', 1};

  u8 count = high_bits_to_count[str[0] >> 3];

  // @Speed: this could be made branchless
  if (capacity >= count) {
    switch (count) {
      case 1: {
        result.size = 1;
        result.codepoint = str[0] & 0x7f;
      } break;

      case 2: {
        result.size = 2;
        result.codepoint = ((str[0] & 0x1f) << 6) | (str[1] & 0x3f);
      } break;

      case 3: {
        result.size = 3;
        result.codepoint = ((str[0] & 0x0f) << 12) | ((str[1] & 0x3f) << 6) | (str[2] & 0x3f);
      } break;

      case 4: {
        result.size = 4;
        result.codepoint = ((str[0] & 0x09) << 18) | ((str[1] & 0x3f) << 12) | ((str[2] & 0x3f) << 6) | (str[3] & 0x3f);
      } break;
    }
  }

  return result;
}

u32 string_encode_utf8(u8 *dest, u32 codepoint) {
  u32 size = 0;

  if (codepoint <= 0x7f) {
    size = 1;
    dest[0] = codepoint;
  }
  else if (codepoint <= 0x7ff) {
    size = 2;
    dest[0] = 0xC0 | (codepoint >> 6);
    dest[1] = 0x80 | (codepoint & 0x3f);
  }
  else if (codepoint <= 0xffff) {
    size = 3;
    dest[0] = 0xE0 | (codepoint >> 12);
    dest[1] = 0x80 | ((codepoint >> 6) & 0x3f);
    dest[2] = 0x80 | (codepoint & 0x3f);
  }
  else if (codepoint <= 0x10FFFF) {
    size = 4;
    dest[0] = 0xF0 | (codepoint >> 18);
    dest[1] = 0x80 | ((codepoint >> 12) & 0x3f);
    dest[2] = 0x80 | ((codepoint >> 6) & 0x3f);
    dest[3] = 0x80 | (codepoint & 0x3f);
  }
  else {
    dest[0] = '?';
    size = 1;
  }

  return size;
}

StringDecode string_decode_utf16(u16 *str, u32 capacity) {
  StringDecode result = {'?', 1};

  u16 x = str[0];

  if (x < 0xD800 || x > 0xDFFF) {
    result.codepoint = x;
    result.size = 1;
  } else if (capacity >= 2) {
    u16 y = str[1];

    // @Robustness: range check?
    result.codepoint = ((x - 0xD800) << 10 | (y - 0xDC00)) + 0x10000;
    result.size = 2;
  }

  return result;
}

u32 string_encode_utf16(u16 *dest, u32 codepoint) {
  u32 size = 0;

  if (codepoint < 0x10000) {
    dest[0] = codepoint;
    size = 1;
  } else {
    u32 x = (codepoint - 0x10000);
    dest[0] = (x >> 10) + 0xD800;
    dest[1] = (x & 0x3ff) + 0xDC00;
    size = 2;
  }

  return size;
}

String32 str32_from_str8(String str) {
  u32 *memory = cast(u32 *)talloc(str.count * sizeof(u32));

  u8 *p0 = str.data;
  u8 *p1 = str.data + str.count;
  u32 *at = memory;
  u64 remaining = str.count;

  while (p0 < p1) {
    auto decode = string_decode_utf8(p0, remaining);

    *at = decode.codepoint;
    p0 += decode.size;
    at += 1;
    remaining -= decode.size;
  }

  u64 alloc_count = str.count;
  u64 string_count = cast(u64)(at - memory);
  u64 unused_count = string_count - alloc_count;

  tpop(unused_count * sizeof(u32));

  String32 result = {string_count, memory};
  return result;
}

String str8_from_str32(String32 str) {
  u8 *memory = cast(u8 *)talloc(str.count * 4);

  u32 *p0 = str.data;
  u32 *p1 = str.data + str.count;
  u8 *at = memory;

  while (p0 < p1) {
    auto size = string_encode_utf8(at, *p0);

    p0 += 1;
    at += size;
  }

  u64 alloc_count = str.count * 4;
  u64 string_count = cast(u64)(at - memory);
  u64 unused_count = alloc_count - string_count;

  tpop(unused_count);

  String result = {string_count, memory};
  return result;
}

String16 str16_from_str8(String str) {
  u16 *memory = cast(u16 *)talloc((str.count + 1) * sizeof(u16));

  u8 *p0 = str.data;
  u8 *p1 = str.data + str.count;
  u16 *at = memory;
  u64 remaining = str.count;

  while (p0 < p1) {
    auto decode = string_decode_utf8(p0, remaining);
    u32 encode_size = string_encode_utf16(at, decode.codepoint);

    at += encode_size;
    p0 += decode.size;
    remaining -= decode.size;
  }

  *at = 0;

  u64 alloc_count = str.count + 1;
  u64 string_count = cast(u64)(at - memory);
  u64 unused_count = alloc_count - string_count - 1;

  tpop(unused_count * sizeof(u16));

  String16 result = {string_count, memory};
  return result;
}

String str8_from_str16(String16 str) {
  String result = {};
  result.data = cast(u8 *)talloc(str.count * 2);

  u16 *p0 = str.data;
  u16 *p1 = str.data + str.count;
  u8 *at = result.data;

  while (p0 < p1) {
    auto decode = string_decode_utf16(p0, cast(u64)(p1 - p0));
    u32 encode_size = string_encode_utf8(at, decode.codepoint);

    p0 += decode.size;
    at += encode_size;
  }

  result.count = at - result.data;

  u64 alloc_size = str.count * 2;
  u64 unused_size = (result.count - alloc_size);
  tpop(unused_size);

  return result;
}

//
// OS
//

#if OS_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

void os_init() {
  reset_temporary_storage();
}

String os_read_entire_file(String path) {
  String result = {};

  // @Cleanup: do we always want to null-terminate String16 s?
  String16 str = str16_from_str8(path);
  HANDLE handle = CreateFileW(cast(WCHAR *)str.data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  if (handle == INVALID_HANDLE_VALUE) {
    print("[file] Error reading entire file: (error code: %d) for %.*s\n", GetLastError(), LIT(path));
    return result;
  }

  DWORD hi_size = 0;
  DWORD lo_size = GetFileSize(handle, &hi_size);

  u64 size = ((cast(u64)hi_size) << 32) | cast(u64)lo_size;

  result.data = cast(u8 *)talloc(size + 1); // space for null character.
  result.data[size] = 0;
  result.count = size;

  DWORD bytes_read = 0;
  BOOL success = ReadFile(handle, (void *)result.data, size, &bytes_read, 0);

  if (!success) {
    print("[file] Failed to read entire file: %.*s\n", LIT(path));
  } else if (bytes_read != size) {
    // @Robustness: should this keep reading until it reads everything?
    print("[file] Warning byte read mismatch, expected %d but got %d for file: %.*s\n", size, bytes_read, LIT(path));
  }

  CloseHandle(handle);

  return result;
}

bool os_write_entire_file(String path, String contents) {
  String16 str = str16_from_str8(path);
  HANDLE handle = CreateFileW(cast(WCHAR *)str.data, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  if (handle == INVALID_HANDLE_VALUE) {
    print("[file] Error writing entire file: (error code: %d) for %.*s\n", GetLastError(), LIT(path));
    return false;
  }

  // @Robustness: handle > 32 byte content size
  assert(contents.count < U32_MAX);

  DWORD bytes_written;
  BOOL success = WriteFile(handle, contents.data, contents.count, &bytes_written, 0);

  if (!success) {
    print("[file] Failed to write entire file: %.*s\n", LIT(path));
    return false;
  } else if (bytes_written != contents.count) {
    // @Robustness: keep writing until everything is written?
    print("[file] Warning byte read mismatch, expected %d but got %d for file: %.*s\n", contents.count, bytes_written, LIT(path));
    return false;
  }

  CloseHandle(handle);

  return true;
}

bool os_delete_file(String path) {
  String16 str = str16_from_str8(path);
  return DeleteFileW(cast(WCHAR *)str.data);
}

bool os_make_directory(String path) {
  String16 str = str16_from_str8(path);
  BOOL success = CreateDirectoryW(cast(WCHAR *)str.data, NULL);
  return success;
}

bool os_remove_directory(String path) {
  String16 str = str16_from_str8(path);
  BOOL success = RemoveDirectoryW(cast(WCHAR *)str.data);
  return success;
}

void normalize_path(String path) {
  u8 *at = path.data;

  for (u64 i = 0; i < path.count; i++) {
    if (*at == '\\') {
      *at = '/';
    }

    at ++;
  }
}

String os_get_executable_path() {
  WCHAR buffer[1024];

  DWORD length = GetModuleFileNameW(NULL, buffer, sizeof(buffer));
  if (length == 0) {
    return {};
  }

  String16 temp = {length, cast(u16 *)buffer};
  String result = str8_from_str16(temp);
  normalize_path(result);

  return result;
}

String os_get_current_directory() {
  WCHAR buffer[1024];

  DWORD length = GetCurrentDirectoryW(sizeof(buffer), buffer);
  if (length == 0) {
    return {};
  }

  String16 temp = {length, cast(u16 *)buffer};
  String result = str8_from_str16(temp);
  normalize_path(result);

  return result;
}


#endif

//
// Main
//

void test() {
  print("hello, sailor!\n");

  {
    u8 buf1[] = {127};
    auto res1 = string_decode_utf8(buf1, count_of(buf1));
    print("res1: %llu (%d)\n", res1.codepoint, res1.size);

    u8 buf2[] = {0xC2, 0xA2};
    auto res2 = string_decode_utf8(buf2, count_of(buf2));
    print("res2: %llu (%d)\n", res2.codepoint, res2.size);

    u8 buf3[] = {0xE2, 0x82, 0xAC};
    auto res3 = string_decode_utf8(buf3, count_of(buf3));
    print("res3: %llu (%d)\n", res3.codepoint, res3.size);

    u8 buf4[] = {0xF0, 0x90, 0x8D, 0x88};
    auto res4 = string_decode_utf8(buf4, count_of(buf4));
    print("res4: %llu (%d)\n", res4.codepoint, res4.size);

    u8 enc1[4] = {};
    string_encode_utf8(enc1, res1.codepoint);
    assert(memory_equals(buf1, enc1, count_of(buf1)));

    u8 enc2[4] = {};
    string_encode_utf8(enc2, res2.codepoint);
    assert(memory_equals(buf2, enc2, count_of(buf2)));

    u8 enc3[4] = {};
    string_encode_utf8(enc3, res3.codepoint);
    assert(memory_equals(buf3, enc3, count_of(buf3)));

    u8 enc4[4] = {};
    string_encode_utf8(enc4, res4.codepoint);
    assert(memory_equals(buf4, enc4, count_of(buf4)));
    print("enc4: %X %X %X %X\n", enc4[0], enc4[1], enc4[2], enc4[3]);
  }

  {
    String test_string_array[] = {
      S("Hello"),
      S("Sailor"),
      S("Would you kindly 034234?"),
      //S("\x01\x02\x03\x04\x05\x06\x07\x08\n\t\r"),
    };

    for (u32 i = 0; i < count_of(test_string_array); i ++) {
      auto it = test_string_array[i];
      auto str32 = str32_from_str8(it);
      auto str8 = str8_from_str32(str32);

      print("%.*s\n", LIT(it));
      print("%.*s\n", LIT(str8));

      auto str16 = str16_from_str8(it);
      auto str8_2 = str8_from_str16(str16);

      print("%.*s\n", LIT(str8_2));
    }
  }
}

int main(int argc, char **argv) {
  os_init();

  auto cwd = os_get_current_directory();

  print("%.*s\n", LIT(cwd));

  #if 0
  String contents = os_read_entire_file(S("C:/Users/nick/dev/myspace/src/stb_sprintf.h"));
  print("%.*s\n", LIT(contents));
  bool success = os_write_entire_file(S("C:/Users/nick/dev/myspace/bin/stb_sprintf.h"), contents);
  print("%d\n", success);
  #endif

  return 0;
}