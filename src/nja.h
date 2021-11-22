#ifndef NJA_H
#define NJA_H

//
// Basic
//

#ifndef NULL
  #define NULL 0
#endif

#ifndef null
  #define null nullptr
#endif

void OS_Print(const char *format, ...);
#define print OS_Print

#ifndef assert
#ifdef DEBUG
  #define assert(expr) do { if (!(expr)) { print("%s %d: assert failed: %s\r\n", __FILE__, __LINE__, #expr); *(volatile int *)0 = 0; } } while (0)
#else
  #define assert(expr)
#endif
#endif

#define cast(type) (type)

#define count_of(array) (sizeof(array) / sizeof((array)[0]))
#define offset_of(Type, member) ((uptr) & (((Type *)0)->member))

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

void *MemoryCopy(void *from, void *to, u64 size) {
  u8 *src = cast(u8 *)from;
  u8 *dest = cast(u8 *)to;

  while (size--) { *dest++ = *src++; }

  return to;
}

bool MemoryEquals(void *a_in, void *b_in, u64 size) {
  u8 *a = cast(u8 *)a_in;
  u8 *b = cast(u8 *)b_in;

  while (size--) {
    if (*a++ != *b++) {
      return false;
    }
  }

  return true;
}

void MemorySet(void *ptr, u8 value, u64 size) {
  u8 *at = cast(u8 *)ptr;
  while (size--) { *at++ = value; }
}

void MemoryZero(void *ptr, u64 size) {
  u8 *at = cast(u8 *)ptr;
  while (size--) { *at++ = 0; }
}

//
// Arenas
//

struct Arena {
  u8 *data;
  u64 offset;
  u64 size;
};

struct Arena_Mark {
  u64 offset;
};

Arena make_arena(u8 *data, u64 size) {
  Arena result = {};
  result.data = data;
  result.size = size;
  return result;
}

void arena_init(Arena *arena, u8 *data, u64 size) {
  *arena = {};
  arena->data = data;
  arena->size = size;
}

void *arena_alloc_aligned(Arena *arena, u64 size, u64 alignment) {
  void *result = NULL;

  assert(alignment >= 1);
  // NOTE(nick): pow2
  assert((alignment & ~(alignment - 1)) == alignment);

  u64 base_address = (u64)(arena->data + arena->offset);
  u64 align_offset = alignment - (base_address & (alignment - 1));
  align_offset &= (alignment - 1);

  size += align_offset;

  if (arena->offset + size < arena->size) {
    result = arena->data + arena->offset + align_offset;
    arena->offset += size;

    // NOTE(nick): make sure our data is aligned properly
    assert((u64)result % alignment == 0);
  }

  return result;
}

void *arena_alloc(Arena *arena, u64 size) {
  return arena_alloc_aligned(arena, size, 8);
}

void arena_pop(Arena *arena, u64 size) {
  if (size > arena->offset) {
    arena->offset = 0;
  } else {
    arena->offset -= size;
  }
}

void arena_reset(Arena *arena) {
  arena->offset = 0;
  MemoryZero(arena->data, arena->size);
}

void *arena_push_size(Arena *arena, u64 size) {
  void *result = 0;

  if (arena->offset + size < arena->size) {
    result = arena->data + arena->offset;
    arena->offset += size;
  }

  return result;
}

#define push_struct(arena, Struct)  \
  (Struct *)arena_push_size(arena, sizeof(Struct))

#define push_array(arena, Struct, count)  \
  (Struct *)arena_push_size(arena, count * sizeof(Struct))

Arena_Mark arena_get_mark(Arena *arena) {
  Arena_Mark result = {};
  result.offset = arena->offset;
  return result;
}

void arena_set_mark(Arena *arena, Arena_Mark mark) {
  arena->offset = mark.offset;
}

const u64 temporary_storage_size = megabytes(32);
static u8 temporary_storage_buffer[temporary_storage_size];
static Arena global_temporary_storage = {temporary_storage_buffer, 0, temporary_storage_size};

// @Robustness: support overflowing
// @Robustness: support arbitrary temp storage size
void *talloc(u64 size) {
  return arena_alloc(&global_temporary_storage, size);
}

void tpop(u64 size) {
  arena_pop(&global_temporary_storage, size);
}

void reset_temporary_storage() {
  arena_reset(&global_temporary_storage);
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

#define LIT(str) (int)str.count, (char *)str.data

String MakeString(u8 *data, u64 count) {
  return String{count, data};
}

char *ToCStr(String str) {
  if (!str.count || !str.data) {
    return NULL;
  }

  char *result = cast(char *)talloc(str.count + 1); // size for null character
  MemoryCopy(str.data, result, str.count);
  result[str.count] = 0;
  return result;
}

i64 CStrLength(char *str) {
  char *at = str;

  while (*at != 0) {
    at ++;
  }

  return at - str;
}

String StringFromCStr(char *data) {
  String result = {};
  i64 length = CStrLength(data);

  if (length > 0) {
    assert(length <= I64_MAX);
    result = {(u64)length, (u8 *)data};
  }

  return result;
}

bool StringEquals(String a, String b) {
  return a.count == b.count && MemoryEquals(a.data, b.data, a.count);
}

bool StringStartsWith(String str, String prefix) {
  return str.count >= prefix.count && MemoryEquals(str.data, prefix.data, prefix.count);
}

String Substring(String str, i64 start_index, i64 end_index) {
  assert(start_index >= 0 && start_index <= str.count);
  assert(end_index >= 0 && end_index <= str.count);
  return MakeString(str.data + start_index, cast(u64)(end_index - start_index));
}

String Substring(String str, i64 start_index) {
  return Substring(str, start_index, str.count);
}

String StringRange(u8 *at, u8 *end) {
  assert(end >= at);
  return MakeString(at, (end - at));
}

i64 StringIndex(String str, String search) {
  for (i64 i = 0; i < str.count; i += 1) {
    if (MemoryEquals(str.data + i, search.data, search.count)) {
      return i;
    }
  }

  return -1;
}

bool StringContains(String str, String search) {
  return StringIndex(str, search) >= 0;
}

String StringJoin(String a, String b) {
  u8 *data = (u8 *)talloc(a.count + b.count);

  MemoryCopy(a.data, data + 0,       a.count);
  MemoryCopy(b.data, data + a.count, b.count);

  return MakeString(data, a.count + b.count);
}

String StringJoin(String a, String b, String c) {
  u8 *data = (u8 *)talloc(a.count + b.count + c.count);

  MemoryCopy(a.data, data + 0,                 a.count);
  MemoryCopy(b.data, data + a.count,           b.count);
  MemoryCopy(c.data, data + a.count + b.count, c.count);

  return MakeString(data, a.count + b.count + c.count);
}

#include <stdarg.h>

extern "C" int stbsp_vsnprintf( char * buf, int count, char const * fmt, va_list va );

i32 print_to_buffer(char *data, i32 max_length, const char *format, va_list args) {
  return stbsp_vsnprintf(data, max_length, format, args);
}

String string_printv(Arena *arena, const char *format, va_list args) {
  // in case we need to try a second time
  va_list args2;
  va_copy(args2, args);

  u64 buffer_size = 1024;
  u8 *buffer = push_array(arena, u8, buffer_size);

  String result = {};
  u64 actual_size = print_to_buffer((char *)buffer, buffer_size, format, args);

  if (actual_size > 0) {
    if (actual_size < buffer_size) {
      arena_pop(arena, buffer_size - actual_size - 1);
      result = MakeString(buffer, actual_size);
    } else {
      arena_pop(arena, buffer_size);
      u8 *fixed_buffer = push_array(arena, u8, actual_size + 1);
      u64 final_size = print_to_buffer((char *)fixed_buffer, actual_size + 1, format, args2);
      result = MakeString(fixed_buffer, final_size);
    }
  }

  va_end(args2);

  return result;
}

String string_print(Arena *arena, const char *format, ...) {
  String result = {};

  va_list args;
  va_start(args, format);
  result = string_printv(arena, format, args);
  va_end(args);

  return result;
}

String sprint(const char *format, ...) {
  String result = {};

  va_list args;
  va_start(args, format);
  result = string_printv(&global_temporary_storage, format, args);
  va_end(args);

  return result;
}

//
// String list
//

struct String_Node {
  String str;
  String_Node *next;
};

struct String_List {
  String_Node *first;
  String_Node *last;

  u64 count;
  u64 size_in_bytes;
};

String_List make_string_list() {
  String_List result = {};
  return result;
}

void string_list_push_explicit(String_List *list, String str, String_Node *node) {
  node->str = str;
  node->next = NULL;

  if (!list->first) list->first = node;
  if (list->last) list->last->next = node;
  list->last = node;

  list->count += 1;
  list->size_in_bytes += str.count;
}

void string_list_push(Arena *arena, String_List *list, String str) {
  String_Node *node = push_struct(arena, String_Node);
  string_list_push_explicit(list, str, node);
}

String_List string_list_split(Arena *arena, String str, String split) {
  String_List result = {};

  u8 *at = str.data;
  u8 *word_first = at;
  u8 *str_end = str.data + str.count;

  for (; at < str_end; at += 1) {
    if (*at == split.data[0])
    {
      if (StringStartsWith(StringRange(at, str_end), split)) {
        String slice = StringRange(word_first, at);
        string_list_push(arena, &result, slice);

        at += split.count - 1;
        word_first = at + 1;
        continue;
      }
    }
  }

  String slice = StringRange(word_first, str_end);
  string_list_push(arena, &result, slice);

  return result;
}

String string_list_join(Arena *arena, String_List *list, String join) {
  u64 size = join.count * (list->count - 1) + list->size_in_bytes;
  u8 *data = push_array(arena, u8, size);

  bool is_mid = false;
  u8 *at = data;
  for (String_Node *it = list->first; it != NULL; it = it->next) {
    if (is_mid) {
      MemoryCopy(join.data, at, join.count);
      at += join.count;
    }

    MemoryCopy(it->str.data, at, it->str.count);
    at += it->str.count;
    is_mid = it->next != NULL;
  }

  return MakeString(data, size);
}

void string_list_print(Arena *arena, String_List *list, const char *format, ...) {
  va_list args;
  va_start(args, format);
  String result = string_printv(arena, format, args);
  va_end(args);

  string_list_push(arena, list, result);
}

//
// String conversions
//

struct String16 {
  u64 count;
  u16 *data;
};

struct String32 {
  u64 count;
  u32 *data;
};

struct String_Decode {
  u32 codepoint;
  u8 size; // 1 - 4
};

String_Decode StringDecodeUtf8(u8 *str, u32 capacity) {
  static u8 high_bits_to_count[] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 2, 3, 3, 4, 0,
  };

  String_Decode result = {'?', 1};

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

u32 StringEncodeUtf8(u8 *dest, u32 codepoint) {
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

String_Decode StringDecodeUtf16(u16 *str, u32 capacity) {
  String_Decode result = {'?', 1};

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

u32 StringEncodeUtf16(u16 *dest, u32 codepoint) {
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

String32 String32FromString(String str) {
  u32 *memory = cast(u32 *)talloc(str.count * sizeof(u32));

  u8 *p0 = str.data;
  u8 *p1 = str.data + str.count;
  u32 *at = memory;
  u64 remaining = str.count;

  while (p0 < p1) {
    auto decode = StringDecodeUtf8(p0, remaining);

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

String StringFromString32(String32 str) {
  u8 *memory = cast(u8 *)talloc(str.count * 4);

  u32 *p0 = str.data;
  u32 *p1 = str.data + str.count;
  u8 *at = memory;

  while (p0 < p1) {
    auto size = StringEncodeUtf8(at, *p0);

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

String16 String16FromString(String str) {
  u16 *memory = cast(u16 *)talloc((str.count + 1) * sizeof(u16));

  u8 *p0 = str.data;
  u8 *p1 = str.data + str.count;
  u16 *at = memory;
  u64 remaining = str.count;

  while (p0 < p1) {
    auto decode = StringDecodeUtf8(p0, remaining);
    u32 encode_size = StringEncodeUtf16(at, decode.codepoint);

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

String StringFromString16(String16 str) {
  String result = {};
  result.data = cast(u8 *)talloc(str.count * 2);

  u16 *p0 = str.data;
  u16 *p1 = str.data + str.count;
  u8 *at = result.data;

  while (p0 < p1) {
    auto decode = StringDecodeUtf16(p0, cast(u64)(p1 - p0));
    u32 encode_size = StringEncodeUtf8(at, decode.codepoint);

    p0 += decode.size;
    at += encode_size;
  }

  result.count = at - result.data;

  u64 unused_size = (result.count - str.count);
  tpop(unused_size);

  return result;
}

//
// Functions
//

#define PI       3.14159265359f
#define TAU      6.28318530717958647692f
#define EPSILON  0.00001f
#define EPSILON2 (EPSILON * EPSILON)

#define SQRT_2 0.70710678118

#define MIN(a, b) ((a < b) ? (a) : (b))
#define MAX(a, b) ((a > b) ? (a) : (b))

#define CLAMP(value, lower, upper) (MAX(MIN(value, upper), lower))
#define SIGN(x) ((x > 0) - (x < 0))
#define ABS(x) ((x < 0) ? -(x) : (x))

inline i32 min(i32 a, i32 b) { return MIN(a, b); }
inline u32 min(u32 a, u32 b) { return MIN(a, b); }
inline u64 min(u64 a, u64 b) { return MIN(a, b); }
inline f32 min(f32 a, f32 b) { return MIN(a, b); }
inline f64 min(f64 a, f64 b) { return MIN(a, b); }

inline i32 max(i32 a, i32 b) { return MAX(a, b); }
inline u32 max(u32 a, u32 b) { return MAX(a, b); }
inline u64 max(u64 a, u64 b) { return MAX(a, b); }
inline f32 max(f32 a, f32 b) { return MAX(a, b); }
inline f64 max(f64 a, f64 b) { return MAX(a, b); }

inline i32 clamp(i32 value, i32 lower, i32 upper) { return MAX(MIN(value, upper), lower); }
inline u32 clamp(u32 value, u32 lower, u32 upper) { return MAX(MIN(value, upper), lower); }
inline u64 clamp(u64 value, u64 lower, u64 upper) { return MAX(MIN(value, upper), lower); }
inline f32 clamp(f32 value, f32 lower, f32 upper) { return MAX(MIN(value, upper), lower); }
inline f64 clamp(f64 value, f64 lower, f64 upper) { return MAX(MIN(value, upper), lower); }

inline f32 lerp(f32 a, f32 b, f32 t) {
  return (1 - t) * a + b * t;
}

inline f32 unlerp(f32 a, f32 b, f32 v) {
  return (v - a) / (b - a);
}

inline f32 square(f32 x) {
  return x * x;
}

//
// Math
//

//
// OS
//

#if OS_WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>

void OS_Init() {
  AttachConsole(ATTACH_PARENT_PROCESS);

  reset_temporary_storage();
}

char *print_callback(const char *buf, void *user, int len) {
  DWORD bytes_written;

  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  WriteFile(handle, buf, len, &bytes_written, 0);

  return (char *)buf;
}

#define STB_SPRINTF_IMPLEMENTATION
#include "deps/stb_sprintf.h"

// #define thread_local __declspec(thread)
// #define thread_local __thread

// @Robustness: make this thread-safe
char output_buffer[2 * STB_SPRINTF_MIN];

void OS_Print(const char *format, ...) {
  va_list args;
  va_start(args, format);
  stbsp_vsprintfcb(print_callback, 0, output_buffer, format, args);
  va_end(args);
}

String OS_ReadEntireFile(String path) {
  String result = {};

  // @Cleanup: do we always want to null-terminate String16 s?
  String16 str = String16FromString(path);
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

bool OS_WriteEntireFile(String path, String contents) {
  String16 str = String16FromString(path);
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

bool OS_DeleteFile(String path) {
  String16 str = String16FromString(path);
  return DeleteFileW(cast(WCHAR *)str.data);
}

bool OS_CreateDirectory(String path) {
  String16 str = String16FromString(path);
  BOOL success = CreateDirectoryW(cast(WCHAR *)str.data, NULL);
  return success;
}

bool OS_DeleteDirectory(String path) {
  String16 str = String16FromString(path);
  BOOL success = RemoveDirectoryW(cast(WCHAR *)str.data);
  return success;
}

bool OS_DeleteEntireDirectory(String path) {
  char *find_path = (char *)StringJoin(path, S("\\*.*\0")).data;

  bool success = true;

  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(find_path, &data);

  if (handle != INVALID_HANDLE_VALUE) {
    do {
      String file_name = StringFromCStr(data.cFileName);

      if (StringEquals(file_name, S(".")) || StringEquals(file_name, S(".."))) continue;

      String file_path = StringJoin(path, S("/"), file_name);

      if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        success |= OS_DeleteEntireDirectory(file_path);
      } else {
        success |= OS_DeleteFile(file_path);
      }

    } while(FindNextFile(handle, &data));
  }

  FindClose(handle);

  success |= OS_DeleteDirectory(path);

  return success;
}

void Win32NormalizePath(String path) {
  u8 *at = path.data;

  for (u64 i = 0; i < path.count; i++) {
    if (*at == '\\') {
      *at = '/';
    }

    at ++;
  }
}

String OS_GetExecutablePath() {
  WCHAR buffer[1024];

  DWORD length = GetModuleFileNameW(NULL, buffer, sizeof(buffer));
  if (length == 0) {
    return {};
  }

  String16 temp = {length, cast(u16 *)buffer};
  String result = StringFromString16(temp);
  Win32NormalizePath(result);

  return result;
}

String OS_GetCurrentDirectory() {
  WCHAR buffer[1024];

  DWORD length = GetCurrentDirectoryW(sizeof(buffer), buffer);
  if (length == 0) {
    return {};
  }

  String16 temp = {length, cast(u16 *)buffer};
  String result = StringFromString16(temp);
  Win32NormalizePath(result);

  return result;
}


#endif // OS_WIN32


#endif // NJA_H
