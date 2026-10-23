#!/usr/bin/env python3
"""Generate typed ARM producer entry points; no resource data or opcode stream."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
rows=[]
def row(name,args,body,values=None,span=1,triangle=False):
 args=args.split(',') if args else []
 rows.append(dict(name=name,args=args,body=body,values=values or args,span=span,triangle=triangle))
def method(name,args,target=None,values=None,span=1,triangle=False):
 params=args.split(',') if args else []
 row(name,args,'g.'+(target or name.replace('gDP','GfxDp',1).replace('gSP','GfxSp',1))+'('+','.join(f'a[{i}]' for i in range(len(params)))+');',values,span,triangle)
row('gSPMatrix','matrix,params','g.GfxSpMatrix(a[1],static_cast<const int32_t*>(g.SegAddr(a[0])));')
row('gSPVertex','vertices,count,first','NativeVertex(g,a[0],a[1],a[2]);')
row('gSPViewport','viewport','g.GfxSpMovememF3d(G_MV_VIEWPORT,0,g.SegAddr(a[0]));')
row('gSPLight','light,index','g.GfxSpMovememF3d((a[1]-1)*2+G_MV_L0,0,g.SegAddr(a[0]));')
row('gSPDisplayList','list','NativeCall(g,a[0],0,false);')
row('gSPDisplayListOffset','list,offset','NativeCall(g,a[0],a[1],false);')
row('gSPBranchList','list','NativeCall(g,a[0],0,true);')
row('gSPDisplayListOTRFilePath','list','NativeCall(g,a[0],0,false);')
row('gSPEndDisplayList','','NativeEnd();')
row('gSPCullDisplayList','first,last','NativeCull(g,a[0],a[1]);')
row('gSPClearGeometryMode','bits','g.GfxSpGeometryMode(a[0],0);')
row('gSPSetGeometryMode','bits','g.GfxSpGeometryMode(0,a[0]);')
method('gSPTexture','sc,tc,level,tile,on',values=['(sc)&65535','(tc)&65535','(level)&7','(tile)&7','(on)&255'])
row('gSPSetOtherMode','cmd,shift,length,data','g.GfxSpSetOtherMode(a[0],a[1],uint64_t(a[2])<<a[3]);',values=['(shift)+((cmd)==G_SETOTHERMODE_H?32:0)','length','data','(cmd)==G_SETOTHERMODE_H?32:0'])
row('gMoveWd','index,offset,data','g.GfxSpMovewordF3d(a[0],a[1],a[2]);',values=['(index)&255','(offset)&65535','data'])
row('gSPPopMatrix','which','g.GfxSpPopMatrix(1);')
row('gSPModifyVertex','vertex,where,value','g.GfxSpModifyVertex(a[0],a[1],a[2]);')
row('gSP1Triangle','v0,v1,v2,flag','NativeTriangle(g,a[0],a[1],a[2],a[3]);',triangle=True)
row('gSP2Triangles','v0,v1,v2,flag0,v3,v4,v5,flag1','NativeTriangle(g,a[0],a[1],a[2],a[3]);NativeTriangle(g,a[4],a[5],a[6],a[7]);',triangle=True)
row('gSP1Quadrangle','v0,v1,v2,v3,flag','NativeQuad(g,a[0],a[1],a[2],a[3],a[4]);',triangle=True)
method('gDPSetTile','fmt,siz,line,tmem,tile,palette,cmt,maskt,shiftt,cms,masks,shifts',values=['(fmt)&7','(siz)&3','(line)&511','(tmem)&511','(tile)&7','(palette)&15','(cmt)&3','(maskt)&15','(shiftt)&15','(cms)&3','(masks)&15','(shifts)&15'])
method('gDPSetTileSize','tile,uls,ult,lrs,lrt',values=['(tile)&7','(uls)&4095','(ult)&4095','(lrs)&4095','(lrt)&4095'])
method('gDPLoadTile','tile,uls,ult,lrs,lrt',values=['(tile)&7','(uls)&4095','(ult)&4095','(lrs)&4095','(lrt)&4095'])
method('gDPLoadBlock','tile,uls,ult,lrs,dxt',values=['(tile)&7','(uls)&4095','(ult)&4095','MIN(lrs,G_TX_LDBLK_MAX_TXL)&4095','(dxt)&4095'])
method('gDPLoadTLUTCmd','tile,count','GfxDpLoadTlut',values=['(tile)&7','(count)&1023'])
row('gDPSetTextureImage','fmt,siz,width,image','NativeTexture(g,a[0],a[1],a[2],a[3]);',values=['(fmt)&7','(siz)&3','(((width)-1)&4095)+1','image'])
row('gDPSetColorImage','fmt,siz,width,image','g.GfxDpSetColorImage(a[0],a[1],a[2],g.SegAddr(a[3]));',values=['(fmt)&7','(siz)&3','((width)-1)&2047','image'])
row('gDPSetDepthImage','image','g.GfxDpSetZImage(g.SegAddr(a[0]));')
for name in ['Env','Blend','Fog','Grayscale']:
 method('gDPSet'+name+'Color','red,green,blue,alpha',values=['(red)&255','(green)&255','(blue)&255','(alpha)&255'])
method('gDPSetPrimColor','minimum,level,red,green,blue,alpha',values=['(minimum)&255','(level)&255','(red)&255','(green)&255','(blue)&255','(alpha)&255'])
method('gDPSetFillColor','color')
method('gDPSetOtherMode','high,low',values=['(high)&0xffffff','low'])
row('gSPGrayscale','enabled','g.mRdp->grayscale=bool(a[0]);')
method('gDPSetScissor','mode,ulx,uly,lrx,lry',values=['(mode)&3',*['((int)((float)('+v+')*4.0f))&4095' for v in ['ulx','uly','lrx','lry']]])
method('gDPSetScissorFrac','mode,ulx,uly,lrx,lry','GfxDpSetScissor',values=['(mode)&3',*['((int)('+v+'))&4095' for v in ['ulx','uly','lrx','lry']]])
method('gDPFillRectangle','ulx,uly,lrx,lry',values=['((uint32_t)('+v+')&1023)*4' for v in ['ulx','uly','lrx','lry']])
method('gDPFillWideRectangle','ulx,uly,lrx,lry','GfxDpFillRectangle',values=['(uint32_t)((int32_t)((uint32_t)('+v+')<<12)>>10)' for v in ['ulx','uly','lrx','lry']],span=2)
rectargs='ulx,uly,lrx,lry,tile,s,t,dsdx,dtdy'
for name,flip,wide in [('gSPTextureRectangle',False,False),('gSPTextureRectangleFlip',True,False),('gSPWideTextureRectangle',False,True)]:
 vals=[('(uint32_t)((int32_t)((uint32_t)('+v+')<<8)>>8)' if wide else '('+v+')&4095') for v in ['ulx','uly','lrx','lry']]+['(tile)&7',*['(uint16_t)('+v+')' for v in ['s','t','dsdx','dtdy']]]
 row(name,rectargs,'g.GfxDpTextureRectangle('+','.join(f'a[{i}]' for i in range(9))+','+str(flip).lower()+');',vals,3)
row('gSPInvalidateTexCache','texture','NativeInvalidate(g,a[0]);')
for name in ['gDPPipeSync','gDPTileSync','gDPLoadSync','gDPFullSync','gDPNoOp','gSPNoOp']:
 row(name,'','(void)g;')
row('gsSPSetFB','framebuffer','NativeSetFramebuffer(g,a[0]);')
row('gsSPResetFB','','NativeResetFramebuffer(g);')
row('gDPSetPrimDepth','depth,delta','g.mRdp->prim_depth=a[0];',values=['(depth)&32767'])
row('gDPSetTextureImageFB','fmt,siz,width,framebuffer','g.Flush();g.mRapi->SelectTextureFb(a[0]);g.mRdp->textures_changed[0]=g.mRdp->textures_changed[1]=false;',values=['framebuffer'])
row('gDPCopyFB','destination,source,once,copied','g.Flush();g.CopyFrameBuffer(a[0],a[1],bool(a[2]),reinterpret_cast<bool*>(a[3]));',values=['(destination)&2047','(source)&2047','(once)&1','copied'])
row('gDPReadFB','source,buffer,ulx,uly,width,height,swap','NativeReadFramebuffer(g,a[0],a[1],a[2],a[3],a[4]);',values=['(source)&255','buffer','(width)&65535','(height)&65535','(swap)&1'],span=2)
row('gDPImageRectangle','x0,y0,s0,t0,x1,y1,s1,t1,tile,width,height','g.GfxDpImageRectangle('+','.join('int16_t(a[%d])'%i for i in range(11))+');',values=['(tile)&7','(uint16_t)(width)','(uint16_t)(height)',*['(uint16_t)('+v+')' for v in ['x0','y0','s0','t0','x1','y1','s1','t1']]],span=3)
# Combine mux selectors become the engine material fields at producer time.
params='a0,b0,c0,d0,Aa0,Ab0,Ac0,Ad0,a1,b1,c1,d1,Aa1,Ab1,Ac1,Ad1'
values=[]
for cyc in (0,1):
 values.append(f'(G_CCMUX_##a{cyc}&15)|((G_CCMUX_##b{cyc}&15)<<4)|((G_CCMUX_##c{cyc}&31)<<8)|((G_CCMUX_##d{cyc}&7)<<13)')
 values.append(f'(G_ACMUX_##Aa{cyc}&7)|((G_ACMUX_##Ab{cyc}&7)<<3)|((G_ACMUX_##Ac{cyc}&7)<<6)|((G_ACMUX_##Ad{cyc}&7)<<9)')
row('gDPSetCombineLERP',params,'g.GfxDpSetCombineMode(a[0],a[1],a[2],a[3]);',values)
header=['// Generated typed native producers. No serialized graphics instructions.','#pragma once','#include <stdint.h>','#include <stddef.h>','#include <libultraship/libultra/types.h>','#include <libultraship/libultra/gbi.h>','#ifdef __cplusplus','extern "C" {','#endif']
cpp=['// Generated typed producer callbacks; see generate_native_emit.py.']
for i,r in enumerate(rows):
 fn='Mk64NativeEmit_'+r['name'];n=len(r['values']);sig='void* packet'+''.join(', uintptr_t p'+str(j) for j in range(n))
 header.append('void '+fn+'('+sig+');')
 cpp.append('static void NativeCallback'+str(i)+'(void* context,const uint32_t* a,size_t) { auto& g=*static_cast<Fast::Interpreter*>(context);(void)a; Mk64NativeProgramTriangleBoundary('+('1' if r['triangle'] else '0')+');'+r['body']+('Mk64NativeProgramTriangleBoundary(-1);' if r['triangle'] else '')+'}')
 cpp.append('extern "C" void '+fn+'('+sig+') {'+('const uint32_t args[]={'+','.join('uint32_t(p'+str(j)+')' for j in range(n))+'};' if n else '')+f'NativeRecord(packet,{r["span"]},NativeCallback{i},'+('args' if n else 'nullptr')+f',{n});'+'}')
header+=['void gSPDisplayList(Gfx*,Gfx*);','void gSPDisplayListOffset(Gfx*,Gfx*,int);','void gSPVertex(Gfx*,uintptr_t,int,int);','void gSPInvalidateTexCache(Gfx*,uintptr_t);','#ifdef __cplusplus','}','#endif','#if !defined(MK64_NATIVE_GBI_IMPLEMENTATION)']
for r in rows:
 if r['name'] in ('gSPDisplayList','gSPDisplayListOffset','gSPVertex','gSPInvalidateTexCache'):continue
 params=','.join(['packet']+r['args']);values=','.join('(uintptr_t)('+v+')' for v in r['values'])
 body='void* nativePacket3DS=(void*)(packet);'+''.join('(void)(packet);' for _ in range(r['span']-1))+'Mk64NativeEmit_'+r['name']+'(nativePacket3DS'+(','+values if values else '')+');'
 header+=['#undef '+r['name'],'#define '+r['name']+'('+params+') do {'+body+'} while(0)']
for leaf in ('gDma0p','gDma1p','gDma2p','gImmp0','gImmp1','gImmp2','gImmp3',
             'gDPNoParam','gDPSetColor','gSetImage','DPRGBColor','gDPLoadTileGeneric',
             '__gSPDisplayList','__gSPVertex','__gSPInvalidateTexCache'):
 header+=['#undef '+leaf,'#define '+leaf+'(...) ((void)sizeof(struct MK64_NATIVE_UNCONVERTED_PRODUCER_'+leaf+'))']
header+=['#endif']
(a.output/'native_emit.generated.h').write_text('\n'.join(header)+'\n');(a.output/'native_emit.generated.inc').write_text('\n'.join(cpp)+'\n');(a.output/'native-emit-manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
