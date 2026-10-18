#include "animation_bank.h"
#include <zlib.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using Bank=bambu::ui::AnimationBank;
using Bytes=std::vector<uint8_t>;
void Check(bool condition){if(!condition)throw std::runtime_error("Animation codec regression");}
void Write(Bytes& b,size_t at,uint32_t value){for(int i=0;i<4;++i)b[at+i]=uint8_t(value>>(i*8));}
Bytes Fixture(const Bytes& raw,int version=3){
 Bytes compressed(compressBound(raw.size()));uLongf size=compressed.size();
 Check(compress2(compressed.data(),&size,raw.data(),raw.size(),9)==Z_OK);compressed.resize(size);
 constexpr size_t table=32+64*40,payload=table+25*8;
 Bytes b(payload);const char* magic=version==3?"BBANIM03":"BBANIM02";
 for(int i=0;i<8;++i)b[i]=magic[i];
 Write(b,8,version);Write(b,12,64);Write(b,16,payload+size);Write(b,20,3);Write(b,24,40);Write(b,28,1024);
 for(size_t i=0;i<64;++i){
  const uint32_t entry[]={uint32_t(i/2),uint32_t(i%2),0,44,3,1,25,24,50,uint32_t(table)};
  for(size_t j=0;j<10;++j)Write(b,32+i*40+j*4,entry[j]);
 }
 for(size_t i=0;i<25;++i){Write(b,table+i*8,payload);Write(b,table+i*8+4,size);}
 b.insert(b.end(),compressed.begin(),compressed.end());return b;
}
bool Decode(const Bytes& b,bool green,uint16_t* output){
 Check(Bank::SetData(b.data(),b.size()));Bank::Clip c;Check(Bank::Get(0,green,c));
 return Bank::Decode(c,0,output,3);
}
Bytes ReadFile(const char* path){std::ifstream f(path,std::ios::binary);Check(bool(f));return Bytes(std::istreambuf_iterator<char>(f),{});}
int main(int argc,char** argv){
 uint16_t out[3]{};
 // Distinct colors in each theme must survive exactly, including index 256.
 for(size_t colors:{size_t(3),size_t(257)}){
  Bytes raw(2+colors*4);raw[0]=uint8_t(colors);raw[1]=uint8_t(colors>>8);
  for(size_t i=0;i<colors;++i)Write(raw,2+i*4,uint32_t(0xabcd+i)<<16|uint32_t(0x1234+i));
  if(colors<=256)raw.insert(raw.end(),{0,1,2});else raw.insert(raw.end(),{0,0,1,0,0,1});
  auto b=Fixture(raw);Check(Decode(b,false,out));Check(out[0]==0x1234 && out[1]==0x1235 && out[2]==0x1234+(colors==3?2:256));
  Check(Decode(b,true,out));Check(out[0]==0xabcd && out[1]==0xabce && out[2]==0xabcd+(colors==3?2:256));
  raw.pop_back();Check(!Decode(Fixture(raw),false,out));
 }
 Check(!Decode(Fixture({0,0}),false,out));
 Check(!Decode(Fixture({1,4}),false,out)); // Palette larger than the allowed 1024.
 Check(!Decode(Fixture({1,0,0,0,0,0,0,0,1}),false,out)); // Invalid index.
 Check(!Decode(Fixture(Bytes(100000)),false,out)); // Inflated output exceeds scratch.
 Check(Decode(Fixture({0x34,0x12,0x78,0x56,0xbc,0x9a},2),false,out));
 Check(out[0]==0x1234 && out[1]==0x5678 && out[2]==0x9abc);
 if(argc==3){
  const auto old=ReadFile(argv[1]),current=ReadFile(argv[2]);
  std::vector<uint16_t> reference,actual;size_t checked=0;
  for(uint32_t id=0;id<32;++id)for(bool green:{false,true}){
   Check(Bank::SetData(old.data(),old.size()));Bank::Clip a;Check(Bank::Get(id,green,a));
   reference.resize(a.width*a.height);actual.resize(reference.size());
   for(uint32_t frame=0;frame<a.frames;++frame){
    Check(Bank::SetData(old.data(),old.size()));Check(Bank::Decode(a,frame,reference.data(),reference.size()));
    Check(Bank::SetData(current.data(),current.size()));Bank::Clip b;Check(Bank::Get(id,green,b));
    Check(a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height && a.frames==b.frames);
    Check(Bank::Decode(b,frame,actual.data(),actual.size()));Check(reference==actual);++checked;
   }
  }
  std::cout<<checked<<" frames match previous RGB565 pixels exactly\n";
 }
 std::cout<<"Animation codec tests passed\n";
}
