#include "presentation_fps_3ds.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using mk64_3ds::PresentationFps;
static void near(float a,float b){assert(std::fabs(a-b)<.001f);}
int main(){
 PresentationFps f;assert(!f.Record(0,60000));near(f.OneSecond(),0);
 for(unsigned i=1;i<60;++i){assert(!f.Record(i*1000,60000));near(f.OneSecond(),0);}
 assert(f.Record(60000,60000));near(f.OneSecond(),60);near(f.Instant(),60);near(f.Average(),60);
 // Rendering at 30 while simulation could still be 60 counts 30 actual images.
 for(unsigned i=1;i<30;++i){assert(!f.Record(60000+i*2000,60000));near(f.OneSecond(),60);}
 assert(f.Record(120000,60000));near(f.OneSecond(),30);near(f.Instant(),30);near(f.Average(),45);
 // A long frame counts in full; do not hide a stall with an unweighted average.
 assert(f.Record(240000,60000));near(f.OneSecond(),.5f);near(f.Instant(),.5f);near(f.Average(),22.75f);
 f.Reset();assert(!f.Record(1000000,60000));near(f.OneSecond(),0);near(f.Instant(),0);
 for(unsigned i=1;i<=660;++i)f.Record(1000000+i*1000,60000);
 near(f.OneSecond(),60);near(f.Average(),60); // History wraps without accumulating old scenes.
 assert(!f.Record(1660000,60000));near(f.Instant(),60); // Duplicate timestamp adds no image.
 assert(!f.Record(1,60000));near(f.OneSecond(),0); // Backwards clock invalidates the epoch.
 std::puts("PASS: one-second publication, presented image rates, weighted history, stalls, reset and clock guards.");
}
