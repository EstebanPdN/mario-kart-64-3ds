#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
// Bounded floor-triangle graph. No per-frame allocation or race-path fallback.
namespace ArenaNavigation3DS {
struct Point { float x, y, z; };
inline float Distance2(Point a, Point b) {
    return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
}
inline Point Lerp(Point a,Point b,float t) {return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline bool SameTriangle(const Point* v,Point a,Point b,Point c) {
    for(Point p:{a,b,c}) {
        bool found=false;
        for(int i=0;i<3;++i)if(p.x==v[i].x && p.y==v[i].y && p.z==v[i].z)found=true;
        if(!found)return false;
    }
    return true;
}
inline bool IntersectsWall(Point a,Point b,const Point* v) {
    // Segment/triangle intersection. Sampling at kart-body height excludes
    // the floor itself and respects walls instead of smoothing through them.
    auto sub=[](Point p,Point q){return Point{p.x-q.x,p.y-q.y,p.z-q.z};};
    auto cross=[](Point p,Point q){return Point{p.y*q.z-p.z*q.y,p.z*q.x-p.x*q.z,p.x*q.y-p.y*q.x};};
    auto dot=[](Point p,Point q){return p.x*q.x+p.y*q.y+p.z*q.z;};
    Point d=sub(b,a),e=sub(v[1],v[0]),f=sub(v[2],v[0]),h=cross(d,f);
    float det=dot(e,h);if(std::fabs(det)<.001f)return false;
    Point s=sub(a,v[0]);float u=dot(s,h)/det;if(u<0 || u>1)return false;
    Point q=cross(s,e);float w=dot(d,q)/det;if(w<0 || u+w>1)return false;
    float t=dot(f,q)/det;return t>.001f && t<.999f;
}
inline float SurfaceDistance2(Point p, const Point* triangle) {
    const Point a=triangle[0],b=triangle[1],c=triangle[2];
    const float det=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
    if (std::fabs(det)<.01f) return Distance2(p,a);
    const float u=((b.z-c.z)*(p.x-c.x)+(c.x-b.x)*(p.z-c.z))/det;
    const float v=((c.z-a.z)*(p.x-c.x)+(a.x-c.x)*(p.z-c.z))/det;
    if(u>=0 && v>=0 && u+v<=1) {
        const float y=u*a.y+v*b.y+(1-u-v)*c.y;
        return (p.y-y)*(p.y-y);
    }
    float best=1e30f;
    for(int i=0;i<3;++i) {
        const auto s=triangle[i],t=triangle[(i+1)%3];
        const float length=Distance2(s,t);
        const float fraction=length>0 ? std::clamp(((p.x-s.x)*(t.x-s.x)+(p.y-s.y)*(t.y-s.y)+(p.z-s.z)*(t.z-s.z))/length,0.f,1.f) : 0;
        best=std::min(best,Distance2(p,{s.x+(t.x-s.x)*fraction,s.y+(t.y-s.y)*fraction,s.z+(t.z-s.z)*fraction}));
    }
    return best;
}
inline float SurfaceHeight(Point p,const Point* v) {
    float det=(v[1].z-v[2].z)*(v[0].x-v[2].x)+(v[2].x-v[1].x)*(v[0].z-v[2].z);
    if(std::fabs(det)<.01f)return -3000;
    float u=((v[1].z-v[2].z)*(p.x-v[2].x)+(v[2].x-v[1].x)*(p.z-v[2].z))/det;
    float w=((v[2].z-v[0].z)*(p.x-v[2].x)+(v[0].x-v[2].x)*(p.z-v[2].z))/det;
    return u>=-.0001f && w>=-.0001f && u+w<=1.0001f ? u*v[0].y+w*v[1].y+(1-u-w)*v[2].y : -3000;
}
class Graph {
public:
    static constexpr int Capacity = 2048;
    struct Node {
        Point vertices[3], center;
        int16_t adjacent[6] = {-1,-1,-1,-1,-1,-1};
        Point portals[6]{};
        int16_t component = -1;
    };
    std::array<Node, Capacity> nodes{};
    // Original arenas have at most 272 wall triangles. Leave bounded headroom
    // without reserving a second full-size floor buffer on Old 3DS.
    static constexpr int WallCapacity = 512;
    std::array<std::array<Point,3>,WallCapacity> walls{};
    int wallCount = 0;
    int count = 0;
    bool overflow = false;
#ifdef MK64_ARENA_TEST_METRICS
    mutable uint64_t supportQueries=0,triangleChecks=0,steerQueries=0;
#endif
    void Reset() { count = wallCount = 0; overflow = false; indexed=false; cachedStart=-1; }
    void AddWall(Point a,Point b,Point c) {
        for(int i=0;i<wallCount;++i)if(SameTriangle(walls[i].data(),a,b,c))return;
        if(wallCount==WallCapacity) {overflow=true;return;}
        walls[wallCount++]={a,b,c};
    }
    bool ClearWalls(Point a,Point b) const {
        a.y+=8;b.y+=8;
        auto blocked=[&](int i) {
            const auto& v=walls[i];
            if(std::max(a.x,b.x)<std::min({v[0].x,v[1].x,v[2].x}) ||
               std::min(a.x,b.x)>std::max({v[0].x,v[1].x,v[2].x}) ||
               std::max(a.z,b.z)<std::min({v[0].z,v[1].z,v[2].z}) ||
               std::min(a.z,b.z)>std::max({v[0].z,v[1].z,v[2].z}))return false;
            return IntersectsWall(a,b,v.data());
        };
        if(!indexed) {
            for(int i=0;i<wallCount;++i)if(blocked(i))return false;
        } else {
            std::array<uint32_t,WallWords> candidates{};
            const int x0=GridX(std::min(a.x,b.x)),x1=GridX(std::max(a.x,b.x));
            const int z0=GridZ(std::min(a.z,b.z)),z1=GridZ(std::max(a.z,b.z));
            for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)
                for(int w=0;w<WallWords;++w)candidates[w]|=wallCells[z*Grid+x][w];
            for(int w=0;w<(wallCount+31)/32;++w)while(candidates[w]) {
                const int i=w*32+__builtin_ctz(candidates[w]);candidates[w]&=candidates[w]-1;
                if(blocked(i))return false;
            }
        }
        return true;
    }
    int ProjectFloor(Point p,Point& floor,float maxDrop=18,int component=-1) const {
#ifdef MK64_ARENA_TEST_METRICS
        ++supportQueries;
#endif
        const int cell=Cell(p);
        if(indexed && cell<0)return -1;
        int result=-1;float best=1e30f;
        auto visit=[&](int i) {
#ifdef MK64_ARENA_TEST_METRICS
            ++triangleChecks;
#endif
            if(component>=0 && nodes[i].component!=component)return;
            const float y=SurfaceHeight(p,nodes[i].vertices);
            const float delta=std::fabs(p.y-y);
            if(y>-2999 && y<=p.y+18 && y>=p.y-maxDrop && delta<best) {
                best=delta;result=i;floor={p.x,y,p.z};
            }
        };
        if(indexed) {
            for(int word=0;word<(count+31)/32;++word) {
                uint32_t bits=floorCells[cell][word];
                while(bits) {
                    int i=word*32+__builtin_ctz(bits);bits&=bits-1;
                    visit(i);
                }
            }
        } else for(int i=0;i<count;++i)visit(i);
        return result;
    }
    bool Supported(Point p,float maxDrop=18) const {
        Point floor{};return ProjectFloor(p,floor,maxDrop)>=0;
    }
    enum ProbeFailure : uint8_t { Safe=0,NoFloor=1,Wall=2,TooLong=3 };
    bool SafeSegment(Point a,Point b,float radius=14,float maxDrop=18,
                     Point* groundedEnd=nullptr,uint8_t* failure=nullptr) const {
        if(failure)*failure=Safe;
        auto fail=[&](uint8_t why){if(failure)*failure=why;return false;};
        float dx=b.x-a.x,dz=b.z-a.z,length=std::sqrt(dx*dx+dz*dz);
        // Fixed bounded work; short look-ahead, never an arena-spanning chord.
        if(length>260)return fail(TooLong);
        int samples=std::max(1,int(std::ceil(length/8)));
        float nx=length>0?dz/length*radius:radius,nz=length>0?-dx/length*radius:0;
        Point floor{};int node=ProjectFloor(a,floor,maxDrop);
        if(node<0)return fail(NoFloor);
        const int component=nodes[node].component;
        const float clearance=std::clamp(a.y-floor.y,0.f,18.f);
        std::array<Point,3> previous{};
        std::array<float,3> previousGround{};
        float previousHeight=floor.y;
        for(int i=0;i<=samples;++i) {
            Point p=Lerp(a,b,float(i)/samples);
            // Follow local ground, not the original kart Y across the entire
            // chord. Each eight-unit step retains the original drop limit.
            p.y=previousHeight;
            node=ProjectFloor(p,floor,maxDrop,component);
            if(node<0)return fail(NoFloor);
            previousHeight=floor.y;
            std::array<Point,3> current{{{p.x,floor.y+clearance,p.z},{},{}}};
            const auto& v=nodes[node].vertices;
            const float det=(v[1].z-v[2].z)*(v[0].x-v[2].x)+(v[2].x-v[1].x)*(v[0].z-v[2].z);
            const int rails=radius>0?3:1;
            for(int side=1;side<rails;++side) {
                const float sign=side==1?1.f:-1.f;
                Point rail{p.x+sign*nx,floor.y,p.z+sign*nz};
                // Predict the same plane under the two tyres. Otherwise a
                // banked road is mistaken for an edge on its high/low side.
                const float u=((v[1].z-v[2].z)*(rail.x-v[2].x)+(v[2].x-v[1].x)*(rail.z-v[2].z))/det;
                const float w=((v[2].z-v[0].z)*(rail.x-v[2].x)+(v[0].x-v[2].x)*(rail.z-v[2].z))/det;
                rail.y=u*v[0].y+w*v[1].y+(1-u-w)*v[2].y;
                Point support{};
                if(ProjectFloor(rail,support,maxDrop,component)<0)return fail(NoFloor);
                current[side]={rail.x,support.y+clearance,rail.z};
            }
            // Ground support can drop at Double Deck's spawn-pad edge before
            // the body actually falls. Do not snap the collision ray through
            // the pad's vertical skirt; retain a bounded descending body ray.
            // The supporting floor/drop checks above remain mandatory.
            for(int side=0;side<rails;++side) {
                const float ground=current[side].y;
                if(i) {
                    if(previousGround[side]-ground>18)current[side].y=previous[side].y;
                    else if(previous[side].y-previousGround[side]>.01f)
                        current[side].y=std::max(ground,previous[side].y-8);
                    if(!ClearWalls(previous[side],current[side]))return fail(Wall);
                }
                previousGround[side]=ground;
            }
            previous=current;
        }
        if(groundedEnd)*groundedEnd={b.x,previousHeight+clearance,b.z};
        return true;
    }
    float WallClearance(Point p,float limit=48) const {
        float best=limit*limit;
        for(int i=0;i<wallCount;++i) {
            const auto& v=walls[i];
            if(p.y+8<std::min({v[0].y,v[1].y,v[2].y}) || p.y>std::max({v[0].y,v[1].y,v[2].y})+8)continue;
            for(int edge=0;edge<3;++edge) {
                const auto a=v[edge],b=v[(edge+1)%3];
                const float dx=b.x-a.x,dz=b.z-a.z,l2=dx*dx+dz*dz;
                if(l2<.01f)continue;
                const float t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/l2,0.f,1.f);
                const float x=p.x-a.x-t*dx,z=p.z-a.z-t*dz;
                best=std::min(best,x*x+z*z);
            }
        }
        return std::sqrt(best);
    }
    bool EscapeTarget(Point pos,Point desired,Point& target,float maxDrop=18,int attempt=0) const {
        float best=-1e30f;bool found=false;
        for(float distance:{48.f,80.f})for(int direction=0;direction<16;++direction) {
            const float angle=(direction+(attempt%2)*.5f)*6.28318530718f/16;
            Point end{},candidate{pos.x+std::sin(angle)*distance,pos.y,pos.z+std::cos(angle)*distance};
            if(!SafeSegment(pos,candidate,0,maxDrop,&end) || !SafeSegment(end,end,20,maxDrop))continue;
            const float clearance=WallClearance(end);
            if(clearance<12)continue;
            const float dx=desired.x-pos.x,dz=desired.z-pos.z,length=std::sqrt(dx*dx+dz*dz);
            const float score=clearance*4+(pos.y-end.y)*2+
                (length>0?((end.x-pos.x)*dx+(end.z-pos.z)*dz)/length*.15f:0);
            if(score>best){best=score;target=end;found=true;}
        }
        return found;
    }
    bool Add(Point a, Point b, Point c) {
        for(int i=0;i<count;++i)if(SameTriangle(nodes[i].vertices,a,b,c))return true;
        if (count == Capacity) { overflow = true; return false; }
        auto& n=nodes[count++]; n=Node{};
        n.vertices[0]=a; n.vertices[1]=b; n.vertices[2]=c;
        n.center={(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
        return true;
    }
    void Connect() {
        IndexFloors();
        // Once per load. Exact matching vertices preserve vertical separation:
        // two overlapping floors are never linked just because their X/Z match.
        for (int i=0;i<count;++i) for (int j=i+1;j<count;++j)
            for (int a=0;a<3;++a) for (int b=0;b<3;++b) {
                const auto& p=nodes[i].vertices[a]; const auto& q=nodes[i].vertices[(a+1)%3];
                const auto& r=nodes[j].vertices[b]; const auto& s=nodes[j].vertices[(b+1)%3];
                const float length=Distance2(p,q);
                if(length<24*24)continue;
                auto fraction=[&](Point t) {return ((t.x-p.x)*(q.x-p.x)+(t.y-p.y)*(q.y-p.y)+(t.z-p.z)*(q.z-p.z))/length;};
                float fr=fraction(r),fs=fraction(s);
                auto onLine=[&](Point t,float f) {return Distance2(t,{p.x+(q.x-p.x)*f,p.y+(q.y-p.y)*f,p.z+(q.z-p.z)*f})<.01f;};
                if(!onLine(r,fr)||!onLine(s,fs))continue;
                float lo=std::max(0.f,std::min(fr,fs)),hi=std::min(1.f,std::max(fr,fs));
                if(hi<=lo || (hi-lo)*(hi-lo)*length<24*24)continue;
                const float mid=(lo+hi)/2;
                Point portal={p.x+(q.x-p.x)*mid,p.y+(q.y-p.y)*mid,p.z+(q.z-p.z)*mid};
                if(!ClearWalls(nodes[i].center,nodes[j].center))continue;
                auto link=[&](int from,int to) {
                    for(int k=0;k<6;++k) {
                        if(nodes[from].adjacent[k]==to)return;
                        if(nodes[from].adjacent[k]<0) {nodes[from].adjacent[k]=to;nodes[from].portals[k]=portal;return;}
                    }
                    overflow=true;
                };
                link(i,j);link(j,i);
            }
        // Double Deck starts on a raised centre pad without a ramp. One-way
        // drop portals let gravity carry a kart to a verified floor beneath
        // the ledge. Never link empty space, large falls or an uphill step.
        for(int i=0;i<count;++i) for(int edge=0;edge<3;++edge) {
            Point a=nodes[i].vertices[edge],b=nodes[i].vertices[(edge+1)%3];
            Point mid={(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2};
            bool joined=false;
            for(int k=0;k<6;++k) if(nodes[i].adjacent[k]>=0 &&
                SurfaceDistance2(nodes[i].portals[k],nodes[i].vertices)<.01f &&
                Distance2(mid,nodes[i].portals[k])<Distance2(a,b)*.24f) joined=true;
            if(joined)continue;
            float dx=b.x-a.x,dz=b.z-a.z,length=std::sqrt(dx*dx+dz*dz);
            if(length<24)continue;
            float nx=dz/length,nz=-dx/length;
            if((nodes[i].center.x-mid.x)*nx+(nodes[i].center.z-mid.z)*nz>0) {nx=-nx;nz=-nz;}
            Point beyond={mid.x+nx*24,mid.y,mid.z+nz*24};
            float highest=-3000;int lower=-1;
            for(int j=0;j<count;++j) if(i!=j) {
                float y=SurfaceHeight(beyond,nodes[j].vertices);
                if(y<=mid.y-5 && y>=mid.y-60 && y>highest) {highest=y;lower=j;}
            }
            if(lower>=0) for(int k=0;k<6;++k) if(nodes[i].adjacent[k]<0) {
                nodes[i].adjacent[k]=lower;nodes[i].portals[k]=beyond;break;
            }
        }
        // Weak components are a quick prefilter; the BFS below still respects
        // directed drops and will never promise an uphill return route.
        for(int i=0;i<count;++i)parent[i]=i;
        auto root=[&](int i) {while(parent[i]!=i)i=parent[i];return i;};
        for(int i=0;i<count;++i) for(auto j:nodes[i].adjacent)
            if(j>=0)parent[root(j)]=root(i);
        for(int i=0;i<count;++i)nodes[i].component=root(i);
        cachedStart=-1;
    }
    int Nearest(Point p, int component=-1) const {
        float best=1e30f; int result=-1;
        const int cell=Cell(p);
        if(indexed && cell>=0) {
            for(int word=0;word<(count+31)/32;++word) {
                uint32_t bits=floorCells[cell][word];
                while(bits) {
                    const int i=word*32+__builtin_ctz(bits);bits&=bits-1;
                    if(component>=0 && nodes[i].component!=component)continue;
                    const float y=SurfaceHeight(p,nodes[i].vertices);
                    if(y<=-2999)continue;
                    const float score=(p.y-y)*(p.y-y);
                    if(score<best){best=score;result=i;}
                }
            }
            if(result>=0)return result;
        }
        for (int i=0;i<count;++i) {
            if (component>=0 && nodes[i].component!=component) continue;
            const float score=SurfaceDistance2(p,nodes[i].vertices);
            if (score<best) { best=score; result=i; }
        }
        return result;
    }
    bool Reachable(int start,int goal) {
        if (start<0 || goal<0 || start>=count || goal>=count ||
            nodes[start].component!=nodes[goal].component) return false;
        if(cachedStart!=start) {
            std::fill(parent.begin(), parent.begin()+count, -1);
            int head=0,tail=0; queue[tail++]=start; parent[start]=start;routeLength[start]=0;
            while(head<tail) for(auto next:nodes[queue[head++]].adjacent)
                if(next>=0 && parent[next]<0) {
                    parent[next]=queue[head-1];
                    routeLength[next]=routeLength[parent[next]]+std::sqrt(Distance2(nodes[parent[next]].center,nodes[next].center));
                    queue[tail++]=next;
                }
            cachedStart=start;
        }
        return parent[goal]>=0;
    }
    // Length of the existing BFS route, not straight-line distance through
    // stacked floors. Cache alongside the existing traversal: no new search
    // per box or per physics update. Eight KiB of fixed, allocation-free data.
    float RouteDistance(int start,int goal,Point pos,Point destination) {
        if(!Reachable(start,goal))return 1e30f;
        if(start==goal)return std::sqrt(Distance2(pos,destination));
        return std::sqrt(Distance2(pos,nodes[start].center))+routeLength[goal]+
            std::sqrt(Distance2(nodes[goal].center,destination));
    }
    bool Step(int start, int goal, Point& target) {
        if(!Reachable(start,goal))return false;
        if (start==goal) { target=nodes[goal].center; return true; }
        int next=goal;
        while(parent[next]!=start) next=parent[next];
        for(int edge=0;edge<6;++edge) if(nodes[start].adjacent[edge]==next) {
            Point portal=nodes[start].portals[edge];
            const auto& center=nodes[next].center;
            // Aim through, rather than stopping exactly on the shared edge.
            target={portal.x+(center.x-portal.x)*.35f,
                    portal.y+(center.y-portal.y)*.35f,
                    portal.z+(center.z-portal.z)*.35f};
            return true;
        }
        return false;
    }
    bool Steer(int start,int goal,Point pos,Point destination,Point& target,bool allowDrop=true) {
#ifdef MK64_ARENA_TEST_METRICS
        ++steerQueries;
#endif
        if(!Reachable(start,goal))return false;
        int size=0,next=goal;
        while(next!=start && size<Capacity){queue[size++]=next;next=parent[next];}
        // A portal alone can be only a few pixels away. Look through several
        // floor triangles, but accept shortcuts only with continuous support
        // for the whole kart and no intervening wall.
        if(size<=8) {
            float distance=std::sqrt(Distance2(pos,destination));
            Point candidate=distance>140?Lerp(pos,destination,140/distance):destination;
            if(SafeSegment(pos,candidate)){target=candidate;return true;}
        }
        // Try the farthest candidate first and stop at the first safe one.
        // Do not validate every nearer point again on each steering refresh.
        for(int i=std::max(0,size-8);i<size;++i) {
            int to=queue[i],from=i+1<size?queue[i+1]:start;Point portal=nodes[to].center;
            for(int k=0;k<6;++k)if(nodes[from].adjacent[k]==to){portal=nodes[from].portals[k];break;}
            for(Point candidate:{nodes[to].center,portal}) {
                float distance=std::sqrt(Distance2(pos,candidate));
                if(distance>140)candidate=Lerp(pos,candidate,140/distance);
                if(SafeSegment(pos,candidate)) {target=candidate;return true;}
            }
        }
            // The kart can start close to a boundary, or on Double Deck's
            // explicit one-way drop pad. Preserve its verified portal route.
            if(size && Step(start,goal,target) && (allowDrop || SafeSegment(pos,target,0)))return true;
            target=nodes[start].center;
            // No unchecked portal fallback beside a deadly hole. If the body
            // starts at the edge, verify the centre trace back onto its floor.
            if(!SafeSegment(pos,target,0))return false;
        return true;
    }
private:
    static constexpr int Grid=16,Words=Capacity/32,WallWords=WallCapacity/32;
    std::array<std::array<uint32_t,Words>,Grid*Grid> floorCells{};
    std::array<std::array<uint32_t,WallWords>,Grid*Grid> wallCells{};
    float minX=0,minZ=0,maxX=0,maxZ=0,scaleX=0,scaleZ=0;
    bool indexed=false;
    int GridX(float x) const {return std::clamp(int((x-minX)*scaleX),0,Grid-1);}
    int GridZ(float z) const {return std::clamp(int((z-minZ)*scaleZ),0,Grid-1);}
    int Cell(Point p) const {
        if(!indexed || p.x<minX || p.x>maxX || p.z<minZ || p.z>maxZ)return -1;
        return std::clamp(int((p.z-minZ)*scaleZ),0,Grid-1)*Grid+
               std::clamp(int((p.x-minX)*scaleX),0,Grid-1);
    }
    void IndexFloors() {
        for(auto& cell:floorCells)cell.fill(0);
        for(auto& cell:wallCells)cell.fill(0);
        indexed=false;if(!count)return;
        minX=minZ=1e30f;maxX=maxZ=-1e30f;
        for(int i=0;i<count;++i)for(auto p:nodes[i].vertices) {
            minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
            minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);
        }
        minX-=.01f;minZ-=.01f;maxX+=.01f;maxZ+=.01f;
        scaleX=Grid/std::max(1.f,maxX-minX);scaleZ=Grid/std::max(1.f,maxZ-minZ);
        indexed=true;
        for(int i=0;i<wallCount;++i) {
            const auto& v=walls[i];
            const int x0=GridX(std::min({v[0].x,v[1].x,v[2].x})-.001f),x1=GridX(std::max({v[0].x,v[1].x,v[2].x})+.001f);
            const int z0=GridZ(std::min({v[0].z,v[1].z,v[2].z})-.001f),z1=GridZ(std::max({v[0].z,v[1].z,v[2].z})+.001f);
            for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)wallCells[z*Grid+x][i/32]|=uint32_t(1)<<(i%32);
        }
        for(int i=0;i<count;++i) {
            float x0=1e30f,z0=1e30f,x1=-1e30f,z1=-1e30f;
            for(auto p:nodes[i].vertices){x0=std::min(x0,p.x);x1=std::max(x1,p.x);z0=std::min(z0,p.z);z1=std::max(z1,p.z);}
            const int a=Cell({x0-.001f,0,z0-.001f}),b=Cell({x1+.001f,0,z1+.001f});
            for(int z=a/Grid;z<=b/Grid;++z)for(int x=a%Grid;x<=b%Grid;++x)
                floorCells[z*Grid+x][i/32]|=uint32_t(1)<<(i%32);
        }
    }
    std::array<int16_t, Capacity> queue{}, parent{};
    std::array<float, Capacity> routeLength{};
    int cachedStart=-1;
};
}
