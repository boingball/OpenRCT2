/* SPDX-License-Identifier: GPL-3.0-only */
#include <openrct2/core/Crypt.h>
#include <cassert>
#include <cstdio>
#include <exception>
int main()
{
    try
    {
        auto key=OpenRCT2::Crypt::CreateRSAKey();key->Generate();
        auto pub=OpenRCT2::Crypt::CreateRSAKey();pub->SetPublic(key->GetPublic());
        auto copy=OpenRCT2::Crypt::CreateRSAKey();copy->SetPrivate(key->GetPrivate());
        const char msg[]="OpenRCT2 Amiga LAN authentication";
        auto rsa=OpenRCT2::Crypt::CreateRSA();auto signature=rsa->SignData(*copy,msg,sizeof msg);
        assert(rsa->VerifyData(*pub,msg,sizeof msg,signature.data(),signature.size()));
        signature[0]^=1;assert(!rsa->VerifyData(*pub,msg,sizeof msg,signature.data(),signature.size()));
        puts("AmiSSL/OpenRCT2 authentication: RSA generate, PEM roundtrip, sign/verify and tamper rejection PASS");return 0;
    }
    catch(const std::exception& e)
    { std::printf("AmiSSL/OpenRCT2 authentication unavailable/failed: %s\n",e.what());return 10; }
}
