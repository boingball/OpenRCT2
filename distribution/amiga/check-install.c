/* SPDX-License-Identifier: GPL-3.0-only */
/* Integer-only preflight: run before C++ runtime initialisers in the game. */
#include <proto/dos.h>
#include <dos/dos.h>
#include <string.h>

static void message(const char* text)
{
    Write(Output(), (APTR)text, (LONG)strlen(text));
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
        char path[1024];
        BPTR file;
        unsigned char header[8];
        LONG count;
        if (strlen(argv[1]) >= sizeof(path))
            return 10;
        strcpy(path, argv[1]);
        if (!AddPart((STRPTR)path, (STRPTR)"Data/G1.DAT", sizeof(path)))
            return 10;
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
