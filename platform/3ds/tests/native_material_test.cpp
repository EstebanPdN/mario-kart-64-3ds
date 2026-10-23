// Reference equations are host-test data only; production draws consume typed native state.
#include "native_material_3ds.hpp"
#include "depth_precision_3ds.hpp"
#include "alpha_policy_3ds.hpp"
#include <cassert>
#include <random>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
using namespace mk64_3ds::native_scene;
using Color=std::array<double,4>;
struct Fixture{uint32_t geometry,low,high,a,b,kind;RasterState raster;};static_assert(sizeof(Fixture)==32);
RasterState ReferenceRaster(unsigned geometry,unsigned low,unsigned high){
 bool alpha=(((low>>20)&3)==1&&((low>>16)&3)==0)||(((low>>22)&3)==1&&((low>>18)&3)==0);
 const auto state=mk64_3ds::ResolveFast3DAlpha(alpha,low&0x1000,(low&3)==1);
 return {uint8_t((geometry&1)&&(low&0x10)),uint8_t(bool(low&0x20)),uint8_t((low&0xc00)==0xc00),uint8_t(state.alphaTest),mk64_3ds::Fast3DAlphaReference(state.cutout),uint8_t(state.blend),uint8_t(bool(high&(3u<<12))),uint8_t(alpha||bool(low&0x1000))};
}
Color Original(const Fixture& f,const Color& tex,const Color& shade){Color previous{};
 auto rgb=[&](unsigned input,unsigned channel,bool multiplier){if(multiplier&&input==8)return tex[3];switch(input){case 0:return previous[channel];case 1:return tex[channel];case 4:return shade[channel];case 6:return 1.;case 7:case 15:case 31:return 0.;default:assert(false);return 0.;}};
 auto alpha=[&](unsigned input){switch(input){case 0:return previous[3];case 1:return tex[3];case 4:return shade[3];case 6:return 1.;case 7:return 0.;default:assert(false);return 0.;}};
 for(unsigned cycle=0;cycle<(((f.high>>20)&3)==1?2u:1u);++cycle){Color next{};const unsigned a=cycle?(f.a>>5)&15:(f.a>>20)&15,b=cycle?(f.b>>24)&15:(f.b>>28)&15,c=cycle?f.a&31:(f.a>>15)&31,d=cycle?(f.b>>6)&7:(f.b>>15)&7;
 for(unsigned k=0;k<3;++k)next[k]=std::clamp((rgb(a,k,false)-rgb(b,k,false))*rgb(c,k,true)+rgb(d,k,false),0.,1.);
 const unsigned aa=cycle?(f.b>>21)&7:(f.a>>12)&7,ab=cycle?(f.b>>3)&7:(f.b>>12)&7,ac=cycle?(f.b>>18)&7:(f.a>>9)&7,ad=cycle?f.b&7:(f.b>>9)&7;
 next[3]=std::clamp((alpha(aa)-alpha(ab))*alpha(ac)+alpha(ad),0.,1.);previous=next;}
 const bool originalAlpha=(((f.low>>20)&3)==1&&((f.low>>16)&3)==0)||(((f.low>>22)&3)==1&&((f.low>>18)&3)==0);if(!originalAlpha&&!(f.low&0x1000))previous[3]=1.;return previous;
}
Color Native(const Color& tex,const Color& shade){Color previous={1,1,1,1};for(const auto& e:mock::env){Color next{};for(unsigned channel=0;channel<4;++channel){const unsigned m=channel==3;double value[3]{};const unsigned count=e.func[m]==GPU_REPLACE?1:(e.func[m]==GPU_INTERPOLATE?3:2);
 for(unsigned n=0;n<count;++n){const unsigned component=(!m&&e.op[n]==GPU_TEVOP_RGB_SRC_ALPHA)?3:channel;switch(e.src[m][n]){case GPU_PREVIOUS:value[n]=previous[component];break;case GPU_TEXTURE0:value[n]=tex[component];break;case GPU_PRIMARY_COLOR:value[n]=shade[component];break;case GPU_CONSTANT:value[n]=double((e.color>>(component*8))&255)/255;break;default:assert(false);}if(!m&&e.op[n]==GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR)value[n]=1-value[n];}
 switch(e.func[m]){case GPU_REPLACE:next[channel]=value[0];break;case GPU_MODULATE:next[channel]=value[0]*value[1];break;case GPU_INTERPOLATE:next[channel]=value[0]*value[2]+value[1]*(1-value[2]);break;default:assert(false);}next[channel]=std::clamp(next[channel],0.,1.);}previous=next;}return previous;}
int main(){std::vector<Fixture> rows;
// Encode common combiner equations from public field semantics, without any
// game assets or extracted display-list data.
using Terms=std::array<uint32_t,4>;
const auto encode=[](Terms r0,Terms a0,Terms r1,Terms a1,unsigned kind){
 const uint32_t w0=0xfc000000u|(r0[0]<<20)|(r0[2]<<15)|(a0[0]<<12)|(a0[2]<<9)|(r1[0]<<5)|r1[2];
 const uint32_t w1=(r0[1]<<28)|(r1[1]<<24)|(a1[0]<<21)|(a1[2]<<18)|(r0[3]<<15)|(a0[1]<<12)|(a0[3]<<9)|(r1[3]<<6)|(a1[1]<<3)|a1[3];
 return std::array<uint32_t,3>{w0,w1,kind};
};
const Terms shade{15,15,31,4},modulate{1,15,4,7},textureRgb{15,15,31,1},decal{1,4,8,4},passRgb{15,15,31,0};
const Terms shadeAlpha{7,7,7,4},textureAlpha{7,7,7,1},productAlpha{1,7,4,7},passAlpha{7,7,7,0};
const std::array<uint32_t,3> combiners[]={
 encode(shade,shadeAlpha,shade,shadeAlpha,0),
 encode(modulate,productAlpha,modulate,productAlpha,1),
 encode(modulate,textureAlpha,modulate,textureAlpha,2),
 encode(modulate,shadeAlpha,modulate,shadeAlpha,3),
 encode(modulate,shadeAlpha,passRgb,passAlpha,3),
 encode(textureRgb,textureAlpha,textureRgb,textureAlpha,4),
 encode(textureRgb,textureAlpha,passRgb,passAlpha,4),
 encode(textureRgb,shadeAlpha,textureRgb,shadeAlpha,5),
 encode(decal,shadeAlpha,decal,shadeAlpha,6)};
for(const auto& words:combiners)for(uint32_t low:{0x00553078u,0x00552078u,0x004045d8u,0x005049f8u,0x005049d8u,0x00504dd8u,0x00504b50u})for(unsigned compare:{0u,1u})for(uint32_t geometry:{0x204u,0x205u,0x1204u,0x2205u})for(uint32_t high:{0x80000u,0x82000u,0x180000u,0x182000u}){
 Fixture f{geometry,(low&~3u)|compare,high,words[0],words[1],words[2],{}};f.raster=ReferenceRaster(geometry,f.low,high);rows.push_back(f);
}
MaterialBinder binder(4,5);C3D_Tex texture;assert(C3D_TexInit(&texture,8,8,GPU_RGBA8));std::mt19937 random(0x210064);uint64_t fixtures=0,channels=0;
for(const auto& f:rows){const auto expected=ReferenceRaster(f.geometry,f.low,f.high);assert(!std::memcmp(&expected,&f.raster,8)&&ValidRasterState(f.raster));Material material{};material.kind=f.kind;material.cull=(f.geometry>>12)&3;material.wrapS=material.wrapT=2;material.uvClamp[0]=material.uvClamp[1]=1e6;
 assert(material.cull<3);assert(binder.Apply(material,&texture,&f.raster));assert(mock::depth[0]==f.raster.depthTest&&mock::depth[1]==unsigned(f.raster.depthTest?(f.raster.decal?GPU_GEQUAL:GPU_GREATER):GPU_ALWAYS)&&mock::depth[2]==(GPU_WRITE_COLOR|(f.raster.depthWrite?GPU_WRITE_DEPTH:0)));assert(mock::depthMap[0]==1&&mock::depthMap[1]==-1&&mock::depthMap[2]==mk64_3ds::PicaDecalBias(f.raster.decal));
 assert(mock::raster[0]==f.raster.alphaTest&&mock::raster[1]==GPU_GREATER&&mock::raster[2]==f.raster.alphaReference);assert(mock::raster[3]==(f.raster.blend?GPU_SRC_ALPHA:GPU_ONE)&&mock::raster[4]==(f.raster.blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO));assert(texture.filter==(f.raster.linearFilter?GPU_LINEAR:GPU_NEAREST));
 const auto env=mock::envCalls,depth=mock::depthCalls,filter=mock::filterCalls,alpha=mock::alphaCalls;assert(binder.Apply(material,&texture,&f.raster));assert(mock::envCalls==env&&mock::depthCalls==depth&&mock::filterCalls==filter&&mock::alphaCalls==alpha);
 for(unsigned sample=0;sample<128;++sample){Color t,s;for(unsigned k=0;k<4;++k){t[k]=(random()%256)/255.;s[k]=(random()%256)/255.;}const auto a=Original(f,t,s),b=Native(t,s);for(unsigned k=0;k<4;++k){assert(std::fabs(a[k]-b[k])<1e-12);++channels;}}
 // Depth-only and sampler-only changes do not rebuild color/alpha stages.
 auto changed=f.raster;changed.depthWrite^=1;binder.Apply(material,&texture,&changed);assert(mock::envCalls==env&&mock::depthCalls==depth+1);const auto afterDepth=mock::depthCalls;changed.linearFilter^=1;binder.Apply(material,&texture,&changed);assert(mock::envCalls==env&&mock::depthCalls==afterDepth&&mock::filterCalls==filter+1);
 changed.alphaOutput^=1;binder.Apply(material,&texture,&changed);
 assert(mock::envCalls==env+6&&mock::depthCalls==afterDepth&&mock::filterCalls==filter+1);
 assert((mock::env[0].src[1][0]!=GPU_CONSTANT)==bool(changed.alphaOutput));
 binder.Reset();++fixtures;
}
// Adjacent technique switches must reset operands as well as functions; a
// decal's texture-alpha interpolation must not leak into a later modulation.
for(bool reverse:{false,true})for(unsigned index=0;index<std::size(combiners);++index) {
 const auto& words=combiners[reverse?std::size(combiners)-1-index:index];
 Fixture f{0x2205,0x005049d8,0x82000,words[0],words[1],words[2],{}};
 f.raster=ReferenceRaster(f.geometry,f.low,f.high);Material material{};material.kind=f.kind;material.cull=2;
 assert(binder.Apply(material,&texture,&f.raster));
 const Color tex={.1,.4,.7,.35},shade={.3,.8,.2,.6};const auto a=Original(f,tex,shade),b=Native(tex,shade);
 for(unsigned channel=0;channel<4;++channel)assert(std::fabs(a[channel]-b[channel])<1e-12);
}
Material hidden{};hidden.cull=3;RasterState hiddenState;
const auto envCalls=mock::envCalls,depthCalls=mock::depthCalls,filterCalls=mock::filterCalls;
assert(!binder.Apply(hidden,&texture,&hiddenState));
assert(mock::envCalls==envCalls&&mock::depthCalls==depthCalls&&mock::filterCalls==filterCalls);
assert(fixtures==rows.size());C3D_TexDelete(&texture);std::printf("PASS %llu asset-free source/native raster fixtures and %llu TEV channels: seven materials, one/two source cycles, depth/write/decal/alpha threshold/blend/filter, actual MaterialBinder SDK calls, repeated-state cache hits and independent depth/filter changes. Ideal host algebra/API only; no PICA pixels or speed claim.\n",(unsigned long long)fixtures,(unsigned long long)channels);
}
