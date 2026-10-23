#include "native_frame_queue_3ds.hpp"
#include <cassert>
#include <stdexcept>
#include <vector>
using Queue=mk64_3ds::NativeFrameQueue<8,16,40>;
struct State {Queue* queue;std::vector<unsigned> values;bool rejectMutation=false;};
void Record(void* ctx,const uint32_t* args,size_t count){auto& state=*static_cast<State*>(ctx);assert(count==1);state.values.push_back(args[0]);if(state.rejectMutation){assert(!state.queue->Begin(1));assert(!state.queue->Seal());assert(!state.queue->Replay(ctx));assert(!state.queue->Append(0,0,Record,args,count));assert(!state.queue->Failed());}}
void Throw(void*,const uint32_t*,size_t){throw std::runtime_error("deliberate callback failure");}
int main(){
 Queue q;State s{&q};unsigned a=1,b=2,c=3,d=4;
 assert(q.Begin(4));assert(q.Append(0,0,Record,&a,1));assert(q.Append(0,1,Record,&b,1));assert(q.Append(1,2,Record,&c,1));assert(q.Append(3,1,Record,&d,1));
 a=999;assert(q.Seal());s.rejectMutation=true;assert(q.Replay(&s));assert((s.values==std::vector<unsigned>{1,2,3,4}));
 assert(q.Begin(4));assert(q.Append(0,1,Record,&b,1));assert(!q.Seal() && q.MissingWord()==1 && q.Failed());assert(!q.Replay(&s));
 assert(q.Begin(4));assert(q.Append(0,2,Record,&b,1));assert(q.Append(1,1,Record,&b,1));assert(!q.Seal() && q.MissingWord()==1);
 assert(q.Begin(1));assert(q.Append(0,1,Record,&b,1));assert(!q.Append(0,0,Record,&b,1));assert(!q.Seal());
 assert(q.Begin(1));assert(q.Append(0,1,Throw,nullptr,0));assert(q.Seal());try{q.Replay(&s);assert(false);}catch(const std::runtime_error&){}assert(!q.Replay(&s));assert(q.Begin(1));
 assert(!q.Append(1,1,Record,&b,1));assert(q.Begin(1));assert(!q.Append(0,2,Record,&b,1));assert(q.Begin(1));assert(!q.Append(0,1,nullptr,&b,1));
 assert(q.Begin(1));for(unsigned i=0;i<16;++i)assert(q.Append(0,0,Record,&b,1));assert(!q.Append(0,1,Record,&b,1));assert(!q.Seal());
 for(unsigned epoch=0;epoch<10000;++epoch){s.values.clear();assert(q.Begin(2));unsigned x=epoch;assert(q.Append(1,1,Record,&x,1));assert(q.Append(0,1,Record,&b,1));assert(q.Seal());assert(q.Replay(&s));assert((s.values==std::vector<unsigned>{2,epoch}));}
}
