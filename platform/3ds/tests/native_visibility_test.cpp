#include "native_visibility_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

using namespace mk64_3ds::native_scene;

static float F24(float x) {
    // Independent mantissa truncation model. Test full precision and each
    // product/sum rounded to 16 fraction bits, including large cancellation.
    std::uint32_t bits;std::memcpy(&bits,&x,4);bits&=~127u;
    std::memcpy(&x,&bits,4);return x;
}
static unsigned Clip(const Vertex& vertex,const Camera& camera,bool reduced) {
    float clip[4]{};
    for(unsigned r=0;r<4;++r) {
        float v=reduced?F24(camera.rows[r][3]):camera.rows[r][3];
        for(unsigned c=0;c<3;++c) {
            const float coefficient=reduced?F24(camera.rows[r][c]):camera.rows[r][c];
            float product=coefficient*vertex.position[c];if(reduced)product=F24(product);
            v+=product;if(reduced)v=F24(v);
        }
        clip[r]=v;
    }
    return (clip[0]<-clip[3]?1:0)|(clip[0]>clip[3]?2:0)|
           (clip[1]<-clip[3]?4:0)|(clip[1]>clip[3]?8:0)|
           (clip[2]<-clip[3]?16:0)|(clip[2]>0?32:0);
}

static std::uint64_t Check(const std::vector<Vertex>& vertices,const std::vector<std::uint16_t>& indices,
                         const std::vector<DrawRange>& ranges,NativeVisibilityPlan& plan,const Camera& camera) {
    plan.SetCamera(camera);std::uint64_t removed=0;
    for(std::size_t id=0;id<ranges.size();++id) {
        const auto& range=ranges[id];const auto selected=plan.Select(id,range);
        assert(selected.count<=4);std::uint32_t previous=range.first;
        for(unsigned s=0;s<selected.count;++s) {
            const auto& span=selected.spans[s];assert(span.count && span.count%3==0 && span.first%3==0);
            assert(span.first>=previous && span.first>=range.first && span.first+span.count<=range.first+range.count);
            previous=span.first+span.count;
        }
        for(std::uint32_t first=range.first;first<range.first+range.count;first+=3) {
            bool retained=false;
            for(unsigned s=0;s<selected.count;++s)retained|=first>=selected.spans[s].first && first<selected.spans[s].first+selected.spans[s].count;
            if(retained)continue;
            // Every removed triangle must be wholly outside a common PICA
            // plane in BOTH references. Near-plane intersections remain drawn.
            for(bool reduced:{false,true}) {
                unsigned common=63;
                for(unsigned corner=0;corner<3;++corner)common&=Clip(vertices[indices[first+corner]],camera,reduced);
                assert(common!=0);
            }
            ++removed;
        }
        const auto again=plan.Select(id,range);assert(again.count==selected.count);
        for(unsigned s=0;s<selected.count;++s)
            assert(again.spans[s].first==selected.spans[s].first && again.spans[s].count==selected.spans[s].count);
    }
    return removed;
}

static std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);assert(input);
    return {(std::istreambuf_iterator<char>(input)),{}};
}

int main(int argc,char** argv) {
    Camera ortho{};ortho.rows[0][0]=ortho.rows[1][1]=ortho.rows[2][2]=1;
    ortho.rows[2][3]=-10;ortho.rows[3][3]=20;
    GeometryBounds inside{{-1,-1,-1},{1,1,1}},right{{30,-1,-1},{40,1,1}},crossing{{19,-1,-1},{30,1,1}};
    NativeClipFrustum frustum(ortho);
    assert(frustum.Classify(inside)==NativeClipFrustum::Inside);
    assert(frustum.Classify(right)==NativeClipFrustum::Outside);
    assert(frustum.Classify(crossing)==NativeClipFrustum::Intersecting);
    for(unsigned axis=0;axis<3;++axis)for(int sign:{-1,1}) {
        GeometryBounds box=inside;box.minimum[axis]=30*sign;box.maximum[axis]=40*sign;
        if(sign<0)std::swap(box.minimum[axis],box.maximum[axis]);
        assert(frustum.Classify(box)==NativeClipFrustum::Outside);
    }
    Camera invalid=ortho;invalid.rows[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(NativeClipFrustum(invalid).Classify(right)==NativeClipFrustum::Intersecting);
    // Almost cancelling W and Z rows must not erase borderline geometry.
    Camera cancel{};cancel.rows[3][0]=65536;cancel.rows[3][3]=1;
    cancel.rows[2][0]=-65536;cancel.rows[2][3]=-1.001f;
    assert(NativeClipFrustum(cancel).Classify(inside)!=NativeClipFrustum::Outside);
    NativeVisibleRanges ordered;
    for(unsigned i=0;i<24;++i)ordered.Append({i*12,3});
    assert(ordered.count==4);
    for(unsigned i=0;i<24;++i) {
        bool retained=false;for(unsigned s=0;s<ordered.count;++s)
            retained|=i*12>=ordered.spans[s].first && i*12<ordered.spans[s].first+ordered.spans[s].count;
        assert(retained);
    }
    std::mt19937 random(0x493d5001);std::vector<Vertex> vertices(256);
    for(unsigned i=0;i<vertices.size();++i)for(unsigned c=0;c<3;++c)
        vertices[i].position[c]=std::int16_t(int((i/32+c)%8)*7000-24500+int(random()%128));
    std::vector<std::uint16_t> indices(3072);
    for(unsigned i=0;i<indices.size();++i)indices[i]=(i/384)*32+random()%32;
    std::vector<DrawRange> ranges{{0,3072,0,0},{0,48,0,0}};
    NativeVisibilityPlan plan;assert(plan.Build(vertices.data(),vertices.size(),indices.data(),indices.size(),ranges.data(),ranges.size()));
    std::uint64_t removed=0;
    for(unsigned iteration=0;iteration<2048;++iteration) {
        Camera camera{};
        for(auto& row:camera.rows)for(float& value:row)value=float(int(random()%2001)-1000)/64.f;
        for(unsigned row=0;row<4;++row)camera.rows[row][3]*=4096;
        removed+=Check(vertices,indices,ranges,plan,camera);
    }
    assert(removed>10000);
    auto bad=indices;bad[7]=vertices.size();
    assert(!plan.Build(vertices.data(),vertices.size(),bad.data(),bad.size(),ranges.data(),ranges.size()));
    assert(!plan.Ready() && plan.Bytes()==0);
    auto fallback=plan.Select(0,ranges[0]);assert(fallback.count==1 && fallback.spans[0].count==3072);
    assert(!plan.Build(vertices.data(),vertices.size(),indices.data(),indices.size(),ranges.data(),100000));
    unsigned courses=0;std::uint64_t courseRemoved=0;std::size_t maximumBytes=0;
    if(argc==2)for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
        if(!entry.is_directory() || !std::filesystem::exists(entry.path()/"mesh.ncm"))continue;
        const auto meshBytes=Read(entry.path()/"mesh.ncm"),sceneBytes=Read(entry.path()/"scene.nlc");
        MeshView mesh;SceneView scene;
        assert(ValidateMesh(meshBytes.data(),meshBytes.size(),mesh));
        assert(ValidateScene(sceneBytes.data(),sceneBytes.size(),mesh.indexCount,scene));
        vertices.resize(mesh.vertexCount);indices.resize(mesh.indexCount);ranges.resize(scene.rangeCount);
        std::memcpy(vertices.data(),mesh.vertices,vertices.size()*sizeof(Vertex));
        std::memcpy(indices.data(),mesh.indices,indices.size()*2);std::memcpy(ranges.data(),scene.ranges,ranges.size()*sizeof(DrawRange));
        assert(plan.Build(vertices.data(),vertices.size(),indices.data(),indices.size(),ranges.data(),ranges.size()));
        maximumBytes=std::max(maximumBytes,plan.Bytes());std::uint64_t local=0;
        for(unsigned sample=0;sample<48;++sample) {
            const auto& eye=vertices[indices[random()%indices.size()]].position;
            const float angle=sample*.61803399f,cs=std::cos(angle),sn=std::sin(angle);
            Camera camera{};camera.rows[0][0]=cs;camera.rows[0][2]=-sn;
            camera.rows[1][1]=1.6f;camera.rows[2][0]=sn;camera.rows[2][2]=cs;
            camera.rows[3][0]=-sn;camera.rows[3][2]=-cs;
            for(unsigned r=0;r<4;++r)for(unsigned c=0;c<3;++c)camera.rows[r][3]-=camera.rows[r][c]*eye[c];
            camera.rows[2][3]-=10;camera.rows[3][3]+=100;
            if(sample&1)for(auto& value:camera.rows[0])value=-value;
            local+=Check(vertices,indices,ranges,plan,camera);
        }
        courseRemoved+=local;++courses;
        std::printf("%s: %u ranges, %zu metadata bytes, %llu rejected triangle occurrences across 48 synthetic cameras\n",
            entry.path().filename().c_str(),unsigned(ranges.size()),plan.Bytes(),(unsigned long long)local);
    }
    if(argc==2)assert(courses==20 && courseRemoved>100000);
    std::printf("PASS: conservative native visibility, 2048 randomized cameras, bounded ordered spans, float32/float24 references, invalid-data fallback; %u optional course groups, max %zu metadata bytes. No game FPS measurement.\n",courses,maximumBytes);
}
