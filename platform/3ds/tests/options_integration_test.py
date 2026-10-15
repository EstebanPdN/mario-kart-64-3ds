#!/usr/bin/env python3
"""Characterize the production bottom-menu input routes with mocked platform calls.

This executes extracted production functions rather than parallel test-only
routing logic. It does not validate Citro2D pixels, GPU ownership or hardware.
"""
from pathlib import Path
import re
import argparse
import subprocess
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[3]
src=root/'platform/3ds/source/bottom_ui_3ds.cpp'
s=src.read_text()
for part in ("options_actions_3ds.inc.cpp", "options_view_3ds.inc.cpp"):
    s += "\n" + (src.parent/part).read_text()
def fn(name):
    match=re.search(r'^[A-Za-z][^\n;{}]*\b'+re.escape(name)+r'\([^;\n]*\)\s*\{',s,re.M)
    assert match, name
    start=match.start(); p=s.index('{',match.start()); depth=0; quote=None; comment=None; escape=False
    for i in range(p,len(s)):
        c=s[i]; nxt=s[i:i+2]
        if comment=='line':
            if c=='\n': comment=None
            continue
        if comment=='block':
            if s[i-1:i+1]=='*/': comment=None
            continue
        if quote:
            if escape: escape=False
            elif c=='\\': escape=True
            elif c==quote: quote=None
            continue
        if nxt=='//': comment='line'; continue
        if nxt=='/*': comment='block'; continue
        if c in ('"',"'"): quote=c; continue
        if c=='{': depth+=1
        elif c=='}':
            depth-=1
            if depth==0:return s[start:i+1]+'\n'
    raise RuntimeError(name)
header=r'''
#include "settings_3ds.h"
#include "bottom_ui_3ds.h"
#include "diagnostics_3ds.h"
#include "game_state_3ds.h"
#include "auto_mode_3ds.h"
#include "solo_modes_3ds.h"
#include "hud_layout_3ds.hpp"
#include "standings_presentation_3ds.hpp"
#include "menu_transition_3ds.hpp"
#include "menu_backdrop_3ds.hpp"
#include "menu_hint_3ds.hpp"
#include "options_layout_3ds.hpp"
#include "art/option_icons_art_3ds.hpp"
#include "input_policy_3ds.hpp"
#include "render_policy_3ds.hpp"
#include "turbo_policy_3ds.hpp"
#include "select_policy_3ds.hpp"
#include "threat_warning_ui_3ds.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
struct C3D_Tex {}; struct Tex3DS_SubTexture {}; struct C3D_RenderTarget {};
struct circlePosition { int16_t dx=0,dy=0; }; struct touchPosition { uint16_t px=0,py=0; };
enum : uint32_t { KEY_A=1<<0,KEY_B=1<<1,KEY_SELECT=1<<2,KEY_START=1<<3,
 KEY_DRIGHT=1<<4,KEY_DLEFT=1<<5,KEY_DUP=1<<6,KEY_DDOWN=1<<7,KEY_R=1<<8,
 KEY_L=1<<9,KEY_X=1<<10,KEY_Y=1<<11,KEY_ZL=1<<14,KEY_ZR=1<<15,KEY_TOUCH=1<<20 };
constexpr int C2D_AlignLeft=0,C2D_AlignCenter=1,C2D_AlignRight=2;
constexpr float kBottomWidth=320,kBottomHeight=240,kOptionsTabY=36,kOptionsRowY=70,kOptionsRowStep=39;
constexpr size_t kFontGlyphCount=42,kRetiredTextureCapacity=8,kFpsHistoryCount=5;
constexpr int kCStickTurboDeadzone=30;
constexpr std::array<uint16_t,6> kVolumeSteps={25,50,75,100,150,200};
static uint64_t tick=1000;
uint64_t osGetTime(){ return tick; } uint64_t svcGetSystemTick(){ return tick*100; }
void hidScanInput(){assert(false);} uint32_t hidKeysHeld(){return 0;} uint32_t hidKeysDown(){return 0;} uint32_t hidKeysUp(){return 0;}
void hidCircleRead(circlePosition*){} void hidCstickRead(circlePosition*){} void hidTouchRead(touchPosition*){}
'''
decls=s[s.index('enum class BaseView'):s.index('constexpr std::array<const char*, kFontGlyphCount>')]
mocks=r'''
static Mk64BottomUIGameState3DS engine{}; static Mk64DiagnosticsInput3DS input{};
static bool autoLocked=false,battleSpectating=false,autoSelect=false,updateOpen=false;
static int randomMenu=0,randomStarts=0,dumps=0,fullDumps=0,cleanDumps=0,ceremonies=0,appliedTurbo=1;
static uint32_t previousInput=0;
extern "C" bool Mk64Diagnostics3DSConsumeInput(Mk64DiagnosticsInput3DS* out){*out=input;return true;}
extern "C" bool Mk64Diagnostics3DSReadInput(Mk64DiagnosticsInput3DS*){return false;}
extern "C" bool Mk64Diagnostics3DSIsNewModel(){return true;}
extern "C" bool Mk64Diagnostics3DSRequestDump(){++dumps;return true;}
extern "C" bool Mk64Diagnostics3DSRequestFullDump(){++fullDumps;return true;}
extern "C" bool Mk64Diagnostics3DSRequestCleanDumps(){++cleanDumps;return true;}
extern "C" void Mk64GameState3DSUpdateHudAnimations(){}
extern "C" void Mk64GameState3DSGetBottomUISnapshot(Mk64BottomUIGameState3DS* out){*out=engine;}
extern "C" void Mk64GameState3DSSetTopHudEnabled(bool){}
extern "C" void Mk64GameState3DSApplyTurbo(bool enabled,uint8_t n){appliedTurbo=enabled?n:1;}
extern "C" bool Mk64GameState3DSPerformPauseAction(Mk64PauseAction3DS){engine.paused=false;return true;}
extern "C" bool Mk64GameState3DSSkipCeremony(){++ceremonies;return true;}
extern "C" bool Mk64Auto3DSRaceInputLocked(){return autoLocked;}
extern "C" bool Mk64SoloBattle3DSSpectating(){return battleSpectating;}
extern "C" void Mk64Auto3DSMenuInput(bool,bool){}
extern "C" void Mk64Auto3DSObserveInput(int,bool){}
extern "C" void Mk64SoloBattle3DSObserveInput(int,bool){}
extern "C" bool Mk64Auto3DSSelect(){return autoSelect;}
extern "C" int Mk64RandomCup3DSMenuKind(){return randomMenu;}
extern "C" bool Mk64RandomCup3DSStart(bool,uint64_t){++randomStarts;return true;}
extern "C" bool Mk64ThreatWarning3DSRead(Mk64ThreatWarning3DS*){return false;}
uint32_t Mk64Graphics3DSBottomHudRefreshDivisor(){return 3;}
bool LoadTexture(const char*,UiTexture&,const char* = nullptr,bool = false){return true;}
void LoadDataTextures(){} void LoadSoftMenuBackground(){} void LoadMenuMaps(){}
void HandleDataInput(const Mk64DiagnosticsInput3DS&){}
bool IdleMenuBackdropActive(){return !sUi.game.racing;}
void OpenUpdate(){updateOpen=true;} void CloseUpdate(){updateOpen=false;}
void HandleUpdateInput(const Mk64DiagnosticsInput3DS&){} bool UpdateIsOpen(){return updateOpen;}
void DrawRaceBackground(float = 255){} void DrawDimMenuBackground(){}
struct Text { std::string text; float x,y,scale; };
struct Rect {float x,y,w,h;};
static std::vector<Text> texts;static std::vector<Rect> rects;static float cursorY=0;static int sDataFontPalette=-1;
void DrawText(const char* t,float x,float y,float scale,uint32_t,int,float,bool = false){texts.push_back({t,x,y,scale});}
void DrawOptionIcon(mk64_3ds::OptionIcon icon,float,float y){if(icon==mk64_3ds::OptionIcon::Hud)cursorY=y+9;}
uint32_t C2D_Color32(int r,int g,int b,int a){return r|(g<<8)|(b<<16)|(uint32_t(a)<<24);}
void C2D_DrawRectSolid(float x,float y,float,float w,float h,uint32_t){rects.push_back({x,y,w,h});}
'''
functions=['GetBaseView','SetStatus','SaveChangedSetting','RowCount','VisibleTabCount','VisibleTab','VisibleTabIndex','OptionsRowY','OpenOptions','CloseOptions','DismissOptions','SetTab','ActivateSelectedRow','PointInside','HandleOptionsTouch','HandleModalInput','DataActive','TabName','OptionValueUnavailable','GetRowText','OptionIconForRow','DrawOptions','Mk64BottomUI3DSPrepareFrame','Mk64BottomUI3DSFilterGameKeys','Mk64BottomUI3DSConsumesCStick','Mk64BottomUI3DSIsModalOpen']
# Forward declarations are necessary because PrepareFrame's original file order includes DataActive later.
prototypes='bool DataActive();\n'
body=''.join(fn(n) for n in functions)
tests=r'''
static void Frame(uint32_t keys=0,int cx=0,int cy=0){
 input={};input.heldMask=keys;input.downMask=keys&~previousInput;input.upMask=previousInput&~keys;
 input.cstickX=cx;input.cstickY=cy;previousInput=keys;tick+=33;Mk64BottomUI3DSPrepareFrame();
}
static void Race(){
 sUi={};sUi.initialized=true;engine={};engine.racing=true;engine.gameState=4;engine.menuSelection=12;
 previousInput=0;autoLocked=battleSpectating=autoSelect=updateOpen=false;randomMenu=0;
 sRenderSliderDragging=sVolumeSliderDragging=sTurboSliderDragging=false;
 Mk64Settings3DSResetDefaults();Mk64Settings3DSSetMenuMaps(false);Frame();
}
int main(){
 Mk64Settings3DSSetHardwareModel(true);Mk64Settings3DSLoad();
 Race();Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_OVERLAY);Mk64Settings3DSSetTurboMultiplier(5);
 Frame(KEY_SELECT|KEY_A|KEY_B|KEY_L|KEY_R,100); // full chord takes priority over overlay
 assert(fullDumps==1&&!Mk64Settings3DSGetOverlayEnabled());
 assert((Mk64BottomUI3DSFilterGameKeys(input.heldMask)&(KEY_L|KEY_R|KEY_SELECT))==0);
 Frame(input.heldMask,100);assert(fullDumps==1);Frame();
 Frame(KEY_SELECT|KEY_A|KEY_R,100);assert(Mk64Settings3DSGetOverlayEnabled());
 assert(!Mk64BottomUI3DSIsModalOpen()&&appliedTurbo==5&&Mk64BottomUI3DSConsumesCStick());
 assert(Mk64BottomUI3DSFilterGameKeys(input.heldMask)==(KEY_A|KEY_R));
 Frame(KEY_A|KEY_B|KEY_L|KEY_R,100);assert(Mk64BottomUI3DSFilterGameKeys(input.heldMask)==input.heldMask&&appliedTurbo==5);
 Frame(KEY_SELECT|KEY_A|KEY_R,100);assert(!Mk64Settings3DSGetOverlayEnabled());Frame();
 Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_DUMP);Frame(KEY_SELECT|KEY_A);assert(dumps==1&&Mk64BottomUI3DSFilterGameKeys(input.heldMask)==KEY_A);
 Frame(KEY_SELECT|KEY_A);assert(dumps==1);Frame();
 for(int binding=0;binding<3;++binding){Mk64Settings3DSSetSelectAction(static_cast<Mk64SelectAction3DS>(binding));Frame(KEY_L|KEY_R|KEY_SELECT);assert(fullDumps==binding+2);Frame();}
 Frame(KEY_ZL|KEY_ZR);assert(appliedTurbo==1);Frame(KEY_ZL|KEY_ZR,100);assert(appliedTurbo==5);Frame();
 // Pause from a live frame, hold A throughout, and resume on START.
 Frame(KEY_A);engine.paused=true;Frame(KEY_A|KEY_START);assert(sUi.modalOpen&&Mk64BottomUI3DSIsModalOpen());
 Frame(KEY_A);Frame(KEY_A|KEY_START);assert(!sUi.modalOpen&&!Mk64BottomUI3DSIsModalOpen());
 assert(Mk64BottomUI3DSFilterGameKeys(input.heldMask)==KEY_A);Frame(KEY_A);assert(Mk64BottomUI3DSFilterGameKeys(KEY_A)==KEY_A);
 // Every Y edge advances the entire preset and persists Custom; hold does not repeat.
 Race();Mk64Settings3DSApplyDisplayProfile(MK64_DISPLAY_PROFILE_3DS_QUALITY);
 for(int n=1;n<=7;++n){Frame(KEY_Y);assert(Mk64Settings3DSGetHudLayout()==(3+n)%5);assert(Mk64Settings3DSGetDisplayProfile()==MK64_DISPLAY_PROFILE_3DS_CUSTOM);Frame(KEY_Y);assert(Mk64Settings3DSGetHudLayout()==(3+n)%5);Frame();}
 // AUTO locks driving, but diagnostic SELECT must remain available.
 Race();autoLocked=true;Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_DUMP);
 int beforeAutoDump=dumps;Frame(KEY_SELECT);assert(dumps==beforeAutoDump+1);
 assert((Mk64BottomUI3DSFilterGameKeys(KEY_A|KEY_SELECT|KEY_START)&~KEY_START)==0);Frame();
 Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_OVERLAY);
 Frame(KEY_SELECT);assert(Mk64Settings3DSGetOverlayEnabled());Frame();
 Frame(KEY_SELECT);assert(!Mk64Settings3DSGetOverlayEnabled());Frame();autoLocked=false;
 // SELECT in contextual menus never captures a dump or in-race overlay.
 engine.racing=false;engine.gameState=2;engine.gameSelectVisible=true;Mk64Settings3DSSetSelectAction(MK64_SELECT_ACTION_3DS_DUMP);
 autoSelect=true;int oldDumps=dumps;Frame(KEY_SELECT);assert(dumps==oldDumps&&(sUi.injectedGameKeys&KEY_A));Frame();
 autoSelect=false;randomMenu=1;Frame(KEY_SELECT);assert(randomStarts==1&&dumps==oldDumps);Frame();
 // Touch and controller activate the real Advanced sub-menu and its back action.
 randomMenu=0;OpenOptions(false);HandleOptionsTouch(150,int(OptionsRowY(OptionsTab::Screen,6)));assert(sUi.displayAdvanced&&RowCount(sUi.tab)==8);
 HandleOptionsTouch(60,66);assert(sUi.displayAdvanced&&sUi.selectedRow==0); // first row, not tab strip
 HandleOptionsTouch(60,int(OptionsRowY(OptionsTab::Screen,7)));assert(!sUi.displayAdvanced&&sUi.selectedRow==6);HandleOptionsTouch(150,int(OptionsRowY(OptionsTab::Screen,6)));
 Mk64DiagnosticsInput3DS back{};back.downMask=KEY_B;HandleModalInput(back);assert(!sUi.displayAdvanced&&sUi.modalOpen);
 SetTab(OptionsTab::Screen);sUi.selectedRow=6;ActivateSelectedRow(1);assert(sUi.displayAdvanced);
 HandleModalInput(back);sUi.selectedRow=2;
 // Artwork choice is persistent and does not alter controls or performance profiles.
 SetTab(OptionsTab::Screen);sUi.selectedRow=6;ActivateSelectedRow(1);
 sUi.selectedRow=6;Mk64Settings3DSSetUiTextures(MK64_UI_TEXTURES_3DS_ORIGINAL);
 ActivateSelectedRow(1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_SCALED);
 ActivateSelectedRow(1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_MODIFIED);
 ActivateSelectedRow(1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_ORIGINAL);
 ActivateSelectedRow(-1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_MODIFIED);
 ActivateSelectedRow(-1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_SCALED);
 ActivateSelectedRow(-1);assert(Mk64Settings3DSGetUiTextures()==MK64_UI_TEXTURES_3DS_ORIGINAL);
 sUi.displayAdvanced=false;
 // Draw the production option rows, with mocked pixels, to compare shared fonts and alignment.
 texts.clear();rects.clear();sUi.selectedRow=2;DrawOptions();
 for(const auto& t:texts)if(t.text=="HUD LAYOUT")assert(t.scale==.62f&&std::abs((t.y+16*t.scale/2)-cursorY)<1);
 assert(rects.size()>7); // Native row recesses now include their original lower edge.
 // Rail, step marks and grip share the row center on every slider page.
 for(auto tab:{OptionsTab::Screen,OptionsTab::Gameplay}){
  SetTab(tab);rects.clear();DrawOptions();int grips=0;
  for(const auto& r:rects)if(r.w==9&&r.h==12){
   const int row=tab==OptionsTab::Screen?3:grips;
   assert(r.y+6==mk64_3ds::OptionRowCenterY(tab==OptionsTab::Screen,row));++grips;
  }
  assert(grips==(tab==OptionsTab::Screen?1:2));
 }
 SetTab(OptionsTab::Game);sUi.modalOpenedFromPause=true;texts.clear();DrawOptions();
 bool foundContinue=false;for(const auto& t:texts)if(t.text=="CONTINUE GAME"){assert(t.scale==.62f);foundContinue=true;}assert(foundContinue);
 SetTab(OptionsTab::Gameplay);HandleOptionsTouch(246,71);assert(Mk64Settings3DSGetTurboMultiplier()==5);
 HandleOptionsTouch(146,71);assert(Mk64Settings3DSGetTurboMultiplier()==2);
 SetTab(OptionsTab::Developer);sUi.selectedRow=3;ActivateSelectedRow(1);assert(Mk64Settings3DSGetSelectAction()==MK64_SELECT_ACTION_3DS_OVERLAY);
 sUi.selectedRow=2;ActivateSelectedRow(1);assert(Mk64Settings3DSGetSelectAction()==MK64_SELECT_ACTION_3DS_DUMP);
 // Updater remains reachable from the Game tab.
 SetTab(OptionsTab::Game);sUi.modalOpenedFromPause=false;sUi.selectedRow=0;ActivateSelectedRow(1);assert(updateOpen);
 CloseUpdate();CloseOptions();engine.gameSelectVisible=false;engine.gameState=5;Frame();Frame(KEY_START);assert(ceremonies==1);
 std::puts("PASS: actual UI routing: nonmodal overlay/Turbo, expanded chord priority/hold guards, held-A resume, Y cycle, AUTO/random contexts, Advanced touch/back, common font/cursor, slider/binding/updater/ceremony actions. Rendering and engine calls are mocked; not console proof.");
}
'''
out=output/'ui_integration.cpp'
out.write_text(header+decls+mocks+prototypes+body+tests)
subprocess.run(['clang++', '-std=c++17', '-g', '-fsanitize=address,undefined',
    '-I', str(root/'platform/3ds/include'), '-I', str(root/'platform/3ds/source'), str(out),
    str(root/'platform/3ds/source/settings_3ds.cpp'), '-o', str(output/'ui_integration')], check=True)
(output/'sdmc:/3ds/MK64').mkdir(parents=True, exist_ok=True)
subprocess.run([str(output/'ui_integration')], cwd=output, check=True)
