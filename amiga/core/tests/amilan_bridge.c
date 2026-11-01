/* SPDX-License-Identifier: GPL-3.0-only
 * Run the actual m68k register bridge against a synthetic library vector. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <amilan/amiga.h>
unsigned long bridge_seen[15];
extern void bridge_vector(void);
int main(void)
{
    uint16_t vector[3];long regs[14];unsigned i;uintptr_t target=(uintptr_t)bridge_vector;
    vector[0]=0x4ef9;vector[1]=(uint16_t)(target>>16);vector[2]=(uint16_t)target;
    for(i=0;i<14;++i)regs[i]=(long)(0x10203000UL+i);
    long result=os_call((unsigned char*)vector+6,-6,regs);
    assert(result==0x12345678);
    for(i=0;i<14;++i)assert(bridge_seen[i]==(unsigned long)regs[i]);
    assert(bridge_seen[14]==(uintptr_t)((unsigned char*)vector+6));
    puts("AmiLAN m68k bridge: actual library vector, all d0-d7/a0-a5 arguments, a6 base and return value PASS");
}
