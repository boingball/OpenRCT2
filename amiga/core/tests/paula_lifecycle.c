/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only fault injection: compile with -D__amigaos__ -Itests/paula_mock.
 * Uses the actual production Paula worker with mocked Exec/device calls.
 */
#include "../../../src/openrct2-ui/platform/amiga/amiga_audio.c"
#include <stdio.h>
static struct ExecBase base = {709379};
struct ExecBase* SysBase = &base;
static int calls, failAt, live, device, writes, waits, busy, failOpen, ioError;
static struct IORequest* inFlight[4];
static int sent[4];
static const char* selection;
static int backendOK[2], backendCalls[2];
static void* allocation(unsigned size)
{
    if (++calls == failAt) return NULL;
    void* p = calloc(1, size); assert(p); ++live; return p;
}
struct MsgPort* CreateMsgPort(void) { return allocation(sizeof(struct MsgPort)); }
void DeleteMsgPort(struct MsgPort* p) { --live; free(p); }
struct IORequest* CreateIORequest(struct MsgPort* p, unsigned size)
{ struct IORequest* r = allocation(size); if (r) r->io_Message.mn_ReplyPort=p; return r; }
void DeleteIORequest(struct IORequest* r) { assert(!busy); --live; free(r); }
void* AllocMem(unsigned n, unsigned flags) { (void)flags; return allocation(n); }
void FreeMem(void* p, unsigned n) { (void)n; assert(!busy); --live; free(p); }
int OpenDevice(STRPTR name, int unit, struct IORequest* req, int flags)
{
    (void)unit; (void)flags;
    assert(!strcmp(name, AUDIONAME));
    struct IOAudio* r=(struct IOAudio*)req;
    assert(r->ioa_Length==1 && r->ioa_Data[0]==3);
    assert(req->io_Flags==ADIOF_NOWAIT);
    if (failOpen) return -1;
    req->io_Device=(void*)1; req->io_Unit=(struct Unit*)3;
    r->ioa_AllocKey=42; ++device; return 0;
}
void CloseDevice(struct IORequest* r) { (void)r; assert(!busy); --device; }
void BeginIO(struct IORequest* r)
{
    struct IOAudio* a=(struct IOAudio*)r;
    int slot;
    for (slot=0; slot<4 && inFlight[slot] && inFlight[slot]!=r; ++slot) {}
    assert(slot<4 && !sent[slot]); inFlight[slot]=r; sent[slot]=1;
    assert(a->ioa_AllocKey==42 && r->io_Flags==ADIOF_PERVOL);
    assert(a->ioa_Length==2048 && a->ioa_Period==161 && a->ioa_Volume==64 && a->ioa_Cycles==1);
    assert((uintptr_t)r->io_Unit==1 || (uintptr_t)r->io_Unit==2);
    ++writes; ++busy;
}
int WaitIO(struct IORequest* r)
{
    int slot; for(slot=0; slot<4 && inFlight[slot]!=r; ++slot) {}
    assert(slot<4 && sent[slot]); sent[slot]=0; --busy;
    if (++waits==1 && ioError) r->io_Error=-12;
    if (waits==4) g_quit=1;
    return 0;
}
void AbortIO(struct IORequest* r) { (void)r; /* completion still requires WaitIO */ }
void Signal(struct Task* t, ULONG s) { (void)t; (void)s; }
ULONG Wait(ULONG s) { return s; }
void SendIO(struct IORequest* r) { (void)r; assert(0); }
struct Task* FindTask(void* p) { return p; }
LONG AllocSignal(int n) { (void)n; return 1; }
void FreeSignal(LONG n) { (void)n; }
void* CreateNewProcTags(ULONG n, ...)
{ (void)n; ++backendCalls[g_paula]; g_status=backendOK[g_paula] ? 1 : -1; return (void*)1; }
LONG GetVar(STRPTR n, STRPTR b, int l, int f)
{ (void)n; (void)f; if (!selection) return -1; strncpy(b,selection,(unsigned)l); return strlen(selection); }
unsigned amiga_ticks_ms(void) { return 0; }
static void run(int fail, int openFail, int error)
{
    calls=live=device=writes=waits=busy=0; failAt=fail; failOpen=openFail; ioError=error;
    memset(inFlight,0,sizeof inFlight); memset(sent,0,sizeof sent);
    g_quit=0; g_head=g_tail=0; g_sigReady=1; g_sigDone=2;
    unsigned char ring[RING_BYTES]={0}; g_ring=ring;
    paula_process();
    assert(!live && !device && !busy && g_status==2);
    if (!fail && !openFail) assert(writes==(error ? 4 : 6) && waits==writes);
    else assert(writes==0);
}
static void select_backend(const char* mode, int ahiOK, int paulaOK, int ahiCalls, int paulaCalls, int success)
{
    selection=mode; backendOK[0]=ahiOK; backendOK[1]=paulaOK;
    backendCalls[0]=backendCalls[1]=0; failAt=0; g_ring=NULL; g_status=0;
    assert(amiga_audio_open(22050,2048,2)==success);
    assert(backendCalls[0]==ahiCalls && backendCalls[1]==paulaCalls);
    amiga_audio_close();
    assert(!live && g_ring==NULL);
}
int main(void)
{
    int i;
    run(0,0,0); /* orderly stop with both queued stereo pairs accounted for */
    for(i=1;i<=9;++i) run(i,0,0); /* every allocation can fail */
    run(0,1,0);
    run(0,0,1); /* stolen-channel/write error also drains every outstanding request */ /* channel allocation/device open can fail */
    select_backend("PAULA",1,1,0,1,1); /* must not probe AHI */
    select_backend("paula",1,0,0,1,0);
    select_backend("AHI",0,1,1,0,0); /* forced AHI must not fall back */
    select_backend(NULL,1,1,1,0,1);
    select_backend(NULL,0,1,1,1,1);
    select_backend(NULL,0,0,1,1,0);
    puts("Paula lifecycle: two queued stereo pairs, stop/reap/free allocation/open/write failures and backend selection/fallback PASS");
    return 0;
}
