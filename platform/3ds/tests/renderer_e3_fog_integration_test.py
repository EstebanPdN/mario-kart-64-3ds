#!/usr/bin/env python3
"""Real course GBI -> GenerateCC -> factory -> static eligibility -> TEV, with negative controls."""
import argparse
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
port=Path(__file__).resolve().parents[1];root=port.parents[1]
up=root/'third_party/SpaghettiKart/libultraship'
backend=(port/'source/gfx_citro3d.cpp').read_text()
def block(text,marker,start=0):
    begin=text.index(marker,start);end=text.index('{',begin)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[begin:end]

# Real PICA freezes unless each stride equals its attribute and padding bytes.
# Emulators accept the mismatch, so check every packed-vertex buffer layout.
sizes={0:16,1:8,2:8,3:4,4:4,0xC:4,0xD:8,0xE:12,0xF:16}
for stride,count,layout in re.findall(r'BufInfo_Add\([^,]+, *[^,]+, *sizeof\((PackedVertex|StaticFogVertex)\), *(\d+), *(0x[0-9A-Fa-f]+)\)',backend):
    fields=[int(layout,16)>>(4*i)&15 for i in range(int(count))]
    assert sum(sizes[field] for field in fields)=={'PackedVertex':40,'StaticFogVertex':16}[stride],(stride,count,layout)
assert 'sizeof(PackedVertex), 2, 0x10)' not in backend

# Reuse the scalar test's SDK mocks and TEV evaluator; that test remains an
# independent algebra/ramp-channel regression. No production header is mocked.
subprocess.run(['python3',str(port/'tests/renderer_e3_fog_backend_test.py'),'--output',str(out/'scalar')],check=True)
pre=(out/'scalar/fog-tev.cpp').read_text().split('int main(){')[0]
pre=pre.replace('#include "alpha_policy_3ds.hpp"','#include "alpha_policy_3ds.hpp"\n#include "fast/interpreter.h"\n#include "native_geometry_3ds.hpp"\n#include "static_runs_3ds.hpp"\n#include "fast3d_math_3ds.hpp"\nnamespace Fast {',1)
pre=pre.replace('struct Impl {','struct Impl {\n std::unordered_map<uint64_t,std::unique_ptr<ShaderProgram>> shaderPrograms;ShaderProgram* currentProgram=nullptr;',1)
pre=pre.replace(' void ApplyCombinerState(',''' void LoadShader(ShaderProgram* p){state.currentProgram=p;}
 ShaderProgram* CreateAndLoadNewShader(uint64_t,uint64_t);
 void ShaderGetInfo(ShaderProgram*,uint8_t*,bool[2]);
 void ApplyCombinerState(''',1)
pre=pre.replace('unsigned updates=0,binds=0;','unsigned updates=0,binds=0,tevWrites=0;')
for name in ['Init','Src','Func','OpRgb','OpAlpha','Color','BufUpdate','BufColor']:
    original=block(pre,'void C3D_TexEnv'+name+'(')
    pre=pre.replace(original,original.replace('{','{++tevWrites;',1),1)
pre=pre.replace('Color Evaluate(Color primary,Color texture,Color fogSample)','Color Evaluate(Color primary,Color texture,Color fogSample,Color second={0.1f,0.3f,0.9f,0.73f})',1)
pre=pre.replace('case GPU_TEXTURE0:v=texture;break;','case GPU_TEXTURE0:v=texture;break;case GPU_TEXTURE1:v=second;break;',1)
# Apply the real patch pipeline in an isolated directory, never a game build.
driver=out/'apply.cmake'
driver.write_text(f'''set(MK64_3DS_SOURCE_ROOT "{root}")
set(CMAKE_CURRENT_BINARY_DIR "{out}")
set(SPAGHETTIKART_3DS_INTERPRETER_SOURCE "{out}/interpreter.cpp")
file(READ "{up}/src/fast/interpreter.cpp" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
include(platform/3ds/cmake/interpreter_patches_3ds.cmake)
''')
subprocess.run(['cmake','-P',str(driver)],cwd=root,check=True)
interpreter=(out/'interpreter.cpp').read_text()
# Lock the fixture to the actual course source statements, not recollection.
for name,combiner in [('ChocoMountain','MODULATERGB'),('ToadsTurnpike','MODULATEI')]:
    course=(root/f'third_party/SpaghettiKart/src/engine/tracks/{name}.cpp').read_text()
    assert f'gDPSetCombineMode(gDisplayListHead++, G_CC_{combiner}, G_CC_PASS2);' in course
    assert 'gDPSetCycleType(gDisplayListHead++, G_CYC_2CYCLE);' in course

# Extract the exact game GBI macros without mixing its duplicate Gfx typedefs
# into the interpreter's F3DGfx ABI. Both words are then decoded by the actual handler.
gbi=(up/'include/libultraship/libultra/gbi.h').read_text()
mbi=(up/'include/libultraship/libultra/mbi.h').read_text()
def macro(text,name):
    lines=text[text.index('#define '+name):].splitlines();result=[]
    for line in lines:
        result.append(line)
        if not line.rstrip().endswith('\\'):break
    return '\n'.join(result)+'\n'
macros=macro(mbi,'_SHIFTL')+macro(gbi,'G_SETCOMBINE')
for name in ['GCCc0w0','GCCc1w0','GCCc0w1','GCCc1w1','gsDPSetCombineLERP']:
    macros+=macro(gbi,name)
macros+='#define gsDPSetCombineMode(a,b) gsDPSetCombineLERP(a,b)\n'
pre+='using u32=uint32_t;\n'+macros
factory=''
for marker in ['enum ShaderOption : uint8_t','constexpr bool HasOption(', 'struct ChannelPlan {','ChannelPlan SingleOperationPlan(', 'ChannelPlan BuildChannelPlan(', 'ShaderProgram* GfxRenderingAPICitro3D::CreateAndLoadNewShader(', 'void GfxRenderingAPICitro3D::ShaderGetInfo(']:
    code=block(backend,marker)
    factory+=code+(';' if marker.startswith(('enum ','struct ')) else '')+'\n'
# The normalized color/alpha word packers have a commented legacy predecessor.
packers=interpreter.index('void Interpreter::GfxDpSetCombineMode(')
actual=block(interpreter,'void Interpreter::GenerateCC(')+'\n'+block(interpreter,'void Interpreter::GfxDpSetCombineMode(')+'\n'
actual+=block(interpreter,'static inline uint32_t color_comb(',packers)+'\n'+block(interpreter,'static inline uint32_t alpha_comb(',packers)+'\n'
actual+='''Interpreter* mInstance=nullptr;
#define C0(pos,width) ((cmd->words.w0>>(pos))&((1U<<(width))-1))
#define C1(pos,width) ((cmd->words.w1>>(pos))&((1U<<(width))-1))
'''+block(interpreter,'bool gfx_set_combine_handler_rdp(')+'''
#undef C0
#undef C1
Interpreter::Interpreter(){mRsp=new RSP{};mRdp=new RDP{};}
Interpreter::~Interpreter(){delete mRsp;delete mRdp;}
static void Mk64NoteStaticReject3DS(uint32_t){}
'''
actual+=block(interpreter,'struct Mk64StaticMaterial3DS {')+';\nstatic Mk64StaticMaterial3DS sStaticMaterial3DS;\n'
material=block(interpreter,'static void Mk64FillStaticMaterial3DS(')+'\n'
post=r'''
struct Case {const char* name;F3DGfx command;bool alpha;unsigned sampled;bool eligible;float expectedAlpha;bool textureOnly=false;};
int main(){
 constexpr uint64_t options=SHADER_OPT(FOG)|SHADER_OPT(_2CYC);
 Case cases[]={
  {"Choco MODULATERGB/PASS2",{gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2)},false,1,true,1},
  {"Toad MODULATEI/PASS2",{gsDPSetCombineMode(G_CC_MODULATEI,G_CC_PASS2)},false,1,true,1},
  {"Choco MODULATERGB/PASS2 alpha enabled",{gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2)},true,1,true,1},
  {"Toad cutout DECALRGBA/PASS2",{gsDPSetCombineMode(G_CC_DECALRGBA,G_CC_PASS2)},true,1,true,.4f,true},
  {"cycle2 TEXEL0 alpha swaps to physical1",{gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,TEXEL0)},true,3,false,.73f},
  {"cycle2 TEXEL1 alpha swaps to physical0",{gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,TEXEL1)},true,1,true,.4f},
  {"disabled alpha may not exclude fog",{gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,1,0,0,0,COMBINED,0,0,0,TEXEL0)},false,1,true,1},
  {"cycle2 RGB samples both units",{gsDPSetCombineLERP(TEXEL0,0,SHADE,0,0,0,0,1,TEXEL0,0,COMBINED,0,0,0,0,1)},true,3,false,1},
 };
 unsigned eligible=0;
 for(auto test:cases){
  Interpreter interpreter;GfxRenderingAPICitro3D api;mInstance=&interpreter;
  auto* command=&test.command;gfx_set_combine_handler_rdp(&command);
  ColorCombiner comb{};Interpreter::GenerateCC(&comb,{interpreter.mRdp->combine_mode,options|(test.alpha?SHADER_OPT(ALPHA)|SHADER_OPT(TEXTURE_EDGE):0),0});
  auto* program=api.CreateAndLoadNewShader(comb.shader_id0,comb.shader_id1);interpreter.mRenderingState.mShaderProgram=program;
  uint8_t inputs=0;bool layout[2]{};api.ShaderGetInfo(program,&inputs,layout);
  assert(program->twoCycle && layout[0] && layout[1]);
  assert(program->textureOffsets[0]==4 && program->textureOffsets[1]==6 && program->fogOffset==8);
  assert(program->strideFloats==12+inputs*(test.alpha?4:3));
  const unsigned samplerMask=unsigned(program->sampledTextures[0])|unsigned(program->sampledTextures[1])<<1;
  assert(samplerMask==test.sampled);
  interpreter.mRdp->other_mode_l=uint32_t(G_BL_CLR_FOG)<<30;
  interpreter.mRdp->fog_color={73,101,191,77};
  uint32_t dimensions[2]={16,16},tiles[2]={0,1};
  Mk64FillStaticMaterial3DS(&interpreter,&comb,inputs,test.alpha,true,false,0,dimensions,dimensions,dimensions,dimensions,tiles,layout,false);
  assert(sStaticMaterial3DS.ok==test.eligible);
  const Color shade={.3f,.6f,.9f,1},texture={.7f,.5f,.2f,.4f};
  std::array<std::array<float,4>,7> constants{};
  if(test.eligible){
   ++eligible;assert(sStaticMaterial3DS.colorMask[3]==0);
   if(!test.alpha || comb.shader_input_mapping[1][sStaticMaterial3DS.varyingInput]==G_CCMUX_SHADE)assert(sStaticMaterial3DS.color[3]==255);
   for(unsigned i=0;i<inputs;++i){auto* input=sStaticMaterial3DS.vertex+program->inputOffsets[i];for(int c=0;c<4;++c)constants[i][c]=(c==3&&!test.alpha)?1:input[c];}
  }
  const Color fog={73/255.f,101/255.f,191/255.f,.25f};
  api.ApplyCombinerState(program,0,constants,{1,1,1,0},false,fog);
  assert(originalEnabled==test.eligible);
  const unsigned before=tevWrites;
  auto changedFactor=fog;changedFactor[3]=.75f;api.ApplyCombinerState(program,0,constants,{1,1,1,0},false,changedFactor);
  assert(tevWrites==before); // Per-vertex factor is not a TEV constant.
  if(test.eligible){
   auto changedRGB=fog;changedRGB[0]=.2f;api.ApplyCombinerState(program,0,constants,{1,1,1,0},false,changedRGB);assert(tevWrites>before);
   api.ApplyCombinerState(program,0,constants,{1,1,1,0},false,fog);
   for(int factor=0;factor<256;++factor){Color sample={factor/255.f,0,0,1};auto color=Evaluate(shade,texture,sample);
    for(int c=0;c<3;++c){float base=test.textureOnly?texture[c]:texture[c]*shade[c];
     assert(std::fabs(color[c]-(fog[c]*sample[0]+base*(1-sample[0])))<1e-6f);}
    if(test.alpha)assert(std::fabs(color[3]-test.expectedAlpha)<1e-6f);
   }
  }else{auto color=Evaluate(shade,texture,{.8f,0,0,1});if(test.alpha)assert(std::fabs(color[3]-test.expectedAlpha)<1e-6f);}
  std::printf("PASS: %s id=%016llx layout=11 sampled=%u static=%u originalFog=%u stride=%u\n",test.name,(unsigned long long)comb.shader_id0,samplerMask,test.eligible,originalEnabled,program->strideFloats);
 }
 // Factory metadata must ignore disabled cycles and dead operation operands,
 // even though legacy layout intentionally still sees their packed tokens.
 GfxRenderingAPICitro3D api;
 uint64_t dead=uint64_t(SourceTexel1)<<0 | uint64_t(SourceTexel0)<<12 | uint64_t(SourceTexel1)<<32;
 auto* single=api.CreateAndLoadNewShader(dead,SHADER_OPT(FOG));
 assert(single->usedTextures[1] && single->sampledTextures[0] && !single->sampledTextures[1]);
 std::printf("PASS: %u actual factory/material fog cases, 256 factors each, independent texture alpha, RDP swapping and factor-only TEV state reuse; disabled cycles/terms excluded.\n",eligible);
 return 0;
}
} // namespace Fast
int main(){return Fast::main();}
'''
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-DF3DEX_GBI=1','-I',str(port/'include'),'-I',str(port/'probe_stubs'),'-I',str(up/'include')]
for name,tev,mat,success in [
    ('factory-fog',pre,material,True),
    ('negative-layout-gate',pre.replace('!program->sampledTextures[1]','!program->usedTextures[1]'),material.replace('program->sampledTextures[1]','program->usedTextures[1]'),False),
    ('negative-factor-key',pre.replace('PackColor({fogColor[0], fogColor[1], fogColor[2], 0.0f})','PackColor(fogColor)'),material,False),
]:
    cpp=out/(name+'.cpp');exe=out/name;cpp.write_text(tev+factory+actual+mat+post)
    subprocess.run(flags+[str(cpp),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(result.stdout+result.stderr)
    assert (result.returncode==0)==success,f'{name}: {result.stdout}{result.stderr}'
    if success:print(result.stdout,end='')
print('PASS: pre-fix layout gate and varying-alpha fog key each fail their independent negative control.')
