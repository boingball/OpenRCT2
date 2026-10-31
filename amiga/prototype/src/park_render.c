/* SPDX-License-Identifier: GPL-3.0-only
 * View-only projection of imported engine terrain/path data. No simulation.
 */
#include "runner.h"
typedef struct { int x,y; } Point;
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
static void quad(Canvas *c,const Point p[4],uint8_t colour) {
    int y,lo=p[0].y,hi=lo,i;
    for(i=1;i<4;i++){lo=min(lo,p[i].y);hi=max(hi,p[i].y);}
    lo=max(lo,c->top); hi=min(hi,c->bottom-1);
    for(y=lo;y<=hi;y++) {
        int left=100000,right=-100000;
        for(i=0;i<4;i++) {
            Point a=p[i],b=p[(i+1)&3]; int x;
            if(a.y==b.y) {if(y==a.y){left=min(left,min(a.x,b.x));right=max(right,max(a.x,b.x));}continue;}
            if(y<min(a.y,b.y)||y>max(a.y,b.y))continue;
            x=a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y);
            left=min(left,x);right=max(right,x);
        }
        if(right>=left)runner_fill(c,left,y,right-left+1,1,colour);
    }
}
static void line(Canvas *c,Point a,Point b,uint8_t colour) {
    int dx=b.x-a.x,dy=b.y-a.y,n=max(dx<0?-dx:dx,dy<0?-dy:dy),i;
    if(!n){runner_fill(c,a.x,a.y,1,1,colour);return;}
    for(i=0;i<=n;i++)runner_fill(c,a.x+dx*i/n,a.y+dy*i/n,1,1,colour);
}
static void path_rect(Canvas *c,int x,int y,int z,const uint8_t *r,int u0,int v0,int u1,int v1,uint8_t colour) {
    int u[4]={u0,u1,u1,u0},v[4]={v0,v0,v1,v1},i;Point p[4];
    for(i=0;i<4;i++) {
        int rise=0;
        if(r[4]&8) switch(r[4]&3){case 0:rise=(32-u[i])/2;break;case 1:rise=v[i]/2;break;case 2:rise=u[i]/2;break;default:rise=(32-v[i])/2;break;}
        p[i]=(Point){x+(u[i]-v[i])/z,y+((u[i]+v[i]-32)/2-rise)/z};
    }
    quad(c,p,colour);
}
static void water(Canvas *c,int x,int y,int z,const uint8_t *r) {
    int row,hw=32/z,hh=16/z,top=(r[3]&3)*16,right=((r[3]>>2)&3)*16;
    int bottom=((r[3]>>4)&3)*16,left=(r[3]>>6)*16,level=((int)r[5]-(int)r[1])*8;
    if(level<=0)return;
    y-=level/z;
    for(row=-hh;row<=hh;row++) {
        int half=hw-(row<0?-row:row)*2,dx;
        if(y+row<c->top||y+row>=c->bottom)continue;
        for(dx=-half;dx<=half;dx++) {
            int u=16+dx*z/2+row*z,v=16-dx*z/2+row*z,height;
            if(u>=v)height=top+(right-top)*u/32+(bottom-right)*v/32;
            else height=top+(left-top)*v/32+(bottom-left)*u/32;
            if(level>height)runner_fill(c,x+dx,y+row,1,1,2);
        }
    }
}
static const uint8_t *surface_at(const ParkMap *map,int x,int y) {
    uint32_t n,end;
    if(x<0||y<0||x>=(int)map->side||y>=(int)map->side)return 0;
    n=park_map_offset(map,(unsigned)(x+y*(int)map->side));end=park_map_offset(map,(unsigned)(x+y*(int)map->side)+1);
    for(;n<end;n++){const uint8_t *p=map->records+n*8;if(!p[0]&&!(p[2]&0x30))return p;}
    return 0;
}
int runner_set_map(Runner *r,const void *data,uint32_t size) {
    uint32_t a,b;
    if(!park_map_open(&r->map,data,size))return 0;
    r->map_zoom=r->map.side<32?1:2;
    r->map_base=2;
    a=park_map_offset(&r->map,(r->map.side/2)*(r->map.side+1));
    b=park_map_offset(&r->map,(r->map.side/2)*(r->map.side+1)+1);
    while(a<b){const uint8_t *p=r->map.records+a++*PARK_MAP_RECORD;if(!p[0])r->map_base=p[1];}
    r->guests=0;r->atlas=0;r->pan_x=r->pan_y=0;r->full_redraw=1;
    return 1;
}
void runner_map_background(Runner *r,Canvas *c) {
    int z=1<<r->map_zoom,side=(int)r->map.side,sum,x;
    int ox=c->width/2+r->pan_x,oy=c->height/2-(side-1)*16/z+r->pan_y;
    for(sum=0;sum<=2*(side-1);sum++)for(x=max(0,sum-side+1);x<side&&x<=sum;x++) {
        int y=sum-x,sx=ox+(x-y)*32/z,plane=oy+sum*16/z;uint32_t n,end;
        if(sx+32/z<0||sx-32/z>=c->width)continue;
        n=park_map_offset(&r->map,(unsigned)(x+y*side));end=park_map_offset(&r->map,(unsigned)(x+y*side)+1);
        for(;n<end;n++) {
            const uint8_t *p=r->map.records+n*PARK_MAP_RECORD;
            int sy=plane-((int)p[1]-r->map_base)*8/z;
            if(p[2]&0x30)continue;
            if(!p[0]) {
                int hw=32/z,hh=16/z,base=plane+(r->map_base-2)*8/z;
                Point top[4]={{sx,sy-hh-(p[3]&3)*16/z},{sx+hw,sy-((p[3]>>2)&3)*16/z},
                    {sx,sy+hh-((p[3]>>4)&3)*16/z},{sx-hw,sy-(p[3]>>6)*16/z}};
                Point front[4];unsigned i;
                if(sy+hh<c->top&&base+hh<c->top)continue;
                if(min(sy-hh-32/z,p[5]>p[1]?plane-((int)p[5]-r->map_base)*8/z-hh:sy)>=c->bottom)continue;
                {
                    const uint8_t *neighbour=surface_at(&r->map,x+1,y);
                    int level=neighbour?neighbour[1]:2;
                    int ny=plane-(level-r->map_base)*8/z;
                    int high=neighbour?(neighbour[3]&3)*16/z:0;
                    int low=neighbour?(neighbour[3]>>6)*16/z:0;
                    front[0]=top[1];front[1]=top[2];
                    front[2]=(Point){sx,max(top[2].y,ny+hh-low)};
                    front[3]=(Point){sx+hw,max(top[1].y,ny-high)};
                    if(front[2].y>top[2].y||front[3].y>top[1].y)quad(c,front,9);
                    neighbour=surface_at(&r->map,x,y+1);level=neighbour?neighbour[1]:2;
                    ny=plane-(level-r->map_base)*8/z;
                    high=neighbour?(neighbour[3]&3)*16/z:0;low=neighbour?((neighbour[3]>>2)&3)*16/z:0;
                    front[0]=top[2];front[1]=top[3];
                    front[2]=(Point){sx-hw,max(top[3].y,ny-high)};
                    front[3]=(Point){sx,max(top[2].y,ny+hh-low)};
                    if(front[2].y>top[3].y||front[3].y>top[2].y)quad(c,front,8);
                }
                if(!r->map_zoom&&!p[3]&&!p[4]&&r->g1.count) {
                    if(!g1_draw(&r->g1,1915,c,sx,sy,0))quad(c,top,6);
                } else {
                    uint8_t colour=8;
                    if(p[4]==0||p[4]==6)colour=(x^y)&1?6:7;
                    else if(p[4]==3)colour=9;
                    else if(p[4]==7)colour=3;
                    else if(p[4]==4||p[4]==12)colour=4;
                    else if(p[4]==5||(p[4]>=8&&p[4]<=11))colour=(x^y)&1?8:9;
                    quad(c,top,colour);
                }
                water(c,sx,sy,z,p);
                if(r->grid)for(i=0;i<4;i++)line(c,top[i],top[(i+1)&3],9);
            } else {
                uint8_t colour=(p[4]&4)?5:8;unsigned corners=p[3]>>4;
                if(sy+16/z<c->top||sy-32/z>=c->bottom)continue;
                path_rect(c,sx,sy,z,p,10,10,22,22,colour);
                if(p[3]&1)path_rect(c,sx,sy,z,p,0,10,16,22,colour);
                if(p[3]&2)path_rect(c,sx,sy,z,p,10,16,22,32,colour);
                if(p[3]&4)path_rect(c,sx,sy,z,p,16,10,32,22,colour);
                if(p[3]&8)path_rect(c,sx,sy,z,p,10,0,22,16,colour);
                if(corners&1)path_rect(c,sx,sy,z,p,0,16,16,32,colour);
                if(corners&2)path_rect(c,sx,sy,z,p,16,16,32,32,colour);
                if(corners&4)path_rect(c,sx,sy,z,p,16,0,32,16,colour);
                if(corners&8)path_rect(c,sx,sy,z,p,0,0,16,16,colour);
            }
        }
    }
}
