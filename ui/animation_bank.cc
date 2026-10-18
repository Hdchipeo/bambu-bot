#include "animation_bank.h"
#include <algorithm>
#include <cstring>
#include <vector>
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
size_t bank_size=0,max_pixels=0,palette_limit=0;
uint32_t generation=0,format=0;
uint32_t Read(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool Range(size_t offset,size_t bytes,size_t size){return offset<=size && bytes<=size-offset;}
AnimationBank::Clip Entry(const uint8_t* p){return {Read(p),Read(p+4),Read(p+8),Read(p+12),Read(p+16),Read(p+20),Read(p+24),Read(p+28),Read(p+32),Read(p+36)};}
}
bool AnimationBank::SetData(const uint8_t* data,size_t size){
 bank=nullptr;bank_size=0;max_pixels=0;format=0;++generation;
 constexpr uint32_t count=kClipCount*2;
 if(!data || size<32+count*40)return false;
 const auto version=Read(data+8);
 if(!((version==2 && std::memcmp(data,"BBANIM02",8)==0) || (version==3 && std::memcmp(data,"BBANIM03",8)==0)))return false;
 if(Read(data+12)!=count || Read(data+16)!=size || Read(data+24)!=40)return false;
 const auto colors=Read(data+28);if(version==3 && (colors==0 || colors>1024))return false;
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
 bank=data;bank_size=size;max_pixels=declared_pixels;format=version;palette_limit=colors;return true;
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
 // Shared scratch is reused under the LVGL lock. Even 16-bit indices and
 // the largest allowed palette fit; malformed streams cannot grow the buffer.
 static std::vector<uint8_t> indexed;
 const bool palette=format==3;
 const size_t scratch_bytes=max_pixels*(palette_limit<=256?1:2)+2+4*palette_limit;
 if(palette && indexed.size()!=scratch_bytes)indexed.resize(scratch_bytes);
 auto* destination=palette?indexed.data():reinterpret_cast<uint8_t*>(output);
 const size_t capacity=palette?indexed.size():pixels*2;
 size_t decoded_bytes=0;
 bool valid=false;
#ifdef ESP_PLATFORM
 // ROM's convenience function puts a large inflater on the task stack. Keep
 // the reusable low-level state in heap/PSRAM instead, with no per-frame malloc.
 static auto inflater=std::make_unique<tinfl_decompressor>();
 tinfl_init(inflater.get());size_t input=bytes,output_bytes=capacity;
 const auto status=tinfl_decompress(inflater.get(),bank+offset,&input,destination,
     destination,&output_bytes,TINFL_FLAG_PARSE_ZLIB_HEADER|TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
 valid=status==TINFL_STATUS_DONE && input==bytes;decoded_bytes=output_bytes;
#else
 z_stream stream{};stream.next_in=const_cast<Bytef*>(bank+offset);stream.avail_in=static_cast<uInt>(bytes);
 stream.next_out=destination;stream.avail_out=static_cast<uInt>(capacity);
 if(inflateInit(&stream)!=Z_OK)return false;
 const auto result=inflate(&stream,Z_FINISH);
 valid=result==Z_STREAM_END && stream.total_in==bytes;decoded_bytes=stream.total_out;
 inflateEnd(&stream);
#endif
 if(!valid)return false;
 if(!palette)return decoded_bytes==pixels*2;
 if(decoded_bytes<2)return false;
 const size_t colors=size_t(indexed[0])|(size_t(indexed[1])<<8);
 if(colors==0 || colors>palette_limit)return false;
 const size_t stride=colors<=256?1:2,table_bytes=2+colors*4;
 if(decoded_bytes!=table_bytes+pixels*stride)return false;
 const auto* indices=indexed.data()+table_bytes;
 for(size_t i=0;i<pixels;++i){
  const size_t index=stride==1?indices[i]:size_t(indices[i*2])|(size_t(indices[i*2+1])<<8);
  if(index>=colors)return false;
  const auto* color=indexed.data()+2+index*4+(c.theme?2:0);
  output[i]=uint16_t(color[0])|(uint16_t(color[1])<<8);
 }
 return true;
}
size_t AnimationBank::Size(){return bank_size;}
size_t AnimationBank::MaxPixels(){return max_pixels;}
uint32_t AnimationBank::Generation(){return generation;}
}
