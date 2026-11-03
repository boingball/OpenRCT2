/* SPDX-License-Identifier: GPL-3.0-only
 * Lazy AmiSSL crypto for the existing OpenRCT2 authentication protocol. */
#if defined(__amigaos__) && !defined(DISABLE_NETWORK)
#include <proto/exec.h>
#include <utility/tagitem.h>
#include <libraries/amisslmaster.h>
#include <proto/amisslmaster.h>
#include <amissl/tags.h>
#include <stabs.h>
struct Library* AmiSSLMasterBase;
struct Library* AmiSSLBase;
struct Library* AmiSSLExtBase;
void amiga_crypto_close(void) __attribute__((used));
void amiga_crypto_close(void)
{
    if (AmiSSLBase) CloseAmiSSL();
    AmiSSLBase=AmiSSLExtBase=NULL;
    if (AmiSSLMasterBase) CloseLibrary(AmiSSLMasterBase);
    AmiSSLMasterBase=NULL;
}
int amiga_crypto_init(void)
{
    struct TagItem tags[] = {
        {AmiSSL_UsesOpenSSLStructs,FALSE},
        {AmiSSL_GetAmiSSLBase,(ULONG)&AmiSSLBase},
        {AmiSSL_GetAmiSSLExtBase,(ULONG)&AmiSSLExtBase},
        {TAG_DONE,0}
    };
    if (AmiSSLBase) return 1;
    AmiSSLMasterBase=OpenLibrary("amisslmaster.library",5);
    if (AmiSSLMasterBase && OpenAmiSSLTagList(AMISSL_V3xx,tags)==0) return 1;
    amiga_crypto_close(); return 0;
}
ADD2EXIT(amiga_crypto_close,-40);
#endif
