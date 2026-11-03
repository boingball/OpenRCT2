/* SPDX-License-Identifier: GPL-3.0-only */
/* Integer-only preflight: run before C++ runtime initialisers in the game. */
#include <proto/dos.h>
#include <dos/dos.h>
#include <string.h>

static void message(const char* text)
{
    Write(Output(), (APTR)text, (LONG)strlen(text));
}

/* Enumerate the parent first and retain the actual entry spelling. This
 * does not depend on a previous CD/Dir warming a shared filesystem's cache. */
static int childName(BPTR parent, const char* wanted, int directory, char* result, size_t capacity)
{
    struct FileInfoBlock* info = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
    int found = 0;
    if (!info)
        return 0;
    if (Examine(parent, info) && info->fib_DirEntryType > 0)
    {
        while (ExNext(parent, info))
        {
            const unsigned char* a = (const unsigned char*)info->fib_FileName;
            const unsigned char* b = (const unsigned char*)wanted;
            while (*a && *b)
            {
                unsigned ca = *a, cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
                if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
                if (ca != cb) break;
                ++a;
                ++b;
            }
            if (!*a && !*b && (directory ? info->fib_DirEntryType > 0 : info->fib_DirEntryType < 0)
                && strlen(info->fib_FileName) < capacity)
            {
                strcpy(result, info->fib_FileName);
                found = 1;
                break;
            }
        }
    }
    FreeDosObject(DOS_FIB, info);
    return found;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        message("Usage: OpenRCT2Check <original-RCT2-folder> (or --cpu-only)\n");
        return 10;
    }
    int cpuOnly = strcmp(argv[1], "--cpu-only") == 0;
    if (!cpuOnly)
    {
        char path[1024], name[108];
        BPTR root = Lock((STRPTR)argv[1], ACCESS_READ);
        BPTR data = 0, file;
        unsigned char header[8];
        LONG count;
        int resolved = 0;
        if (root)
        {
            if (NameFromLock(root, (STRPTR)path, sizeof(path))
                && childName(root, "Data", 1, name, sizeof(name))
                && AddPart((STRPTR)path, (STRPTR)name, sizeof(path)))
            {
                data = Lock((STRPTR)path, ACCESS_READ);
                if (data && childName(data, "G1.DAT", 0, name, sizeof(name))
                    && AddPart((STRPTR)path, (STRPTR)name, sizeof(path)))
                    resolved = 1;
            }
            if (data) UnLock(data);
            UnLock(root);
        }
        if (!resolved)
        {
            message("Could not resolve the original RCT2 Data/G1.DAT directory entries.\n"
                "RCT2: must point to the original game installation, not OpenRCT2's included data.\n");
            return 10;
        }
        message("Original G1 resolved as: ");
        message(path);
        message("\n");
        file = Open((STRPTR)path, MODE_OLDFILE);
        if (!file)
        {
            message("Original RCT2 Data/G1.DAT was not found.\n"
                "RCT2: must point to your original RollerCoaster Tycoon 2 installation.\n"
                "The included OpenRCT2 data folder is different and contains no G1.DAT.\n"
                "Example: Assign RCT2: Work:Games/RollerCoasterTycoon2\n");
            return 10;
        }
        count = Read(file, header, sizeof(header));
        Close(file);
        if (count != sizeof(header))
        {
            message("Data/G1.DAT is empty or unreadable. Check your original RCT2 installation.\n");
            return 10;
        }
    }
    message(cpuOnly ? "FPU prerequisite: none (not a test of the game runtime).\n"
        : "Original-data path prerequisite: PASS (game startup not yet tested).\n");
    return 0;
}
