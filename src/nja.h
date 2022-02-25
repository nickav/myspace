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

#ifndef print
  #include <stdio.h>
  #define print(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#endif

#ifndef print_to_buffer
  #include <stdio.h>
  #define print_to_buffer(data, max_length, format, args) vsnprintf(data, max_length, format, args)
#endif

#ifndef assert
#ifdef DEBUG
  #define assert(expr) do { if (!(expr)) { print("%s %d: assert failed: %s\r\n", __FILE__, __LINE__, #expr); *(volatile int *)0 = 0; } } while (0)
#else
  #define assert(expr)
#endif
#endif

#define cast(type) (type)

#define count_of(array) (sizeof(array) / sizeof((array)[0]))
#define offset_of(Type, member) ((uint64_t) & (((Type *)0)->member))

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
  memory_zero(arena->data, arena->size);
}

void *arena_push(Arena *arena, u64 size) {
  void *result = 0;

  if (arena->offset + size < arena->size) {
    result = arena->data + arena->offset;
    arena->offset += size;
  }

  return result;
}

#define push_struct(arena, Struct)  \
  (Struct *)arena_push(arena, sizeof(Struct))

#define push_array(arena, Struct, count)  \
  (Struct *)arena_push(arena, count * sizeof(Struct))

Arena_Mark arena_get_position(Arena *arena) {
  Arena_Mark result = {};
  result.offset = arena->offset;
  return result;
}

void arena_set_position(Arena *arena, Arena_Mark mark) {
  arena->offset = mark.offset;
}

struct Thread_Context {
  Arena temporary_storage;
};

void *os_thread_get_context();

Arena *thread_get_temporary_arena() {
  Thread_Context *ctx = (Thread_Context *)os_thread_get_context();
  return &ctx->temporary_storage;
}

// @Robustness: support overflowing
// @Robustness: support arbitrary temp storage size
void *talloc(u64 size) {
  Arena *arena = thread_get_temporary_arena();
  return arena_alloc(arena, size);
}

void tpop(u64 size) {
  Arena *arena = thread_get_temporary_arena();
  arena_pop(arena, size);
}

void reset_temporary_storage() {
  Arena *arena = thread_get_temporary_arena();
  arena_reset(arena);
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

String make_string(u8 *data, u64 count) {
  return String{count, data};
}

char *string_to_cstr(Arena *arena, String str) {
  if (!str.count || !str.data) {
    return NULL;
  }

  char *result = push_array(arena, char, str.count + 1); // size for null character
  memory_copy(str.data, result, str.count);
  result[str.count] = 0;
  return result;
}

char *string_to_cstr(String str) {
  Arena *arena = thread_get_temporary_arena();
  return string_to_cstr(arena, str);
}

i64 cstr_length(char *str) {
  char *at = str;

  while (*at != 0) {
    at ++;
  }

  return at - str;
}

String string_from_cstr(char *data) {
  String result = {};
  i64 length = cstr_length(data);

  if (length > 0) {
    assert(length <= I64_MAX);
    result = {(u64)length, (u8 *)data};
  }

  return result;
}

bool string_equals(String a, String b) {
  return a.count == b.count && memory_equals(a.data, b.data, a.count);
}

bool string_starts_with(String str, String prefix) {
  return str.count >= prefix.count && memory_equals(str.data, prefix.data, prefix.count);
}

bool string_ends_with(String str, String postfix) {
  return str.count >= postfix.count && memory_equals(str.data + (str.count - postfix.count), postfix.data, postfix.count);
}

String string_slice(String str, i64 start_index, i64 end_index) {
  assert(start_index >= 0 && start_index <= str.count);
  assert(end_index >= 0 && end_index <= str.count);
  return make_string(str.data + start_index, cast(u64)(end_index - start_index));
}

String string_slice(String str, i64 start_index) {
  return string_slice(str, start_index, str.count);
}

String string_range(u8 *at, u8 *end) {
  assert(end >= at);
  return make_string(at, (end - at));
}

i64 string_index(String str, String search, i64 start_index = 0) {
  for (i64 i = start_index; i < str.count; i += 1) {
    if (memory_equals(str.data + i, search.data, search.count)) {
      return i;
    }
  }

  return -1;
}

bool string_contains(String str, String search) {
  return string_index(str, search) >= 0;
}

String string_copy(Arena *arena, String other) {
  u8 *data = push_array(arena, u8, other.count);
  String copy = make_string(data, other.count);
  memory_copy(other.data, copy.data, other.count);
  return copy;
}

String string_join(Arena *arena, String a, String b) {
  u8 *data = push_array(arena, u8, a.count + b.count);

  memory_copy(a.data, data + 0,       a.count);
  memory_copy(b.data, data + a.count, b.count);

  return make_string(data, a.count + b.count);
}

String string_join(String a, String b) {
  Arena *arena = thread_get_temporary_arena();
  return string_join(arena, a, b);
}

String string_join(Arena *arena, String a, String b, String c) {
  u8 *data = push_array(arena, u8, a.count + b.count + c.count);

  memory_copy(a.data, data + 0,                 a.count);
  memory_copy(b.data, data + a.count,           b.count);
  memory_copy(c.data, data + a.count + b.count, c.count);

  return make_string(data, a.count + b.count + c.count);
}

String string_join(String a, String b, String c) {
  Arena *arena = thread_get_temporary_arena();
  return string_join(arena, a, b, c);
}

void string_advance(String *str, u64 amount) {
  assert(str->count >= amount);

  str->count -= amount;
  str->data  += amount;
}

bool char_is_whitespace(char ch) {
  return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r';
}

String string_trim_whitespace(String str) {
  while (str.count > 0 && char_is_whitespace(str.data[0])) {
    str.data ++;
    str.count --;
  }

  while (str.count > 0 && char_is_whitespace(str.data[str.count - 1])) {
    str.count --;
  }

  return str;
}

#include <stdarg.h>

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
      result = make_string(buffer, actual_size);
    } else {
      arena_pop(arena, buffer_size);
      u8 *fixed_buffer = push_array(arena, u8, actual_size + 1);
      u64 final_size = print_to_buffer((char *)fixed_buffer, actual_size + 1, format, args2);
      result = make_string(fixed_buffer, final_size);
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

char *cstr_print(Arena *arena, const char *format, ...) {
  String result = {};

  va_list args;
  va_start(args, format);
  result = string_printv(arena, format, args);
  va_end(args);

  char *null_terminator = (char *)arena_push(arena, 1);
  *null_terminator = 0;

  return (char *)result.data;
}

#define sprint(...) string_print(thread_get_temporary_arena(), __VA_ARGS__)
#define cprint(...) cstr_print(thread_get_temporary_arena(), __VA_ARGS__)

// NOTE(nick): The path_* functions assume that we are working with a normalized (unix-like) path string.
// All paths should be normalized at the OS interface level, so we can make that assumption here.

String path_join(String path1, String path2) {
  return sprint("%.*s/%.*s", path1.count, path1.data, path2.count, path2.data);
}

String path_join(String path1, String path2, String path3) {
  return sprint("%.*s/%.*s/%.*s", path1.count, path1.data, path2.count, path2.data, path3.count, path3.data);
}

String path_dirname(String filename) {
  for (i32 i = filename.count - 1; i >= 0; i--) {
    if (filename.data[i] == '/') {
      return string_slice(filename, 0, i);
    }
  }

  return sprint("./");
}

String path_filename(String path) {
  for (i32 i = path.count - 1; i >= 0; i--) {
    char ch = path.data[i];
    if (ch == '/') {
      return string_slice(path, i + 1, path.count);
    }
  }

  return path;
}

String path_strip_extension(String path) {
  for (i32 i = path.count - 1; i >= 0; i--) {
    char ch = path.data[i];
    if (ch == '.') {
      return string_slice(path, 0, i);
    }
  }

  return path;
}

String path_get_extension(String path) {
  for (i32 i = path.count - 1; i >= 0; i--) {
    char ch = path.data[i];
    if (ch == '.') {
      return string_slice(path, i, path.count);
    }
  }

  return {};
}

String to_string(bool x) { if (x) return S("true"); return S("false"); }
String to_string(char x) { return sprint("%c", x); }
String to_string(char *x) { return string_from_cstr(x); }
String to_string(i8 x)  { return sprint("%d", x); }
String to_string(u8 x)  { return sprint("%d", x); }
String to_string(i16 x) { return sprint("%d", x); }
String to_string(u16 x) { return sprint("%d", x); }
String to_string(i32 x) { return sprint("%d", x); }
String to_string(u32 x) { return sprint("%d", x); }
String to_string(i64 x) { return sprint("%d", x); }
String to_string(u64 x) { return sprint("%llu", x); }
String to_string(f32 x) { return sprint("%.2f", x); }
String to_string(f64 x) { return sprint("%.4f", x); }
String to_string(String x) { return x; }

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
      if (string_starts_with(string_range(at, str_end), split)) {
        String slice = string_range(word_first, at);
        string_list_push(arena, &result, slice);

        at += split.count - 1;
        word_first = at + 1;
        continue;
      }
    }
  }

  String slice = string_range(word_first, str_end);
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
      memory_copy(join.data, at, join.count);
      at += join.count;
    }

    memory_copy(it->str.data, at, it->str.count);
    at += it->str.count;
    is_mid = it->next != NULL;
  }

  return make_string(data, size);
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

String_Decode string_decode_utf8(u8 *str, u32 capacity) {
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

String_Decode string_decode_utf16(u16 *str, u32 capacity) {
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

String32 string32_from_string(Arena *arena, String str) {
  u32 *memory = push_array(arena, u32, str.count);;

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

  arena_pop(arena, unused_count * sizeof(u32));

  String32 result = {string_count, memory};
  return result;
}

String string_from_string32(Arena *arena, String32 str) {
  u8 *memory = push_array(arena, u8, str.count * 4);

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

  arena_pop(arena, unused_count);

  String result = {string_count, memory};
  return result;
}

String16 string16_from_string(Arena *arena, String str) {
  u16 *memory = push_array(arena, u16, str.count + 1);

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

  arena_pop(arena, unused_count * sizeof(u16));

  String16 result = {string_count, memory};
  return result;
}

String string_from_string16(Arena *arena, String16 str) {
  String result = {};
  result.data = push_array(arena, u8, str.count * 2);

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

  u64 unused_size = (result.count - str.count);
  arena_pop(arena, unused_size);

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

inline i32 Min(i32 a, i32 b) { return MIN(a, b); }
inline u32 Min(u32 a, u32 b) { return MIN(a, b); }
inline u64 Min(u64 a, u64 b) { return MIN(a, b); }
inline f32 Min(f32 a, f32 b) { return MIN(a, b); }
inline f64 Min(f64 a, f64 b) { return MIN(a, b); }

inline i32 Max(i32 a, i32 b) { return MAX(a, b); }
inline u32 Max(u32 a, u32 b) { return MAX(a, b); }
inline u64 Max(u64 a, u64 b) { return MAX(a, b); }
inline f32 Max(f32 a, f32 b) { return MAX(a, b); }
inline f64 Max(f64 a, f64 b) { return MAX(a, b); }

inline i32 Clamp(i32 value, i32 lower, i32 upper) { return MAX(MIN(value, upper), lower); }
inline u32 Clamp(u32 value, u32 lower, u32 upper) { return MAX(MIN(value, upper), lower); }
inline u64 Clamp(u64 value, u64 lower, u64 upper) { return MAX(MIN(value, upper), lower); }
inline f32 Clamp(f32 value, f32 lower, f32 upper) { return MAX(MIN(value, upper), lower); }
inline f64 Clamp(f64 value, f64 lower, f64 upper) { return MAX(MIN(value, upper), lower); }

#define ClampTop Max
#define ClampBot Min

inline f32 Lerp(f32 a, f32 b, f32 t) {
  return (1 - t) * a + b * t;
}

inline f32 Unlerp(f32 a, f32 b, f32 v) {
  return (v - a) / (b - a);
}

inline f32 Square(f32 x) {
  return x * x;
}

//
// Math
//

//
// OS
//

struct File {
  void *handle;
  bool has_errors;
  u64 offset;
};

struct File_Info {
  u64 date;
  u64 size;
  String name;
  bool is_directory;
};

enum File_Mode {
  FILE_MODE_READ   = 0x1,
  FILE_MODE_WRITE  = 0x2,
  FILE_MODE_APPEND = 0x4,
};

#define THREAD_PROC(name) u32 name(void *data)
typedef THREAD_PROC(Thread_Proc);

struct Thread {
  u32 id;
  void *handle;
};

struct Thread_Params {
  Thread_Proc *proc;
  void *data;
};

#if OS_WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#include <windows.h>

static LARGE_INTEGER win32_perf_frequency;
static LARGE_INTEGER win32_perf_counter;

static DWORD win32_thread_context_index = 0;

static bool did_init_os = false;

void *os_alloc(u64 size);
void os_free(void *ptr);
void os_thread_set_context(void *ptr);

void init_thread_context(u64 temporary_storage_size) {
  Thread_Context *ctx = (Thread_Context *)os_alloc(sizeof(Thread_Context));

  u8 *data = (u8 *)os_alloc(temporary_storage_size);
  arena_init(&ctx->temporary_storage, data, temporary_storage_size);

  os_thread_set_context(ctx);
}

void free_thread_context() {
  Thread_Context *ctx = (Thread_Context *)os_thread_get_context();

  os_free(&ctx->temporary_storage.data);
  os_free(ctx);
}

void os_init() {
  if (did_init_os) return;

  AttachConsole(ATTACH_PARENT_PROCESS);

  QueryPerformanceFrequency(&win32_perf_frequency);
  QueryPerformanceCounter(&win32_perf_counter);

  win32_thread_context_index = TlsAlloc();

  init_thread_context(megabytes(32));

  did_init_os = true;
}

void os_thread_set_context(void *ptr) {
  TlsSetValue(win32_thread_context_index, ptr);
}

void *os_thread_get_context() {
  void *result = TlsGetValue(win32_thread_context_index);
  return result;
}

f64 os_time_in_miliseconds() {
  assert(win32_perf_frequency.QuadPart);

  LARGE_INTEGER current_time;
  QueryPerformanceCounter(&current_time);

  LARGE_INTEGER elapsed;
  elapsed.QuadPart = current_time.QuadPart - win32_perf_counter.QuadPart;
  return (f64)(elapsed.QuadPart * 1000) / win32_perf_frequency.QuadPart;
}

void os_sleep(f64 miliseconds) {
  LARGE_INTEGER ft;
  ft.QuadPart = -(10 * (__int64)(miliseconds * 1000));

  HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
  SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
  WaitForSingleObject(timer, INFINITE);
  CloseHandle(timer);
}

void os_set_high_process_priority(bool enable) {
  if (enable) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  } else {
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
  }
}

void *os_memory_reserve(u64 size) {
  return VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
}

void os_memory_commit(void *ptr, u64 size) {
  VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void os_memory_decommit(void *ptr, u64 size) {
  VirtualFree(ptr, size, MEM_DECOMMIT);
}

void os_memory_release(void *ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}

void *os_alloc(u64 size) {
  // Memory allocated by this function is automatically initialized to zero.
  return VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void os_free(void *ptr) {
  if (ptr) {
    VirtualFree(ptr, 0, MEM_RELEASE);
    ptr = 0;
  }
}

String os_read_entire_file(Arena *arena, String path) {
  String result = {};

  // @Cleanup: do we always want to null-terminate String16 s?
  String16 str = string16_from_string(arena, path);
  HANDLE handle = CreateFileW(cast(WCHAR *)str.data, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  if (handle == INVALID_HANDLE_VALUE) {
    print("[file] Error reading entire file: (error code: %d) for %.*s\n", GetLastError(), LIT(path));
    return result;
  }

  DWORD hi_size = 0;
  DWORD lo_size = GetFileSize(handle, &hi_size);

  u64 size = ((cast(u64)hi_size) << 32) | cast(u64)lo_size;

  result.data = cast(u8 *)arena_alloc(arena, size + 1); // space for null character.
  result.data[size] = 0;
  result.count = size;

  DWORD bytes_read = 0;
  BOOL success = ReadFile(handle, (void *)result.data, size, &bytes_read, 0);

  if (!success) {
    print("[file] Failed to read entire file: %.*s\n", LIT(path));
  } else if (bytes_read != size) {
    // @Robustness: should this keep reading until it reads everything?
    print("[file] Warning byte read mismatch, expected %llu but got %lu for file: %.*s\n", size, bytes_read, LIT(path));
  }

  CloseHandle(handle);

  return result;
}

String os_read_entire_file(String path) {
  Arena *arena = thread_get_temporary_arena();
  return os_read_entire_file(arena, path);
}

bool os_write_entire_file(String path, String contents) {
  Arena *arena = thread_get_temporary_arena();

  // :ScratchMemory
  String16 str = string16_from_string(arena, path);
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
    print("[file] Warning byte read mismatch, expected %llu but got %lu for file: %.*s\n", contents.count, bytes_written, LIT(path));
    return false;
  }

  CloseHandle(handle);

  return true;
}

bool os_delete_file(String path) {
  Arena *arena = thread_get_temporary_arena();

  auto restore_point = arena_get_position(arena);
  String16 str = string16_from_string(arena, path);
  BOOL success = DeleteFileW(cast(WCHAR *)str.data);
  arena_set_position(arena, restore_point);

  return success;
}

bool os_make_directory(String path) {
  Arena *arena = thread_get_temporary_arena();

  auto restore_point = arena_get_position(arena);
  String16 str = string16_from_string(arena, path);
  BOOL success = CreateDirectoryW(cast(WCHAR *)str.data, NULL);
  arena_set_position(arena, restore_point);

  return success;
}

bool os_delete_directory(String path) {
  Arena *arena = thread_get_temporary_arena();

  auto restore_point = arena_get_position(arena);
  String16 str = string16_from_string(arena, path);
  BOOL success = RemoveDirectoryW(cast(WCHAR *)str.data);
  arena_set_position(arena, restore_point);

  return success;
}

bool os_delete_entire_directory(String path) {
  Arena *arena = thread_get_temporary_arena();

  auto restore_point = arena_get_position(arena);
  char *find_path = cstr_print(arena, "%.*s\\*.*", LIT(path));

  bool success = true;

  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(find_path, &data);

  if (handle != INVALID_HANDLE_VALUE) {
    do {
      String file_name = string_from_cstr(data.cFileName);

      if (string_equals(file_name, S(".")) || string_equals(file_name, S(".."))) continue;

      String file_path = string_join(path, S("/"), file_name);

      if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        success |= os_delete_entire_directory(file_path);
      } else {
        success |= os_delete_file(file_path);
      }

    } while(FindNextFile(handle, &data));
  }

  FindClose(handle);

  success |= os_delete_directory(path);

  arena_set_position(arena, restore_point);

  return success;
}

struct Scan_Result {
  File_Info *files;
  u32 count;
};

Scan_Result os_scan_directory(Arena *arena, String path) {
  Scan_Result result = {};

  // :ScratchMemory
  char *find_path = cstr_print(arena, "%.*s\\*.*", LIT(path));

  // @Speed: we could avoid doing this twice by assuming a small-ish number to start and only re-scanning if we're wrong
  // but then that wastes a bunch of memory
  // and is this block actually slow?
  u32 count = 0; 
  {
    WIN32_FIND_DATA data;
    HANDLE handle = FindFirstFile(find_path, &data);

    if (handle != INVALID_HANDLE_VALUE) {
      do {
        count += 1;
      } while (FindNextFile(handle, &data));
    }

    FindClose(handle);
  }

  result.files = push_array(arena, File_Info, count);

  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(find_path, &data);

  if (handle != INVALID_HANDLE_VALUE) {
    do {
      String name = string_from_cstr(data.cFileName);
      // Ignore . and .. "directories"
      if (string_equals(name, S(".")) || string_equals(name, S(".."))) continue;

      File_Info *info = &result.files[result.count];
      result.count += 1;

      *info = {};
      info->name         = string_copy(arena, name);
      info->date         = ((u64)data.ftLastWriteTime.dwHighDateTime << (u64)32) | (u64)data.ftLastWriteTime.dwLowDateTime;
      info->size         = ((u64)data.nFileSizeHigh << (u64)32) | (u64)data.nFileSizeLow;
      info->is_directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

    } while (FindNextFile(handle, &data));
  }

  FindClose(handle);

  return result;
}

struct File_Lister {
  char *find_path;

  HANDLE handle;
  WIN32_FIND_DATA data;
};

File_Lister os_file_list_begin(Arena *arena, String path) {
  File_Lister result = {};

  char *find_path = cstr_print(arena, "%.*s\\*.*", LIT(path));
  result.find_path = find_path;
  result.handle = 0;

  return result;
}

bool os_file_list_next(File_Lister *iter, File_Info *info) {
  bool should_continue = true;

  if (!iter->handle) {
    iter->handle = FindFirstFile(iter->find_path, &iter->data);
  } else {
    should_continue = FindNextFile(iter->handle, &iter->data);
  }

  if (iter->handle != INVALID_HANDLE_VALUE) {
    String name = string_from_cstr(iter->data.cFileName);

    // Ignore . and .. "directories"
    while (should_continue && (string_equals(name, S(".")) || string_equals(name, S("..")))) {
      should_continue = FindNextFile(iter->handle, &iter->data);
      name = string_from_cstr(iter->data.cFileName);
    }

    *info = {};
    info->name         = name;
    info->date         = ((u64)iter->data.ftLastWriteTime.dwHighDateTime << (u64)32) | (u64)iter->data.ftLastWriteTime.dwLowDateTime;
    info->size         = ((u64)iter->data.nFileSizeHigh << (u64)32) | (u64)iter->data.nFileSizeLow;
    info->is_directory = iter->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  } else {
    should_continue = false;
  }

  return should_continue;
}

void os_file_list_end(File_Lister *iter) {
  if (iter->handle) {
    FindClose(iter->handle);
    iter->handle = 0;
  }
}

File_Info os_get_file_info(Arena *arena, String path) {
  File_Info result = {};

  // :ScratchMemory
  char *cpath = string_to_cstr(arena, path);

  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExA(cpath, GetFileExInfoStandard, &data)) {
    result.name         = path_filename(path);
    result.date         = ((u64)data.ftLastWriteTime.dwHighDateTime << (u64)32) | (u64)data.ftLastWriteTime.dwLowDateTime;
    result.size         = ((u64)data.nFileSizeHigh << (u64)32) | (u64)data.nFileSizeLow;
    result.is_directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  }

  return result;
}

DWORD WINAPI win32_thread_proc(LPVOID lpParameter) {
  Thread_Params *params = (Thread_Params *)lpParameter;

  init_thread_context(megabytes(16));
  assert(params->proc);
  u32 result = params->proc(params->data);
  free_thread_context();

  os_free(params);

  return result;
}

Thread os_create_thread(u64 stack_size, Thread_Proc *proc, void *data) {
  Thread_Params *params = (Thread_Params *)os_alloc(sizeof(Thread_Params));
  params->proc = proc;
  params->data = data;

  DWORD thread_id;
  HANDLE handle = CreateThread(0, stack_size, win32_thread_proc, params, 0, &thread_id);

  Thread result = {};
  result.handle = handle;
  result.id = thread_id;
  return result;
}

void os_detatch_thread(Thread thread) {
  HANDLE handle = (HANDLE)thread.handle;
  CloseHandle(handle);
}

u32 os_await_thread(Thread thread) {
  HANDLE handle = (HANDLE)thread.handle;
  WaitForSingleObject(handle, INFINITE);
  DWORD result;
  GetExitCodeThread(handle, &result);
  // @MemoryLeak: free Thread_Params
  return result;
}

void win32_normalize_path(String path) {
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

  Arena *arena = thread_get_temporary_arena();
  String result = string_from_string16(arena, temp);
  win32_normalize_path(result);

  return result;
}

String os_get_current_directory() {
  WCHAR buffer[1024];

  DWORD length = GetCurrentDirectoryW(sizeof(buffer), buffer);
  if (length == 0) {
    return {};
  }

  String16 temp = {length, cast(u16 *)buffer};

  Arena *arena = thread_get_temporary_arena();
  String result = string_from_string16(arena, temp);
  win32_normalize_path(result);

  return result;
}


#endif // OS_WIN32

String os_get_executable_directory() {
  String result = os_get_executable_path();
  return path_dirname(result);
}

//
// Array
//

#if 0
#define Array_Foreach(item, array)                                             \
  for (i64 index = 0;                                                          \
       index < cast(i64)(array).count ? (item) = (array).data[index], true : false; \
       index++)

#define Array_Foreach_Pointer(item, array)                                     \
  for (i64 index = 0;                                                          \
       index < cast(i64)(array).count ? (item) = &(array).data[index], true : false; \
       index++)

#define Array_Foreach_Reverse(item, array)                                     \
  for (i64 index = cast(i64)(array).count - 1;                             \
       index >= 0 ? (item) = (array).data[index], true : false;                \
       index--)

#define Array_Foreach_Pointer_Reverse(item, array)                             \
  for (i64 index = cast(i64)(array).count - 1;                             \
       index >= 0 ? (item) = &(array).data[index], true : false;               \
       index--)

#define For(array) for (auto it : array)

#define Forv(array, it) for (auto it : array)

#define Forp(array)   \
  auto CONCAT(__array, __LINE__) = (array);   \
  for (auto it = CONCAT(__array, __LINE__).begin_ptr(); it < CONCAT(__array, __LINE__).end_ptr(); it++)

#define For_Index(array) for (i64 index = 0; index < cast(i64)(array).count; index++)

#define Fori For_Index

#define For_Index_Reverse(array)                                               \
  for (i64 index = cast(i64)(array).count - 1; index >= 0; index--)

#define Array_Remove_Current_Item_Unordered(array)  \
  do { (array).remove_unordered(index); index --; } while(0)

template <typename T> struct Array;

template <typename T>
struct Array_Iterator {
  const Array<T> *array;
  u64 index;

  Array_Iterator(const Array<T> *_array, u64 _index) : array(_array), index(_index) {};

  bool operator != (const Array_Iterator<T> &other) const {
    return index != other.index;
  }

  T &operator *() const {
    return array->data[index];
  }

  const Array_Iterator &operator++ () {
    index++;
    return *this;
  }
};
#endif

struct Sized_Pointer {
  u8  *data;
  u16 stride;

  u8 &operator[](u64 i) {
    assert(stride > 0);
    return data[i * stride];
  }

  void operator=(const void* _data) {
    data = (u8 *)_data;
  }
};

struct Raw_Array {
  u64 capacity = 0;
  u64 count = 0;

  Sized_Pointer data = {NULL, 0};

/*
  T &operator[](u64 i) {
    assert(i >= 0 && i < capacity);
    return data[i];
  }
*/
};

void array_reset(Raw_Array &it) {
  it.count = 0;
}

bool array_at_capacity(Raw_Array &it) {
  return it.capacity <= it.count;
}

void *array_peek(Raw_Array &it) {
  return it.count > 0 ? &it.data[it.count - 1] : NULL;
}

void *array_pop(Raw_Array &it) {
  void *removed = array_peek(it);
  if (removed) it.count --;
  return removed;
}

void array_remove_unordered(Raw_Array &it, u64 index) {
  assert(index >= 0 && index < it.count);
  memory_copy(&it.data[it.count - 1], &it.data[index], it.data.stride);
  it.count--;
}

void array_remove_while_keeping_order(Raw_Array &it, u64 index, u64 num_to_remove = 1) {
  assert(index >= 0 && index < it.count);
  assert(num_to_remove > 0 && index + num_to_remove <= it.count);

  // @Speed: this could be replaced with memory_move, the problem is overlapping memory
  for (u64 i = index + num_to_remove; i < it.count; i++) {
    memory_copy(&it.data[i], &it.data[i - num_to_remove], it.data.stride);
  }

  it.count -= num_to_remove;
}

void array_resize(Raw_Array &it, u64 next_capacity) {
  if (it.data.data && it.capacity == next_capacity) return;

  u64 prev_size = it.capacity * it.data.stride;
  u64 next_size = next_capacity * it.data.stride;

  // @Speed: realloc
  //it.data = realloc(it.data.data, next_size, prev_size);
  // @Cleanup
  if (it.data.data) os_free(it.data.data);
  it.data = (u8 *)os_alloc(next_size * it.data.stride);
  assert(it.data.data);
  it.capacity = next_capacity;
}

void array_init(Raw_Array &it, u64 initial_capacity = 16) {
  it.count = 0;
  it.data = NULL;
  array_resize(it, initial_capacity);
}

void array_free(Raw_Array &it) {
  // @Cleanup
  if (it.data.data) {
    os_free(it.data.data);
    it.data.data = NULL;
    it.capacity = 0;
    it.count = 0;
  }
}

void array_reserve(Raw_Array &it, u64 minimum_count) {
  if (it.capacity < minimum_count) {
    array_resize(it, minimum_count);
  }
}

void *array_push(Raw_Array &it) {
  if (it.count >= it.capacity) {
    array_resize(it, it.capacity ? it.capacity * 2 : 16);
  }

  memory_set(&it.data[it.count], 0, it.data.stride);
  return &it.data[it.count ++];
}

template <typename T>
struct Array : Raw_Array {
  Array() {
    data.stride = sizeof(T);
  }

  Array(u64 _capacity, u64 _count) {
    capacity = _capacity;
    count = _count;
    data.stride = sizeof(T);
  }

  Array(u64 _capacity, u64 _count, void *_data) {
    capacity = _capacity;
    count = _count;
    data.data = (u8 *)_data;
    data.stride = sizeof(T);
  }

  T &operator[](u64 i) {
    assert(i >= 0 && i < capacity);
    assert(data.stride == sizeof(T));

    return (T &)data[i];
  }
};

template <typename T> T *array_peek(Array<T> &it) { return (T *)array_peek((Raw_Array &)(it)); }
template <typename T> T *array_pop(Array<T> &it) { return (T *)array_pop((Raw_Array &)(it)); }
template <typename T> T *array_push(Array<T> &it) { return (T *)array_push((Raw_Array &)(it)); }
template <typename T> T *array_push(Array<T> &it, T item) {
  if (it.count >= it.capacity) array_resize(it, it.capacity ? it.capacity * 2 : 16);

  memory_copy(&item, &it.data[it.count], it.data.stride);
  return (T *)&it.data[it.count ++];
}

#if 0
Array_Iterator<T> begin() const {
  return Array_Iterator<T>(this, 0);
}

Array_Iterator<T> end() const {
  return Array_Iterator<T>(this, count);
}

T *begin_ptr() {
  return data ? &data[0] : NULL;
}

T *end_ptr() {
  return data ? &data[count] : NULL;
}
#endif

#if 0
template <typename T> struct Static_Array;

template <typename T>
struct Static_Array_Iterator {
  const Static_Array<T> *array;
  u64 index;

  Static_Array_Iterator(const Static_Array<T> *_array, u64 _index) : array(_array), index(_index) {};

  bool operator != (const Static_Array_Iterator<T> &other) const {
    return index != other.index;
  }

  T &operator *() const {
    return array->data[index];
  }

  const Static_Array_Iterator &operator++ () {
    index++;
    return *this;
  }
};

template <typename T>
struct Static_Array {
  u64 count;
  T *data;

  T &operator[](u64 i) {
    assert(i >= 0 && i < count);
    return data[i];
  }

  Static_Array_Iterator<T> begin() const {
    return Static_Array_Iterator<T>(this, 0);
  }

  Static_Array_Iterator<T> end() const {
    return Static_Array_Iterator<T>(this, count);
  }

  T *begin_ptr() {
    return data ? &data[0] : NULL;
  }

  T *end_ptr() {
    return data ? &data[count] : NULL;
  }
};
#endif


#if 0
//
// Hash Table
//

#define For_Table(table)                                    \
  for (auto it = (table).begin(); it < (table).end(); it++) \
    if (it->hash < FIRST_VALID_HASH) continue; else

const int NEVER_OCCUPIED_HASH = 0;
const int REMOVED_HASH = 1;
const int FIRST_VALID_HASH = 2;

// NOTE(nick): djb2 algorithm
u32 compute_hash(char *str) {
  u32 hash = 5381;
  i32 c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}

u32 compute_hash(u32 value) {
  value ^= (value >> 20) ^ (value >> 12);
  return value ^ (value >> 7) ^ (value >> 4);
}

template <typename K>
bool hash_compare_keys(const K &a, const K &b) {
  return a == b;
}

template <typename K, typename V> struct Hash_Table {
  struct Entry {
    u32 hash;
    K key;
    V value;
  };

  Allocator *allocator = context.allocator;
  u32 capacity = 0;
  u32 count = 0;
  u32 slots_filled = 0;
  Entry *data = 0;

  void init(u32 table_size) {
    capacity = next_power_of_two(table_size);
    count = 0;
    slots_filled = 0;

    assert((capacity & (capacity - 1)) == 0); // Must be a power of two!

    data = (Entry *)Alloc(capacity * sizeof(Entry), allocator);
    reset();
  }

  void free() {
    if (data) {
      Free(data, allocator);
      data = NULL;
      capacity = 0;
      count = 0;
      slots_filled = 0;
    }
  }

  void expand() {
    u32 next_capacity = capacity ? capacity * 2 : 32;

    #if 0
    u32 num_removed = slots_filled - count;
    if (capacity > 0 && (capacity - num_removed) * 2 <= capacity) {
      next_capacity = capacity;
    }
    #endif

    assert((next_capacity & (next_capacity - 1)) == 0); // Must be a power of two!

    Entry *old_data = data;
    u32 old_capacity = capacity;

    init(next_capacity);

    // count and slots_filled will be incremented by add.
    count        = 0;
    slots_filled = 0;

    for (u32 i = 0; i < old_capacity; i++) {
      Entry *entry = &old_data[i];

      // Note that if we removed some stuff we will over-allocate the new table.
      // Maybe we should count the number of clobbers and subtract that? I dunno.
      if (entry->hash >= FIRST_VALID_HASH) add(entry->key, entry->value);
    }

    Free(old_data, allocator);
  }

  // Sets the key-value pair, replacing it if it already exists.
  V *set(K key, V value) {
    auto result = find_pointer(key);
    if (result) {
      *result = value;
      return result;
    } else {
      return add(key, value);
    }
  }

  // Adds the given key-value pair to the table, returns a pointer to the inserted item.
  V *add(K key, V value) {
    // The + 1 is here to handle the weird case where the table size is 1 and you add the first item
    // slots_filled / capacity >= 7 / 10 ...therefore:
    // slots_filled * 10 >= capacity * 7
    if ((slots_filled + 1) * 10 >= capacity * 7) expand();
    assert(slots_filled <= capacity);

    u32 hash = compute_hash(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    u32 index = hash & (capacity - 1);

    while (data[index].hash) {
      index = (index + 1) & (capacity - 1);
    }

    count ++;
    slots_filled ++;
    data[index] = {hash, key, value};

    return &data[index].value;
  }

  V *find_pointer(K key) {
    if (!data) return NULL; // @Incomplete: do we want this extra branch hit here?

    assert(data); // Must be initialized!

    u32 hash = compute_hash(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    u32 index = hash & (capacity - 1);

    while (data[index].hash) {
      auto entry = &data[index];
      if (entry->hash == hash && hash_compare_keys(entry->key, key)) {
        return &entry->value;
      }

      index = (index + 1) & (capacity - 1);
    }

    return NULL;
  }

  FORCE_INLINE V find(K key) {
    V *result = find_pointer(key);
    if (result) return *result;
    return {};
  }

  bool remove(K key) {
    assert(data); // Must be initialized!

    u32 hash = compute_hash(key);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    u32 index = hash & (capacity - 1);

    while (data[index].hash) {
      if (data[index].hash == hash && hash_compare_keys(data[index].key, key)) {
        data[index].hash = REMOVED_HASH; // No valid entry will ever hash to REMOVED_HASH.
        count --;
        return true;
      }

      index = (index + 1) & (capacity - 1);
    }

    return false;
  }

  void reset() {
    count = 0;
    slots_filled = 0;

    if (data) {
      for (u32 i = 0; i < capacity; i++) { data[i].hash = 0; }
    }
  }

  Entry *begin() { return data ? &data[0] : NULL; }

  Entry *end() { return data ? &data[capacity] : NULL; }
};

#endif


#endif // NJA_H
