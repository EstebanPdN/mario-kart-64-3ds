#!/usr/bin/env python3
"""Lower embedded C graphics arrays into ordinary native procedures."""
import argparse,re,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
inputs=['harbour_masters.c','racing/skybox_and_splitscreen.c','ending/dl_unk_80284EE0.c']
def skip_space(s,i):
 while i<len(s):
  if s[i].isspace() or s[i]==',':i+=1
  elif s.startswith('//',i):i=s.index('\n',i)+1
  elif s.startswith('/*',i):i=s.index('*/',i)+2
  else:break
 return i
def end_group(s,i,op,cl):
 assert s[i]==op;depth=1;i+=1
 while depth:
  if s.startswith('//',i):i=s.index('\n',i)+1;continue
  if s.startswith('/*',i):i=s.index('*/',i)+2;continue
  if s[i] in '\"\'':
   quote=s[i];i+=1
   while s[i]!=quote:
    if s[i]=='\\':i+=1
    i+=1
  elif s[i]==op:depth+=1
  elif s[i]==cl:depth-=1
  i+=1
 return i
registry=['#include <stdint.h>','#include <stddef.h>','extern "C" {'];exports=[];report=[]
for module,relative in enumerate(inputs):
 path=(a.root/'src'/relative).resolve();s=path.read_text();functions=[];arrays=[]
 for m in re.finditer(r'\bGfx\s+([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{',s):
  name=m[1];begin=m.end()-1;end=end_group(s,begin,'{','}');body=s[begin+1:end-1];calls=[];at=0
  while (at:=skip_space(body,at))<len(body):
   call=re.match(r'(gs\w+)\s*\(',body[at:]);assert call,(relative,name,body[at:at+80]);fn=call[1];brace=at+call.end()-1;last=end_group(body,brace,'(',')');args=body[brace+1:last-1]
   assert fn not in ('gsSPSetFB','gsSPResetFB'),'These names are dynamic GBI macros, not initializers'
   calls.append((fn,args));at=last
  assert calls and calls[-1][0]=='gsSPEndDisplayList',(relative,name,'unterminated')
  assertion='static_assert' if path.suffix=='.cpp' else '_Static_assert'
  functions.append(assertion+'(sizeof('+name+')/sizeof('+name+'[0])=='+str(len(calls))+',\"Embedded native entry mapping changed\");')
  function='Mk64NativeEmbedded_'+str(module)+'_'+name
  functions.append('static void '+function+'(size_t entry) {\n switch(entry) {')
  for index,(fn,args) in enumerate(calls):
   target='g'+fn[2:]
   if fn=='gsSPDisplayListOTRFilePath':target='gSPDisplayList'
   functions+=[' case '+str(index)+': '+target+'(NULL'+(', '+args if args.strip() else '')+'); if(Mk64NativeListStopped())return;']
  functions+=[' break; default: break; }\n}']
  arrays.append((name,function,len(calls)))
  report.append({'source':relative,'array':name,'entries':len(calls),'families':sorted({fn for fn,args in calls})})
 export='Mk64NativeEmbeddedModule'+str(module);functions.append('int '+export+'(const void* pointer) { const uintptr_t address=(uintptr_t)pointer;')
 for name,fn,count in arrays:
  functions.append(' if(address>=(uintptr_t)'+name+' && address<(uintptr_t)('+name+'+sizeof('+name+')/sizeof('+name+'[0]))) { '+fn+'((address-(uintptr_t)'+name+')/sizeof('+name+'[0]));return 1;}')
 functions.append(' return 0; }')
 # C wrappers and C++ wrappers export the same unambiguous C ABI.
 wrapper=['#include "native_emit.generated.h"','#include "native_dynamic_3ds.h"','#include "'+str(path)+'"']
 if path.suffix=='.cpp':wrapper+=['extern "C" {']
 wrapper+=functions
 if path.suffix=='.cpp':wrapper+=['}']
 out=a.output/(f'native-embedded-{module}'+path.suffix);out.write_text('\n'.join(wrapper)+'\n')
 registry.append('int '+export+'(const void*);');exports.append(export)
registry+=['}','extern "C" bool Mk64NativeEmbeddedDraw(const void* pointer) {']+[f' if({name}(pointer))return true;' for name in exports]+[' return false;','}']
(a.output/'native_embedded_registry.cpp').write_text('\n'.join(registry)+'\n');(a.output/'native-embedded-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
