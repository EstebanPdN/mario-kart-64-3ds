#include "native_anchor_queue_3ds.hpp"
#include <cassert>
#include <vector>
int main(){mk64_3ds::NativeAnchorQueue<unsigned,4,8> q;assert(!q.Push(0,1));
 assert(q.Push(0x100,1)&&q.Push(0x100,2)&&q.Push(0x140,3)&&q.Push(0x110,4));assert(!q.Push(0x120,5));
 for(unsigned replay=0;replay<3;++replay){q.BeginReplay();std::vector<unsigned> order;
 assert(!q.Consume(0x200,[&](auto v){order.push_back(v);}));assert(q.Remaining()==4);
 assert(q.Consume(0x100,[&](auto v){order.push_back(v);}));assert(!q.Consume(0x100,[&](auto){assert(false);}));
 assert(q.Consume(0x110,[&](auto v){order.push_back(v);}));assert(q.Consume(0x140,[&](auto v){order.push_back(v);}));
 assert((order==std::vector<unsigned>{1,2,4,3})&&q.Remaining()==0);}
 q.Reset();assert(q.Push(0x100,99));q.Reset();assert(q.Push(0x140,7));q.BeginReplay();
 assert(!q.Consume(0x100,[](auto){assert(false);}));assert(q.Consume(0x140,[](auto v){assert(v==7);}));assert(q.Remaining()==0);
 q.Reset();assert(q.Size()==0&&q.Remaining()==0&&!q.Consume(0x100,[](auto){assert(false);}));
}
