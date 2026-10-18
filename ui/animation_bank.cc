#include "animation_bank.h"
#include <algorithm>
#include <cstring>
#ifdef ESP_PLATFORM
#include <memory>
extern "C" {
#include <miniz.h>
}
#else
#include <zlib.h>
#endif

namespace bambu::ui {
namespace {
const uint8_t* bank=nullptr;
size_t bank_size=0,max_pixels=0;
uint32_t generation=0;
uint32_t Read(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool Range(size_t offset,size_t bytes,size_t size){return offset<=size && bytes<=size-offset;}
AnimationBank::Clip Entry(const uint8_t* p){return {Read(p),Read(p+4),Read(p+8),Read(p+12),Read(p+16),Read(p+20),Read(p+24),Read(p+28),Read(p+32),Read(p+36)};}
}
bool AnimationBank::SetData(const uint8_t* data,size_t size){
 bank=nullptr;bank_size=0;max_pixels=0;++generation;
 constexpr uint32_t count=kClipCount*2;
 if(!data || size<32+count*40 || std::memcmp(data,"BBANIM02",8)!=0)return false;
 if(Read(data+8)!=2 || Read(data+12)!=count || Read(data+16)!=size || Read(data+24)!=40)return false;
 const auto declared_pixels=Read(data+20);if(declared_pixels==0 || declared_pixels>280*152)return false;
 size_t payload=32+count*40,observed_pixels=0;
 for(uint32_t i=0;i<count;++i){const auto c=Entry(data+32+i*40);
  if(c.id!=i/2 || c.theme!=i%2 || !c.width || !c.height || c.x>280 || c.width>280-c.x || c.y<44 || c.y>196 || c.height>196-c.y)return false;
  if(c.frame_ms!=50 || c.intro!=24 || c.frames<=c.intro || c.frames>256 || c.table<32+count*40 || !Range(c.table,size_t(c.frames)*8,size))return false;
  payload=std::max<size_t>(payload,size_t(c.table)+c.frames*8);observed_pixels=std::max<size_t>(observed_pixels,size_t(c.width)*c.height);
 }
 if(observed_pixels!=declared_pixels)return false;
 for(uint32_t i=0;i<count;++i){const auto c=Entry(data+32+i*40);
  for(uint32_t frame=0;frame<c.frames;++frame){const auto p=data+c.table+frame*8;const auto offset=Read(p),length=Read(p+4);
   if(offset<payload || length==0 || !Range(offset,length,size))return false;
  }
 }
 bank=data;bank_size=size;max_pixels=declared_pixels;return true;
}
bool AnimationBank::Get(uint32_t id,bool green,Clip& clip){
 if(!bank || id>=kClipCount)return false;
 clip=Entry(bank+32+(id*2+(green?1:0))*40);return true;
}
uint32_t AnimationBank::FrameAt(const Clip& c,uint32_t ms){
 if(!c.frame_ms || c.frames<=c.intro)return 0;
 const auto frame=ms/c.frame_ms;return frame<c.intro?frame:c.intro+(frame-c.intro)%(c.frames-c.intro);
}
bool AnimationBank::Decode(const Clip& requested,uint32_t frame,uint16_t* output,size_t pixels){
 Clip c;
 if(!Get(requested.id,requested.theme!=0,c) || frame>=c.frames || !output || pixels!=size_t(c.width)*c.height)return false;
 const auto p=bank+c.table+frame*8;const size_t offset=Read(p),bytes=Read(p+4);
 if(!Range(offset,bytes,bank_size))return false;
 const size_t expected=pixels*2;
#ifdef ESP_PLATFORM
 // ROM's convenience function puts a large inflater on the task stack. Keep
 // the reusable low-level state in heap/PSRAM instead, with no per-frame malloc.
 static auto inflater=std::make_unique<tinfl_decompressor>();
 tinfl_init(inflater.get());size_t input=bytes,output_bytes=expected;
 const auto status=tinfl_decompress(inflater.get(),bank+offset,&input,reinterpret_cast<uint8_t*>(output),
     reinterpret_cast<uint8_t*>(output),&output_bytes,TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
 return status==TINFL_STATUS_DONE && input==bytes && output_bytes==expected;
#else
 z_stream stream{};stream.next_in=const_cast<Bytef*>(bank+offset);stream.avail_in=static_cast<uInt>(bytes);
 stream.next_out=reinterpret_cast<Bytef*>(output);stream.avail_out=static_cast<uInt>(expected);
 if(inflateInit(&stream)!=Z_OK)return false;
 const auto result=inflate(&stream,Z_FINISH);
 const bool valid=result==Z_STREAM_END && stream.total_in==bytes && stream.total_out==expected;
 inflateEnd(&stream);return valid;
#endif
}
size_t AnimationBank::Size(){return bank_size;}
size_t AnimationBank::MaxPixels(){return max_pixels;}
uint32_t AnimationBank::Generation(){return generation;}
}
