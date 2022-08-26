#ifndef NA_HOTLOADER_H
#define NA_HOTLOADER_H

#if 0
// Usage:

#define NA_HOTLOADER_IMPLEMENTATION
#include "na_hotloader.h"

static Hotloader hot;

HOTLOADER_CALLBACK(my_hotloader_callback) {
        String relative_name = change.relative_name;
        print("[hotloader] Changed file: %S\n", relative_name);
}

int main() {
        hotloader_init(&hot, S("C:/"));
        hotloader_register_callback(&hot, my_hotloader_callback);

        // NOTE(nick): main loop
        while (1)
        {
                hotloader_poll_events(&hot);
                os_sleep(1000);
        }
}

#endif

//------------------------------------------------------------------------------
// Header ----------------------------------------------------------------------
typedef u32 Asset_Change_Type;
enum {
    AssetChange_NULL = 0,

    AssetChange_Added,
    AssetChange_Removed,
    AssetChange_Modified,
    AssetChange_Renamed,

    ASSET_CHANGE_COUNT,
};

struct Asset_Change {
    Asset_Change_Type type;

    String relative_name;
    String prev_relative_name; // Only set when type is AssetChange_Renamed

    Asset_Change *next;
};

struct Hotloader;

#define HOTLOADER_CALLBACK(name) void name(Hotloader *hot, Asset_Change change)
typedef HOTLOADER_CALLBACK(Hotloader_Callback);

bool hotloader_init(Hotloader *it, String path);
void hotloader_register_callback(Hotloader *it, Hotloader_Callback callback);
Asset_Change *hotloader_poll_events(Hotloader *it);
void hotloader_free(Hotloader *it);

#endif // NA_HOTLOADER_H


//------------------------------------------------------------------------------
// Implementation --------------------------------------------------------------
#ifdef NA_HOTLOADER_IMPLEMENTATION

#if OS_WINDOWS

struct Hotloader {
    String path;
    u8 path_storage[1024];

    Hotloader_Callback *callback;

    HANDLE file;
    DWORD flags;
    OVERLAPPED overlapped;

    u8 buffer[1024];
};

bool hotloader_init(Hotloader *it, String path) {
    DWORD flags = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;

    HANDLE file = CreateFile(
        string_to_cstr(path),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    it->path  = string_write(path, it->path_storage, sizeof(it->path_storage));
    it->file  = file;
    it->flags = flags;
    it->overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);

    BOOL success = ReadDirectoryChangesW(
        it->file, it->buffer, count_of(it->buffer), TRUE, it->flags, NULL, &it->overlapped, NULL);

    return success;
}

void hotloader_register_callback(Hotloader *it, Hotloader_Callback *callback) {
    it->callback = callback;
}

// NOTE(nick): The same as:
// string_from_string16(arena, make_string16(buffer, buffer_count));
na_internal String win32_string_from_wchar(Arena *arena, WCHAR *buffer, DWORD buffer_count)
{
    String result = {};

    int count = WideCharToMultiByte(CP_UTF8, 0, buffer, buffer_count, NULL, 0, NULL, NULL);
    if (count)
    {
        char *data = cast(char *)arena_push(arena, count);
        if (WideCharToMultiByte(CP_UTF8, 0, buffer, buffer_count, data, count, NULL, NULL))
        {
            result.count = count;
            result.data  = (u8 *)data;
        }
        else
        {
            arena_pop(arena, count);
        }
    }

    return result;
}

Asset_Change *hotloader_poll_events(Hotloader *it)
{
    Asset_Change *result = NULL;
    Asset_Change *last_change = NULL;

    DWORD status = WaitForSingleObject(it->overlapped.hEvent, 0);
    if (status == WAIT_OBJECT_0)
    {
        DWORD bytes_transferred;
        GetOverlappedResult(it->file, &it->overlapped, &bytes_transferred, FALSE);

        FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)it->buffer;

        // Get all events
        for (;;) {
            DWORD count = event->FileNameLength / sizeof(wchar_t);
            String name = win32_string_from_wchar(temp_arena(), event->FileName, count);
            String prev_name = {};
            Asset_Change_Type type = AssetChange_NULL;

            win32_normalize_path(name);

            switch (event->Action) {
                case FILE_ACTION_ADDED:    { type = AssetChange_Added;    } break;
                case FILE_ACTION_REMOVED:  { type = AssetChange_Removed;  } break;
                case FILE_ACTION_MODIFIED: { type = AssetChange_Modified; } break;

                case FILE_ACTION_RENAMED_OLD_NAME: {
                    type = AssetChange_Renamed;
                    FILE_NOTIFY_INFORMATION *current_event = event;

                    prev_name = name;
                    name = {};

                    if (event->NextEntryOffset) {
                        FILE_NOTIFY_INFORMATION *next_event = (FILE_NOTIFY_INFORMATION *)(it->buffer + event->NextEntryOffset);

                        if (next_event->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                            DWORD count = event->FileNameLength / sizeof(wchar_t);
                            name = win32_string_from_wchar(temp_arena(), next_event->FileName, count);

                            win32_normalize_path(name);

                            // NOTE(nick): Consume FILE_ACTION_RENAMED_NEW_NAME
                            *((u8 **)&event) += event->NextEntryOffset;
                        }
                    }
                } break;

                case FILE_ACTION_RENAMED_NEW_NAME: {
                    // We assume that FILE_ACTION_RENAMED_OLD_NAME is always followed by FILE_ACTION_RENAMED_NEW_NAME
                    // It seems like that's not always true. Not sure if it matters
                } break;
            }

            if (type != AssetChange_NULL)
            {
                Asset_Change *change = push_array_zero(temp_arena(), Asset_Change, 1);
                change->type = type;
                change->relative_name      = push_string_copy(temp_arena(), name);
                change->prev_relative_name = push_string_copy(temp_arena(), prev_name);

                if (!result)
                {
                    result = change;
                    last_change = change;
                }
                else
                {
                    last_change->next = change;
                }

                if (it->callback)
                {
                    it->callback(it, *change);
                }
            }

            // Are there more events to handle?
            if (!event->NextEntryOffset) break;

            *((u8 **)&event) += event->NextEntryOffset;
        }

        // Queue the next event
        BOOL success = ReadDirectoryChangesW(
                it->file, it->buffer, count_of(it->buffer), TRUE, it->flags, NULL, &it->overlapped, NULL);
    }

    return result;
}

void hotloader_free(Hotloader *it) {
    if (it->file) {
        CloseHandle(it->file);
        it->file = 0;
    }
}

#endif // OS_WINDOWS


#if OS_MACOS

// NOTE(nick): Stubs for other platforms.
struct Hotloader {
    String path;
};

bool hotloader_init(Hotloader *it, String path) { return false; }

void hotloader_register_callback(Hotloader *it, Hotloader_Callback callback) {}

Asset_Change *hotloader_poll_events(Hotloader *it) {
    return false;
};

void hotloader_free(Hotloader *it) {}

#endif // OS_MACOS

#if OS_LINUX

// NOTE(nick): Stubs for other platforms.
struct Hotloader {
    String path;
};

bool hotloader_init(Hotloader *it, String path) { return false; }

void hotloader_register_callback(Hotloader *it, Hotloader_Callback callback) {}

Asset_Change *hotloader_poll_events(Hotloader *it) {
    return false;
};

void hotloader_free(Hotloader *it) {}

#endif // OS_LINUX


#endif // NA_HOTLOADER_IMPLEMENTATION
