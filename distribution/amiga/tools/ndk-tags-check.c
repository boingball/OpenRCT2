/* SDK/compiler regression check, run with the production -O3 flags.
 * A fake Intuition vector validates the actual A1 tag list received by the
 * OS-call ABI. It does not require an Intuition screen implementation.
 * Define TEST_SDK_VARARGS to reproduce the old &firstTag wrapper failure.
 */
#include <proto/intuition.h>
#include <intuition/screens.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct IntuitionBase* IntuitionBase;
static unsigned char vectors[612 + 8] __attribute__((aligned(4)));

unsigned long check_tags(const struct TagItem* tags)
{
    return tags[0].ti_Tag == SA_DisplayID && tags[0].ti_Data == 0x00029004
        && tags[1].ti_Tag == SA_Width && tags[1].ti_Data == 640
        && tags[2].ti_Tag == SA_Height && tags[2].ti_Data == 512
        && tags[3].ti_Tag == SA_Depth && tags[3].ti_Data == 8
        && tags[4].ti_Tag == TAG_DONE;
}

extern void tag_vector(void);
__asm__(".text\n.even\n_tag_vector:\n"
        "move.l %a1,-(%sp)\njsr _check_tags\naddq.l #4,%sp\nrts\n");

int main(void)
{
    uintptr_t target = (uintptr_t)tag_vector;
    unsigned long ok;
    vectors[0] = 0x4e; vectors[1] = 0xf9; /* JMP absolute long at LVO -612 */
    memcpy(vectors + 2, &target, 4);
    IntuitionBase = (struct IntuitionBase*)(vectors + 612);
#ifdef TEST_SDK_VARARGS
    ok = (unsigned long)OpenScreenTags(NULL, SA_DisplayID, 0x00029004UL,
        SA_Width, 640UL, SA_Height, 512UL, SA_Depth, 8UL, TAG_DONE);
#else
    const struct TagItem tags[] = {
        { SA_DisplayID, 0x00029004 }, { SA_Width, 640 }, { SA_Height, 512 },
        { SA_Depth, 8 }, { TAG_DONE, 0 }
    };
    ok = (unsigned long)OpenScreenTagList(NULL, tags);
#endif
    printf("NDK screen tags through real m68k library-call ABI: %s\n", ok == 1 ? "PASS" : "FAIL");
    return ok == 1 ? 0 : 1;
}
