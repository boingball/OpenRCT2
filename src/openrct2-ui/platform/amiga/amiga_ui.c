/*****************************************************************************
 * AmigaOS display/input layer for the openrct2-ui SDL2 shim (plain C).
 * 8-bit CyberGraphX/Picasso96 screen, backdrop window, IDCMP input.
 *****************************************************************************/
#ifdef __amigaos__

/* SDK varargs wrappers take &firstTag, which is not a portable tag array
 * under optimisation. Keep every OS argument list explicit. */
#define NO_INLINE_STDARG

    #include "amiga_ui.h"
    #include "amiga_c2p8.h"

    #include <cybergraphx/cybergraphics.h>
    #include <devices/inputevent.h>
    #include <graphics/displayinfo.h>
    #include <graphics/modeid.h>
    #include <intuition/intuition.h>
    #include <intuition/pointerclass.h>
    #include <intuition/screens.h>
    #include <proto/cybergraphics.h>
    #include <proto/dos.h>
    #include <proto/exec.h>
    #include <proto/graphics.h>
    #include <proto/intuition.h>
    #include <proto/keymap.h>
    #include <stdio.h>
    #include <string.h>

extern void amiga_trace(const char* line);
static void trace(const char* fmt, long a, long b, long c, long d)
{
    char buf[160];
    snprintf(buf, sizeof buf, fmt, a, b, c, d);
    amiga_trace(buf);
}

struct Library* CyberGfxBase = NULL;
struct Library* KeymapBase = NULL;

static struct Screen* s_screen = NULL;
static struct Window* s_window = NULL;
static int s_width = 0, s_height = 0;
static int use_aga(void);
static char s_error[256];

const char* amiga_ui_error(void) { return s_error; }

static void screen_error(const char* stage, long code)
{
    snprintf(s_error, sizeof s_error, "%s %s failed (code %ld); Chip free %lu, largest %lu; Fast free %lu",
        use_aga() ? "AGA" : "RTG", stage, code,
        (unsigned long)AvailMem(MEMF_CHIP), (unsigned long)AvailMem(MEMF_CHIP | MEMF_LARGEST),
        (unsigned long)AvailMem(MEMF_FAST));
    amiga_trace(s_error);
}
static int s_mouseX = 0, s_mouseY = 0, s_buttons = 0;
static ULONG s_palette[1 + 256 * 3 + 1];
static Object* s_pointerObj = NULL;
static struct BitMap* s_pointerBM = NULL;
static int s_pointerShown = 1;
static struct BitMap* s_agaBitmap = NULL;
/* Chunky copy of what the Chip planes show; one byte per pixel, s_agaShadowPitch bytes per row. */
static UBYTE* s_agaShadow = NULL;
static int s_agaPitch = 0, s_agaPlaneSize = 0, s_agaShadowPitch = 0;

static int use_aga(void)
{
    char value[16] = { 0 };
    return GetVar((STRPTR)"OPENRCT2_DISPLAY", value, sizeof(value), 0) > 0
        && (strcmp(value, "AGA") == 0 || strcmp(value, "aga") == 0);
}

static ULONG aga_mode(void)
{
    const ULONG mode = PAL_MONITOR_ID | HIRESLACE_KEY;
    struct DimensionInfo dimensions;
    if (ModeNotAvailable(mode) != 0
        || !GetDisplayInfoData(NULL, (UBYTE*)&dimensions, sizeof(dimensions), DTAG_DIMS, mode)
        || dimensions.MaxDepth < 8)
    {
        amiga_trace("ui: AGA PAL 640x512x8 unavailable (AGA chipset and PAL monitor required)");
        return INVALID_ID;
    }
    return mode;
}

static void free_aga(void)
{
    if (s_agaBitmap) { WaitBlit(); FreeBitMap(s_agaBitmap); }
    if (s_agaShadow) FreeVec(s_agaShadow);
    s_agaBitmap = NULL;
    s_agaShadow = NULL;
    s_agaPitch = s_agaPlaneSize = s_agaShadowPitch = 0;
}

static void free_pointer_shape(Object* obj, struct BitMap* bm)
{
    if (obj != NULL)
        DisposeObject(obj);
    if (bm != NULL)
        FreeBitMap(bm);
}

static int open_libs(void)
{
    const int aga = use_aga();
    if (!aga && CyberGfxBase == NULL)
        CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    if (KeymapBase == NULL)
        KeymapBase = OpenLibrary("keymap.library", 36);
    return aga || CyberGfxBase != NULL;
}

/* Smallest 8-bit RTG mode that fits width x height; the largest one if none fits. */
static ULONG find_mode(int width, int height)
{
    ULONG id = INVALID_ID, best = INVALID_ID, largest = INVALID_ID;
    long bestArea = -1, largestArea = -1;
    if (!open_libs())
        return INVALID_ID;
    if (use_aga())
        return aga_mode();
    while ((id = NextDisplayInfo(id)) != (ULONG)INVALID_ID)
    {
        long w, h, area;
        if (!IsCyberModeID(id) || GetCyberIDAttr(CYBRIDATTR_DEPTH, id) != 8)
            continue;
        w = (long)GetCyberIDAttr(CYBRIDATTR_WIDTH, id);
        h = (long)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
        area = w * h;
        if (area > largestArea)
        {
            largestArea = area;
            largest = id;
        }
        if (w >= width && h >= height && (bestArea < 0 || area < bestArea))
        {
            bestArea = area;
            best = id;
        }
    }
    trace("ui: find_mode %ldx%ld -> best %08lx largest %08lx", width, height, (long)best, (long)largest);
    return best != (ULONG)INVALID_ID ? best : largest;
}

/* Every distinct width x height the RTG driver offers at 8 bits, largest first. Returns the count. */
int amiga_ui_list_modes(int* widths, int* heights, int max)
{
    ULONG id = INVALID_ID;
    int n = 0;
    if (!open_libs() || max <= 0)
        return 0;
    if (use_aga())
    {
        if (aga_mode() == (ULONG)INVALID_ID) return 0;
        widths[0] = 640; heights[0] = 512;
        return 1;
    }
    while ((id = NextDisplayInfo(id)) != (ULONG)INVALID_ID)
    {
        int w, h, i;
        if (!IsCyberModeID(id) || GetCyberIDAttr(CYBRIDATTR_DEPTH, id) != 8)
            continue;
        w = (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, id);
        h = (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, id);
        if (w < 640 || h < 480) /* the game cannot lay out its windows below 640x480 */
            continue;
        for (i = 0; i < n; i++)
            if (widths[i] == w && heights[i] == h)
                break;
        if (i < n)
            continue;
        if (n < max)
        {
            widths[n] = w;
            heights[n] = h;
            n++;
        }
    }
    trace("ui: %ld distinct 8-bit RTG modes", (long)n, 0L, 0L, 0L);
    return n;
}

int amiga_ui_mode_available(int width, int height)
{
    ULONG id = find_mode(width, height);
    if (id == (ULONG)INVALID_ID)
        return 0;
    if (use_aga())
        return width == 640 && height == 512;
    return GetCyberIDAttr(CYBRIDATTR_WIDTH, id) == (ULONG)width && GetCyberIDAttr(CYBRIDATTR_HEIGHT, id) == (ULONG)height;
}

int amiga_ui_desktop_size(int* width, int* height)
{
    struct Screen* wb = LockPubScreen(NULL);
    if (wb == NULL)
        return 0;
    *width = wb->Width;
    *height = wb->Height;
    UnlockPubScreen(NULL, wb);
    return 1;
}

int amiga_ui_open(int width, int height, const char* title, int* outWidth, int* outHeight)
{
    ULONG modeId;
    LONG screenError = 0;
    amiga_ui_close();
    s_error[0] = 0;
    modeId = find_mode(width, height);
    if (modeId == (ULONG)INVALID_ID)
    {
        screen_error("display mode selection", OSERR_UNKNOWNMODE);
        return 0;
    }
    if (use_aga())
    {
        int plane;
        width = 640; height = 512;
        s_agaBitmap = AllocBitMap(width, height, 8, BMF_DISPLAYABLE | BMF_CLEAR, NULL);
        if (!s_agaBitmap) { screen_error("Chip bitmap allocation", OSERR_NOCHIPMEM); return 0; }
        s_agaPitch = s_agaBitmap->BytesPerRow;
        s_agaPlaneSize = s_agaPitch * height;
        if (s_agaBitmap->Depth != 8 || (s_agaPitch & 3) || (s_agaBitmap->Flags & BMF_INTERLEAVED))
        {
            screen_error("bitmap layout", 0);
            free_aga();
            return 0;
        }
        for (plane = 0; plane < 8; ++plane)
        {
            if (!s_agaBitmap->Planes[plane] || !(TypeOfMem(s_agaBitmap->Planes[plane]) & MEMF_CHIP))
            {
                screen_error("planar Chip bitmap validation", 0);
                free_aga();
                return 0;
            }
        }
        s_agaShadowPitch = s_agaPitch * 8;
        s_agaShadow = AllocVec((ULONG)s_agaShadowPitch * height, MEMF_FAST | MEMF_CLEAR);
        if (!s_agaShadow)
        {
            screen_error("Fast bitmap allocation", OSERR_NOMEM);
            free_aga();
            return 0;
        }
        amiga_trace("ui: AGA PAL 640x512x8, single-pass c2p8 of changed 32-pixel groups");
    }
    else
    {
        /* open the screen at the mode's own size: no autoscroll, and the game adapts to the size it gets */
        int mw = (int)GetCyberIDAttr(CYBRIDATTR_WIDTH, modeId);
        int mh = (int)GetCyberIDAttr(CYBRIDATTR_HEIGHT, modeId);
        trace("ui: open requested %ldx%ld mode %ldx%ld", width, height, mw, mh);
        width = mw;
        height = mh;
    }
    const struct TagItem screenTags[] = {
        { SA_DisplayID, modeId }, { SA_Width, width }, { SA_Height, height }, { SA_Depth, 8 },
        { SA_Title, (ULONG)title }, { SA_Quiet, TRUE }, { SA_ShowTitle, FALSE }, { SA_Behind, FALSE },
        { s_agaBitmap ? SA_BitMap : TAG_IGNORE, (ULONG)s_agaBitmap },
        { SA_ErrorCode, (ULONG)&screenError }, { TAG_DONE, 0 }
    };
    s_screen = OpenScreenTagList(NULL, screenTags);
    if (s_screen == NULL)
    {
        screen_error("OpenScreenTagList", screenError);
        free_aga();
        return 0;
    }
    const struct TagItem windowTags[] = {
        { WA_CustomScreen, (ULONG)s_screen }, { WA_Left, 0 }, { WA_Top, 0 },
        { WA_Width, s_screen->Width }, { WA_Height, s_screen->Height },
        { WA_Backdrop, TRUE }, { WA_Borderless, TRUE }, { WA_Activate, TRUE }, { WA_RMBTrap, TRUE },
        { WA_NoCareRefresh, TRUE }, { WA_SimpleRefresh, TRUE }, { WA_ReportMouse, TRUE },
        { WA_IDCMP, IDCMP_RAWKEY | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW },
        { TAG_DONE, 0 }
    };
    s_window = OpenWindowTagList(NULL, windowTags);
    if (s_window == NULL)
    {
        screen_error("OpenWindowTagList", IoErr());
        CloseScreen(s_screen);
        s_screen = NULL;
        free_aga();
        return 0;
    }
    s_width = s_screen->Width;
    s_height = s_screen->Height;
    if (s_agaBitmap)
    {
        int plane;
        /* Intuition may have filled the backdrop. The chunky shadow must equal
         * the planes, so clear the planes to match the cleared shadow; the first
         * game frame repaints the whole screen anyway. */
        WaitBlit();
        for (plane = 0; plane < 8; ++plane)
            memset(s_agaBitmap->Planes[plane], 0, (size_t)s_agaPlaneSize);
    }
    *outWidth = s_width;
    *outHeight = s_height;
    trace("ui: screen %ldx%ld depth %ld open", s_width, s_height, s_screen->RastPort.BitMap->Depth, 0);
    return 1;
}

void amiga_ui_close(void)
{
    if (s_window != NULL)
    {
        CloseWindow(s_window);
        s_window = NULL;
    }
    free_pointer_shape(s_pointerObj, s_pointerBM);
    s_pointerObj = NULL;
    s_pointerBM = NULL;
    if (s_screen != NULL)
    {
        CloseScreen(s_screen);
        s_screen = NULL;
        amiga_trace("ui: screen closed");
    }
    free_aga();
}

int amiga_ui_is_open(void)
{
    return s_window != NULL;
}

void amiga_ui_set_palette(const unsigned char* rgb)
{
    int i;
    if (s_screen == NULL)
        return;
    s_palette[0] = (256UL << 16) | 0;
    for (i = 0; i < 256; i++)
    {
        s_palette[1 + i * 3 + 0] = (ULONG)rgb[i * 3 + 0] * 0x01010101UL;
        s_palette[1 + i * 3 + 1] = (ULONG)rgb[i * 3 + 1] * 0x01010101UL;
        s_palette[1 + i * 3 + 2] = (ULONG)rgb[i * 3 + 2] * 0x01010101UL;
    }
    s_palette[1 + 256 * 3] = 0;
    LoadRGB32(&s_screen->ViewPort, s_palette);
}

/* CyberGraphX LockBitMapTags() tags; bebbo's trimmed cybergraphics.h lacks them. */
#ifndef LBMI_PIXFMT
    #define LBMI_PIXFMT (0x84001004)
    #define LBMI_BYTESPERROW (0x84001006)
    #define LBMI_BASEADDRESS (0x84001007)
#endif
#ifndef PIXFMT_LUT8
    #define PIXFMT_LUT8 (0UL)
#endif

/* Blit method: 0 = WriteChunkyPixels() through the RTG driver (default), 1 = lock the screen bitmap and copy
 * the rows straight into the 8-bit framebuffer. The direct path is opt-in (env OPENRCT2_BLIT=direct): a
 * PiStorm/Emu68 tester saw the game hang during loading with it, so it stays an experiment until measured. */
static int s_blitMethod = -1;

int amiga_ui_blit_method(void)
{
    if (s_agaBitmap) return 2;
    return s_blitMethod;
}

static int lock_blit(const unsigned char* src, int srcPitch, int x, int y, int w, int h)
{
    APTR handle;
    ULONG fmt = ~0UL, bpr = 0;
    UBYTE* base = NULL;
    int row;
    const struct TagItem lockTags[] = {
        { LBMI_PIXFMT, (ULONG)&fmt }, { LBMI_BYTESPERROW, (ULONG)&bpr },
        { LBMI_BASEADDRESS, (ULONG)&base }, { TAG_DONE, 0 }
    };
    handle = LockBitMapTagList(s_screen->RastPort.BitMap, lockTags);
    if (handle == NULL)
        return 0;
    if (fmt != PIXFMT_LUT8 || base == NULL || bpr == 0)
    {
        UnLockBitMap(handle);
        return 0;
    }
    base += (ULONG)y * bpr + (ULONG)x;
    for (row = 0; row < h; row++)
    {
        memcpy(base, src, (size_t)w);
        base += bpr;
        src += srcPitch;
    }
    UnLockBitMap(handle);
    return 1;
}

void amiga_ui_blit(const unsigned char* src, int srcPitch, int x, int y, int w, int h)
{
    if (s_window == NULL || w <= 0 || h <= 0)
        return;
    if (x < 0 || y < 0 || x + w > s_width || y + h > s_height)
        return;
    if (s_agaBitmap)
    {
        uint8_t* chip[8];
        int plane;
        for (plane = 0; plane < 8; ++plane) chip[plane] = s_agaBitmap->Planes[plane];
        WaitBlit();
        amiga_c2p8_update(src, srcPitch, s_agaShadow, s_agaShadowPitch, chip, s_agaPitch, x, y, w, h);
        return;
    }
    if (s_blitMethod < 0)
    {
        char v[16];
        s_blitMethod = 0;
        if (GetVar((STRPTR) "OPENRCT2_BLIT", (STRPTR)v, sizeof v, 0) > 0)
            s_blitMethod = (v[0] == 'd' || v[0] == 'D' || v[0] == 'l' || v[0] == 'L') ? 1 : 0;
    }
    if (s_blitMethod == 1)
    {
        if (lock_blit(src, srcPitch, x, y, w, h))
            return;
        s_blitMethod = 0; /* bitmap not lockable as 8-bit chunky: fall back for good */
    }
    WriteChunkyPixels(s_window->RPort, x, y, x + w - 1, y + h - 1, (UBYTE*)src, srcPitch);
}

int amiga_ui_poll(amiga_ui_event* ev)
{
    struct IntuiMessage* m;
    if (s_window == NULL)
        return 0;
    while ((m = (struct IntuiMessage*)GetMsg(s_window->UserPort)) != NULL)
    {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        UWORD qual = m->Qualifier;
        int mx = m->MouseX, my = m->MouseY;
        APTR iaddr = m->IAddress;
        int handled = 1;
        memset(ev, 0, sizeof(*ev));
        switch (cls)
        {
            case IDCMP_RAWKEY:
                if ((code & 0x7F) == 0x7A || (code & 0x7F) == 0x7B)
                {
                    /* NewMouse wheel codes */
                    if (code & 0x80)
                        handled = 0;
                    else
                    {
                        ev->type = AMIGA_UI_EV_WHEEL;
                        ev->code = (code == 0x7A) ? 1 : -1;
                    }
                }
                else
                {
                    struct InputEvent ie;
                    char buf[4];
                    LONG n;
                    memset(&ie, 0, sizeof ie);
                    ie.ie_Class = IECLASS_RAWKEY;
                    ie.ie_Code = code;
                    ie.ie_Qualifier = qual;
                    ie.ie_EventAddress = *((APTR*)iaddr);
                    ev->type = AMIGA_UI_EV_RAWKEY;
                    ev->code = code;
                    ev->qual = qual;
                    ev->ascii = -1;
                    if (KeymapBase != NULL && !(code & 0x80))
                    {
                        n = MapRawKey(&ie, buf, sizeof buf, NULL);
                        if (n == 1)
                            ev->ascii = (unsigned char)buf[0];
                    }
                }
                break;
            case IDCMP_MOUSEMOVE:
                ev->type = AMIGA_UI_EV_MOUSEMOVE;
                ev->x = s_mouseX = mx;
                ev->y = s_mouseY = my;
                break;
            case IDCMP_MOUSEBUTTONS:
            {
                int button = 0, down = 0;
                switch (code)
                {
                    case SELECTDOWN:
                        button = 1;
                        down = 1;
                        break;
                    case SELECTUP:
                        button = 1;
                        break;
                    case MENUDOWN:
                        button = 3;
                        down = 1;
                        break;
                    case MENUUP:
                        button = 3;
                        break;
                    case MIDDLEDOWN:
                        button = 2;
                        down = 1;
                        break;
                    case MIDDLEUP:
                        button = 2;
                        break;
                    default:
                        handled = 0;
                        break;
                }
                if (handled)
                {
                    ev->type = AMIGA_UI_EV_BUTTON;
                    ev->code = button;
                    ev->qual = down ? 0x8000 : 0;
                    ev->x = s_mouseX = mx;
                    ev->y = s_mouseY = my;
                    if (down)
                        s_buttons |= 1 << (button - 1);
                    else
                        s_buttons &= ~(1 << (button - 1));
                }
                break;
            }
            case IDCMP_ACTIVEWINDOW:
                ev->type = AMIGA_UI_EV_ACTIVE;
                ev->code = 1;
                break;
            case IDCMP_INACTIVEWINDOW:
                ev->type = AMIGA_UI_EV_ACTIVE;
                ev->code = 0;
                break;
            default:
                handled = 0;
                break;
        }
        ReplyMsg((struct Message*)m);
        if (handled)
            return 1;
    }
    return 0;
}

int amiga_ui_map_key(int code, int qual, char* out, int outSize)
{
    struct InputEvent ie;
    LONG n;
    if (KeymapBase == NULL)
        return 0;
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = (UWORD)code;
    ie.ie_Qualifier = (UWORD)qual;
    n = MapRawKey(&ie, out, outSize, NULL);
    return n < 0 ? 0 : (int)n;
}

int amiga_ui_mouse(int* x, int* y)
{
    if (s_window != NULL)
    {
        s_mouseX = s_window->MouseX;
        s_mouseY = s_window->MouseY;
    }
    if (x != NULL)
        *x = s_mouseX;
    if (y != NULL)
        *y = s_mouseY;
    return s_buttons;
}

/* ---- pointer shapes ------------------------------------------------------------------------------
 * The game's tool cursors are 32x32 two-colour bitmaps; intuition's pointerclass (V39+) shows pointers up to
 * 64 px wide. Pointer colour n (1..3) is screen pen 16+n, which on our 8-bit screen holds whatever the game
 * palette has there, so the darkest of the three pens draws the black parts and the lightest the white parts. */

static int pen_luma(int pen)
{
    ULONG r = s_palette[1 + pen * 3 + 0] >> 24, g = s_palette[1 + pen * 3 + 1] >> 24, b = s_palette[1 + pen * 3 + 2] >> 24;
    return (int)(r * 3 + g * 6 + b);
}

static void set_window_pointer(Object* pointer)
{
    const struct TagItem tags[] = { { WA_Pointer, (ULONG)pointer }, { TAG_DONE, 0 } };
    SetWindowPointerA(s_window, tags);
}

int amiga_ui_set_pointer(const unsigned char* data, const unsigned char* mask, int w, int h, int hotX, int hotY)
{
    struct BitMap* bm;
    Object* obj;
    int x, y, blackPen = 2, whitePen = 1, bytesPerRow = (w + 7) / 8;
    if (s_window == NULL || w <= 0 || h <= 0 || w > 64 || h > 64)
        return 0;
    {
        int l1 = pen_luma(17), l2 = pen_luma(18), l3 = pen_luma(19);
        blackPen = (l1 <= l2 && l1 <= l3) ? 1 : (l2 <= l3) ? 2 : 3;
        whitePen = (l1 >= l2 && l1 >= l3) ? 1 : (l2 >= l3) ? 2 : 3;
        if (whitePen == blackPen)
            whitePen = blackPen == 1 ? 2 : 1;
    }
    bm = AllocBitMap(w, h, 2, BMF_CLEAR | BMF_DISPLAYABLE, NULL);
    if (bm == NULL)
        return 0;
    for (y = 0; y < h; y++)
    {
        UBYTE* p0 = bm->Planes[0] + y * bm->BytesPerRow;
        UBYTE* p1 = bm->Planes[1] + y * bm->BytesPerRow;
        for (x = 0; x < w; x++)
        {
            int bit = 0x80 >> (x & 7);
            int d = (data[y * bytesPerRow + x / 8] & bit) != 0, m = (mask[y * bytesPerRow + x / 8] & bit) != 0;
            int pen = (m && d) ? blackPen : m ? whitePen : d ? blackPen : 0;
            if (pen & 1)
                p0[x / 8] |= bit;
            if (pen & 2)
                p1[x / 8] |= bit;
        }
    }
    const struct TagItem pointerTags[] = {
        { POINTERA_BitMap, (ULONG)bm }, { POINTERA_XOffset, (ULONG)-hotX }, { POINTERA_YOffset, (ULONG)-hotY },
        { POINTERA_WordWidth, (ULONG)((w + 15) / 16) }, { POINTERA_XResolution, POINTERXRESN_SCREENRES },
        { POINTERA_YResolution, POINTERYRESN_SCREENRES }, { TAG_DONE, 0 }
    };
    obj = NewObjectA(NULL, (STRPTR)"pointerclass", pointerTags);
    if (obj == NULL)
    {
        FreeBitMap(bm);
        return 0;
    }
    if (s_pointerShown)
        set_window_pointer(obj);
    free_pointer_shape(s_pointerObj, s_pointerBM); /* after the switch: intuition must not see it vanish first */
    s_pointerObj = obj;
    s_pointerBM = bm;
    return 1;
}

void amiga_ui_reset_pointer(void)
{
    if (s_window != NULL && s_pointerShown)
        set_window_pointer(NULL);
    free_pointer_shape(s_pointerObj, s_pointerBM);
    s_pointerObj = NULL;
    s_pointerBM = NULL;
}

void amiga_ui_show_pointer(int show)
{
    static UWORD blank[6] = { 0, 0, 0, 0, 0, 0 };
    if (s_window == NULL)
        return;
    s_pointerShown = show;
    if (!show)
        SetPointer(s_window, blank, 1, 16, 0, 0);
    else if (s_pointerObj != NULL)
        set_window_pointer(s_pointerObj);
    else
        ClearPointer(s_window);
}

int amiga_ui_request(const char* title, const char* body, const char* gadgets)
{
    struct EasyStruct es;
    LONG r;
    int n = 1;
    const char* p;
    for (p = gadgets; *p; p++)
        if (*p == '|')
            n++;
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE*)title;
    es.es_TextFormat = (UBYTE*)"%s";
    es.es_GadgetFormat = (UBYTE*)gadgets;
    const char* args[] = { body };
    r = EasyRequestArgs(s_window, &es, NULL, (APTR)args);
    /* EasyRequestArgs: 1..n-1 for the left gadgets, 0 for the rightmost */
    if (r == 0)
        return n - 1;
    return (int)r - 1;
}

#endif
