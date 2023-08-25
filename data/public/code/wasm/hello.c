// Types
#define NULL 0
typedef unsigned char      u8;
typedef unsigned int       u32;
typedef signed int         i32;
typedef signed long long   i64;
typedef double             f64;


// Imports
__attribute__((import_module("env"), import_name("js__output")))
void js__output(char *str, i32 size);

__attribute__((import_module("env"), import_name("js__sin")))
f64 js__sin(f64 x);

// Math
f64 sqrt(f64 x)
{
    return __builtin_sqrt(x);
}

f64 sin(f64 x)
{
    return js__sin(x);
}

// Memory
u32 wasm__heap_size_in_bytes()
{
    return __builtin_wasm_memory_size(0) << 16;
}

extern u32 __heap_base;
void *wasm__heap_pointer()
{
    return (void *)&__heap_base;
}

static u32 offset = 0;

__attribute__((export_name("wasm__push")))
u8 *wasm__push(u32 size)
{
    u8 *result = NULL;

    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if (offset + size < wasm__heap_size_in_bytes())
    {
        result = (u8 *)(wasm__heap_pointer()) + offset;
        offset += size;
    }

    return result;
}

__attribute__((export_name("wasm__pop")))
void wasm__pop(u32 size)
{
    // NOTE(nick): align up to the nearest 16 bytes
    size = (size + 15) & ~15;

    if ((i64)offset - size > 0) {
        offset -= size;
    } else {
        offset = 0;
    }
}

//
// Exported API
//

typedef struct f64_Array f64_Array;
struct f64_Array
{
    u32 count;
    f64 *data;
};

__attribute__((export_name("add")))
i32 add(i32 x, i32 y)
{
    return x + y;
}

__attribute__((export_name("secret_message")))
char *secret_message()
{
    return (char *)"Hello, Sailor!";
}

__attribute__((export_name("ping")))
void ping(char *message, i32 count)
{
    js__output(message, count);
}

__attribute__((export_name("compute_square_roots")))
void compute_square_roots(f64 *array, i32 count)
{
    for (i32 i = 0; i < count; i += 1)
    {
        array[i] = sqrt(array[i]);
    }
}

__attribute__((export_name("push_f64_array")))
f64_Array *push_f64_array(u32 count)
{
    f64_Array *result = (f64_Array *)wasm__push(sizeof(f64_Array));
    result->count = count;
    result->data = (f64 *)wasm__push(sizeof(f64) * count);

    for (i32 i = 0; i < count; i += 1)
    {
        result->data[i] = 0;
    }

    return result;
}
