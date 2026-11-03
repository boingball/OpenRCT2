#include "amilan/protocol.h"
void amilan_ipv4_text(char out[16],const uint8_t ip[4])
{
    int i;char *s=out;for(i=0;i<4;i++){unsigned n=ip[i];if(n>=100)*s++=(char)('0'+n/100);if(n>=10)*s++=(char)('0'+n/10%10);*s++=(char)('0'+n%10);if(i<3)*s++='.';}*s=0;
}

