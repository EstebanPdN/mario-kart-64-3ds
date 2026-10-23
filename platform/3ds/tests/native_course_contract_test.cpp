#include "native_course_contract_3ds.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace mk64_3ds::native_scene;
int main(int argc,char** argv) {
    IncomingCourseState c{0xf3205u,0xffffffffu,0x3fffffu,1,1};
    assert(c.Matches(0x2205,0x552078,0x82000,42,42,65535,65535));
    assert(!c.Matches(0x12205,0x552078,0x82000,42,42,65535,65535));
    assert(!c.Matches(0x2205,0x552068,0x82000,42,42,65535,65535));
    assert(!c.Matches(0x2205,0x552078,0x182000,42,42,65535,65535));
    assert(!c.Matches(0x2205,0x552078,0x82000,43,42,65535,65535));
    assert(!c.Matches(0x2205,0x552078,0x82000,42,42,0,65535));
    c={};assert(c.Matches(0xffffffff,0xffffffff,0xffffffff,43,42,0,0));
    std::vector<ExitState> out;std::vector<CourseExitExtra> extra;uint32_t textures=0;
    auto read=[](const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),{});};
    for(int i=1;i<argc;++i) {
        const std::string dir=argv[i];auto mesh=read(dir+"/mesh.ncm"),roots=read(dir+"/roots.ncr"),bytes=read(dir+"/exit.ncx");
        MeshView checked;assert(ValidateMesh(mesh.data(),mesh.size(),checked));const auto count=ReadLE32(roots.data()+4);
        if(!std::memcmp(bytes.data(),"NCX1",4)) {assert(ValidateExits(bytes.data(),bytes.size(),checked.vertexCount,out,textures,count));continue;}
        assert(ValidateCourseExits(bytes.data(),bytes.size(),checked.vertexCount,count,out,extra,textures));
        assert(out.size()==count && extra.size()==count);
        assert(!ValidateCourseExits(bytes.data(),bytes.size()-1,checked.vertexCount,count,out,extra,textures));
        bytes[16+sizeof(ExitState)+24]=64;
        assert(!ValidateCourseExits(bytes.data(),bytes.size(),checked.vertexCount,count,out,extra,textures));
        assert(out.empty() && extra.empty());
    }
}
