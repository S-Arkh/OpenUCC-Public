#include "openucc/package.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace openucc;
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("Expected fixture path");
    std::filesystem::path path(argv[1]);
    Package p;p.version=133;p.licensee=40;p.header.resize(56);p.intern("None");
    auto plain=p.serialize();Bytes wrapped;
    for(char c:std::string("Lineage2Ver121")){wrapped.push_back(c);wrapped.push_back(0);}
    // Fixed oracle for the CMake fixture basename MiXeD.utx, including extension.
    constexpr unsigned char key=166;
    for(auto c:plain)wrapped.push_back(c^key);
    auto save=[&](){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(wrapped.data()),wrapped.size());if(!f)throw std::runtime_error("Fixture write failed");};
    save();auto decoded=Package::load(path);
    if(decoded.envelope!="Lineage2Ver121"||decoded.serialize()!=plain)throw std::runtime_error("Ver121 roundtrip failed");
    auto snapshot=Package::parseFile(wrapped,path);
    if(snapshot.envelope!="Lineage2Ver121"||snapshot.serialize()!=plain)throw std::runtime_error("Ver121 snapshot failed");
    bool failed=false;try{Package::parse(wrapped);}catch(const std::exception&){failed=true;}
    if(!failed)throw std::runtime_error("Filename-free Ver121 accepted");
    failed=false;try{Package::parseFile(wrapped,path.parent_path()/"Other.utx");}catch(const std::exception&){failed=true;}
    if(!failed)throw std::runtime_error("Ver121 snapshot accepted wrong filename");
    wrapped[28]^=1;save();failed=false;
    try{Package::load(path);}catch(const std::exception&){failed=true;}
    if(!failed)throw std::runtime_error("Corrupt Ver121 signature accepted");
    std::cout<<"Ver121 checks passed\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
