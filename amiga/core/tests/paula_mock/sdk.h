#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uintptr_t ULONG;
typedef intptr_t LONG;
typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef char* STRPTR;
struct Task { int dummy; };
struct Unit { int dummy; };
struct MsgPort { int dummy; };
struct Node { int ln_Pri; };
struct Message { struct Node mn_Node; struct MsgPort* mn_ReplyPort; };
struct IORequest { struct Message io_Message; void* io_Device; struct Unit* io_Unit; int io_Command, io_Flags, io_Error; void* io_Data; ULONG io_Length, io_Offset; };
struct IOAudio { struct IORequest ioa_Request; int ioa_AllocKey; UBYTE* ioa_Data; ULONG ioa_Length; UWORD ioa_Period, ioa_Volume, ioa_Cycles; };
struct AHIRequest { struct IORequest ahir_Std; int ahir_Version, ahir_Type, ahir_Frequency, ahir_Volume, ahir_Position; struct AHIRequest* ahir_Link; };
struct ExecBase { ULONG ex_EClockFrequency; };
extern struct ExecBase* SysBase;
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define MEMF_CHIP 4
#define CMD_WRITE 3
#define ADIOF_NOWAIT 64
#define ADIOF_PERVOL 16
#define ADALLOC_MINPREC -128
#define AUDIONAME "audio.device"
#define AHINAME "ahi.device"
#define AHI_DEFAULT_UNIT 0
#define AHIST_S16S 1
#define NP_Entry 1
#define NP_Name 2
#define NP_Priority 3
#define NP_StackSize 4
#define TAG_DONE 0
struct MsgPort* CreateMsgPort(void);
void DeleteMsgPort(struct MsgPort*);
struct IORequest* CreateIORequest(struct MsgPort*, unsigned);
void DeleteIORequest(struct IORequest*);
void* AllocMem(unsigned, unsigned);
void FreeMem(void*, unsigned);
int OpenDevice(STRPTR, int, struct IORequest*, int);
void CloseDevice(struct IORequest*);
void Signal(struct Task*, ULONG);
ULONG Wait(ULONG);
void SendIO(struct IORequest*);
void BeginIO(struct IORequest*);
int WaitIO(struct IORequest*);
void AbortIO(struct IORequest*);
struct Task* FindTask(void*);
LONG AllocSignal(int);
void FreeSignal(LONG);
void* CreateNewProcTags(ULONG, ...);
LONG GetVar(STRPTR, STRPTR, int, int);
