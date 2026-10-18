#include "scene3d.h"
#include <src/misc/cache/instance/lv_image_cache.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bambu::studio {
namespace {
constexpr float pi=3.14159265359f;
constexpr std::array<SceneInfo,18> info{{
 {"Game time","Bam plays a tiny game. A floating controller taps along below the head."},
 {"Coffee break","A warm cup rises for a sip. Steam curls into the air."},
 {"Music lover","Headphones on. Bam nods to the beat as little notes drift past."},
 {"Curious explorer","A magnifying glass sweeps around, searching for a new discovery."},
 {"Sleepy afternoon","The head slowly droops, sleepy eyes close, then Bam wakes a little."},
 {"A visiting fly","A fly loops around Bam. The head follows its very unpredictable flight."},
 {"Printer check","Bam watches a miniature printer build one little green cube."},
 {"Print complete!","A happy head bounce, confetti and a check mark. A print to celebrate."},
 {"Mini racer","A tiny race car, rolling wheels and a road rushing underneath."},
 {"Bubble tea","A cup of pearls, a straw and a very unhurried sip."},
 {"Just chilling","Sunglasses, a drifting cloud and a slow, sunny sway."},
 {"Printer alert","A stopped printer, an alert sign and concerned eyes. A calm error scene."},
 {"Robot disco","A tiny dance floor. The head bops and sways under a disco sparkle."},
 {"Plant parent","Bam waters a little sprout. Three drops and a very proud leaf."},
 {"Paper pilot","A paper airplane circles by. Bam turns to follow its flight."},
 {"Little magician","A wand, a star and a levitating cube. No supports required."},
 {"Stretch break","A slow head tilt and neck stretch after a long print."},
 {"Hello, maker!","A cheerful nod and a floating heart for the person at the desk."},
}};
static_assert(info.size()==static_cast<size_t>(Scene::Count));
uint32_t Shade(uint32_t c,float f) {
 auto ch=[&](int shift){return static_cast<uint32_t>(std::clamp(((c>>shift)&255)*f,0.f,255.f));};
 return ch(16)<<16|ch(8)<<8|ch(0);
}
}
const SceneInfo& DescribeScene(Scene s) { auto i=static_cast<size_t>(s);return info[i<info.size()?i:0]; }
SceneView::SceneView(lv_obj_t* parent,int w,int h,bool round):width_(w),height_(h) {
 scale_=std::min(w/320.f,h/320.f);round_=round;
 pixels_.resize(w*h);depth_.resize(w*h);
 image_.header.magic=LV_IMAGE_HEADER_MAGIC;image_.header.cf=LV_COLOR_FORMAT_ARGB8888;
 image_.header.w=w;image_.header.h=h;image_.header.stride=w*4;
 image_.data_size=pixels_.size()*4;image_.data=reinterpret_cast<const uint8_t*>(pixels_.data());
 root_=lv_obj_create(parent);lv_obj_remove_style_all(root_);lv_obj_set_size(root_,w,h);
 lv_obj_set_style_radius(root_,round?LV_RADIUS_CIRCLE:0,0);lv_obj_set_style_clip_corner(root_,true,0);
 lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
 lv_obj_remove_flag(root_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE));
 lv_obj_add_event_cb(root_,[](lv_event_t* e){static_cast<SceneView*>(lv_event_get_user_data(e))->Draw(lv_event_get_layer(e));},LV_EVENT_DRAW_MAIN,this);
 title_=lv_label_create(root_);footer_=lv_label_create(root_);
 for(auto label:{title_,footer_}) {
  lv_obj_set_style_text_font(label,w<320?&lv_font_montserrat_12:&lv_font_montserrat_14,0);
  lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_width(label,static_cast<int>(240*scale_));
 }
 lv_obj_align(title_,LV_ALIGN_TOP_MID,0,static_cast<int>((round?36:16)*scale_));
 lv_obj_align(footer_,LV_ALIGN_TOP_MID,0,round?77:31);
 CreateTelemetryHud();
 last_tick_=lv_tick_get();SetTheme(ui::Theme::Black);RenderAt(0);
 timer_=lv_timer_create([](lv_timer_t* t){auto self=static_cast<SceneView*>(lv_timer_get_user_data(t));
  auto now=lv_tick_get();auto dt=now-self->last_tick_;self->last_tick_=now;self->RenderAt(self->elapsed_+dt);
 },33,this);
}
SceneView::~SceneView(){if(timer_)lv_timer_delete(timer_);lv_image_cache_drop(&image_);lv_obj_delete(root_);}
void SceneView::SetScene(Scene scene){scene_=static_cast<size_t>(scene)>=info.size()?Scene::Game:scene;Restart();}
void SceneView::SetTheme(ui::Theme theme){theme_=theme;ink_=theme==ui::Theme::Black?ui::kBambuGreen:0xFFFFFF;bg_=theme==ui::Theme::Black?0:ui::kBambuGreen;
 lv_obj_set_style_bg_color(root_,lv_color_hex(bg_),0);for(auto l:{title_,footer_})lv_obj_set_style_text_color(l,lv_color_hex(ink_),0);
 for(auto value:values_)lv_obj_set_style_text_color(value,lv_color_hex(ink_),0);
 for(uint32_t i=0;i<lv_obj_get_child_count(hud_);++i){auto child=lv_obj_get_child(hud_,static_cast<int>(i));
  if(lv_obj_check_type(child,&lv_line_class))lv_obj_set_style_line_color(child,lv_color_hex(ink_),0);}
 lv_obj_set_style_bg_color(progress_bar_,lv_color_hex(ink_),LV_PART_MAIN);
 lv_obj_set_style_bg_color(progress_bar_,lv_color_hex(ink_),LV_PART_INDICATOR);RenderAt(elapsed_);}
void SceneView::SetTelemetry(const ui::Telemetry& telemetry){telemetry_=telemetry;RenderAt(elapsed_);}
void SceneView::SetAnimated(bool animated){animated_=animated;last_tick_=lv_tick_get();if(timer_){if(animated)lv_timer_resume(timer_);else lv_timer_pause(timer_);}}
void SceneView::Restart(){last_tick_=lv_tick_get();RenderAt(0);}
void SceneView::RenderAt(uint32_t elapsed){elapsed_=elapsed;Build();
 lv_label_set_text(title_,elapsed<1200?"BAMBU BOT / 2D > 3D":DescribeScene(scene_).name);
 char text[96];
 if(scene_==Scene::Error)std::snprintf(text,sizeof(text),"DEMO / Printer needs attention");
 else if(scene_==Scene::Complete)std::snprintf(text,sizeof(text),"DEMO / Your print is ready!");
 else if(!telemetry_.connected||telemetry_.stale)std::snprintf(text,sizeof(text),"Printer data unavailable");
 else text[0]=0;
 lv_label_set_text(footer_,text);
 if(text[0])lv_obj_remove_flag(footer_,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(footer_,LV_OBJ_FLAG_HIDDEN);
 UpdateTelemetryHud();lv_obj_invalidate(root_);
}
void SceneView::CreateTelemetryHud(){
 hud_=lv_obj_create(root_);lv_obj_remove_style_all(hud_);
 const int width=round_?252:232;
 lv_obj_set_size(hud_,width,round_?34:30);lv_obj_align(hud_,LV_ALIGN_BOTTOM_MID,0,round_?-35:-8);
 lv_obj_remove_flag(hud_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
 const float size=round_?1.f:.9f;
 auto line=[&](size_t index,int x,std::initializer_list<lv_point_precise_t> points){
  auto obj=lv_line_create(hud_);lv_obj_set_pos(obj,x,0);
  lv_obj_set_style_line_width(obj,round_?2:1,0);lv_obj_set_style_line_rounded(obj,true,0);
  lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE);
  size_t i=0;for(auto p:points)hud_points_[index][i++]={static_cast<lv_value_precise_t>(std::lround(p.x*size)),static_cast<lv_value_precise_t>(std::lround(p.y*size))};
  lv_line_set_points(obj,hud_points_[index].data(),i);
 };
 // Independent outlines inspired by Bambu Studio monitor_item_print/nozzle/bed icons.
 line(0,0,{{1,6},{1,14},{5,14},{5,10},{15,10},{15,14},{19,14},{19,6},{1,6}});
 line(1,0,{{5,6},{5,1},{15,1},{15,6}});
 line(2,0,{{5,10},{5,20},{15,20},{15,10}});
 line(3,0,{{8,13},{12,13},{12,16},{8,16}});
 const int nozzle=round_?84:74,bed=round_?178:160;
 line(4,nozzle,{{6,1},{12,1},{12,6},{16,6},{9,12},{2,6},{6,6},{6,1}});
 line(5,nozzle,{{3,13},{2,15},{4,17},{3,19}});
 line(6,nozzle,{{9,15},{8,17},{9,19}});
 line(7,nozzle,{{15,13},{14,15},{16,17},{15,19}});
 line(8,bed,{{1,16},{19,16},{19,19},{1,19},{1,16}});
 for(int i=0;i<3;++i){lv_value_precise_t x=4+i*6;line(9+i,bed,{{x,2},{x-1,4},{x+1,7},{x-1,10},{x,13}});}
 const int positions[3]={round_?26:24,round_?110:98,round_?204:184};
 for(size_t i=0;i<values_.size();++i){values_[i]=lv_label_create(hud_);lv_obj_set_pos(values_[i],positions[i],3);
  lv_obj_set_size(values_[i],i==1?58:48,22);lv_label_set_long_mode(values_[i],LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(values_[i],round_?&lv_font_montserrat_14:&lv_font_montserrat_12,0);}
 progress_bar_=lv_bar_create(hud_);lv_obj_remove_style_all(progress_bar_);
 lv_obj_set_pos(progress_bar_,0,round_?30:27);lv_obj_set_size(progress_bar_,width,2);
 lv_bar_set_range(progress_bar_,0,100);lv_obj_set_style_bg_opa(progress_bar_,LV_OPA_20,LV_PART_MAIN);
 lv_obj_set_style_bg_opa(progress_bar_,LV_OPA_COVER,LV_PART_INDICATOR);
}
void SceneView::UpdateTelemetryHud(){
 const bool fresh=telemetry_.connected&&!telemetry_.stale;
 auto value=[&](int v){return !fresh||v<0?std::string("--"):std::to_string(v);};
 lv_label_set_text(values_[0],(value(telemetry_.progress)+"%").c_str());
 lv_label_set_text(values_[1],(value(telemetry_.nozzle_c)+" °C").c_str());
 lv_label_set_text(values_[2],(value(telemetry_.bed_c)+" °C").c_str());
 lv_bar_set_value(progress_bar_,fresh&&telemetry_.progress>=0?std::clamp(telemetry_.progress,0,100):0,LV_ANIM_OFF);
}

SceneView::V SceneView::Transform(V v) const {
 float x=v.x*std::cos(yaw_)+v.z*std::sin(yaw_),z=-v.x*std::sin(yaw_)+v.z*std::cos(yaw_);
 return {x+shift_,v.y*std::cos(pitch_)-z*std::sin(pitch_)+bob_,v.y*std::sin(pitch_)+z*std::cos(pitch_)};
}
void SceneView::Triangle(V a,V b,V c,uint32_t color,float opacity,bool lit){
 if(count_==items_.size()){overflow_=true;return;}
 if(lit){V u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};V n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
 float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);float light=length>0?std::abs((n.x*-.45f+n.y*-.65f+n.z*-.6f)/length):0;
 color=Shade(color,.42f+.58f*light);}
 items_[count_++]={{{a,b,c}},color,(a.z+b.z+c.z)/3,0,static_cast<uint8_t>(std::clamp(opacity,0.f,1.f)*255),false};
}
void SceneView::Segment(V a,V b,float width,uint32_t color,float opacity){if(count_==items_.size()){overflow_=true;return;}
 items_[count_++]={{{a,b,b}},color,(a.z+b.z)/2,width,static_cast<uint8_t>(std::clamp(opacity,0.f,1.f)*255),true};}
void SceneView::RobotBox(V c,V size,uint32_t color,float opacity){Box(c,size,color,opacity,true);}
void SceneView::Box(V c,V size,uint32_t color,float opacity,bool local){
 std::array<V,8> p;for(int i=0;i<8;++i)p[i]={c.x+(i&1?1:-1)*size.x/2,c.y+(i&2?1:-1)*size.y/2,c.z+(i&4?1:-1)*size.z/2};
 if(local)for(auto& v:p)v=Transform(v);
 constexpr int f[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
 for(auto& face:f){Triangle(p[face[0]],p[face[1]],p[face[2]],color,opacity);Triangle(p[face[0]],p[face[2]],p[face[3]],color,opacity);}
}
void SceneView::Disc(V c,float rx,float ry,uint32_t color,float opacity){for(int i=0;i<20;++i){float a=2*pi*i/20,b=2*pi*(i+1)/20;
 Triangle(c,{c.x+rx*std::cos(a),c.y+ry*std::sin(a),c.z},{c.x+rx*std::cos(b),c.y+ry*std::sin(b),c.z},color,opacity,false);}}
void SceneView::Cup(V c,float r,float h,bool tea){
 for(int i=0;i<20;++i){float a=i*pi/10,b=(i+1)*pi/10;
 V p{c.x+r*std::cos(a),c.y-h/2,c.z+r*std::sin(a)},q{c.x+r*std::cos(b),c.y-h/2,c.z+r*std::sin(b)};
 V u{c.x+r*.8f*std::cos(a),c.y+h/2,c.z+r*.8f*std::sin(a)},v{c.x+r*.8f*std::cos(b),c.y+h/2,c.z+r*.8f*std::sin(b)};
 Triangle(p,q,u,ink_,morph_);Triangle(q,v,u,ink_,morph_);Triangle({c.x,c.y-h/2-.1f,c.z},p,q,tea?0x8EA98D:0x52392D,morph_);
 }
 if(tea){for(int i=0;i<6;++i)Disc({c.x-8+i%3*7,c.y+3+i/3*7,c.z-r-1},2,2,0x17251F,morph_);Segment({c.x+3,c.y-h/2,c.z-2},{c.x+5,c.y-h/2-22,c.z-2},3,ink_,morph_);}
 else {for(int i=0;i<12;++i){float a=i*pi/6,b=(i+1)*pi/6;Segment({c.x+r+5+7*std::cos(a),c.y+10*std::sin(a),c.z},{c.x+r+5+7*std::cos(b),c.y+10*std::sin(b),c.z},3,ink_,morph_);}
 for(int j=0;j<2;++j)for(int i=0;i<6;++i)Segment({c.x-5+j*10+std::sin(time_*2+i*.6f)*3,c.y-h/2-5-i*4,c.z},{c.x-5+j*10+std::sin(time_*2+(i+1)*.6f)*3,c.y-h/2-9-i*4,c.z},1,ink_,morph_*.5f);}
}
void SceneView::Head(float m,float time){
 float cy=-8*m,hw=85,hh=48+5*m,depth=1+35*m;
 // Rounded extrusion with a bevel, sampled in 24 points per ring.
 std::array<V,24> front,back,inner;
 for(int i=0;i<24;++i){int corner=i/6;float a=(-pi+corner*pi/2)+(i%6)*pi/10;
 float sx=corner==0||corner==3?-1.f:1.f,sy=corner<2?-1.f:1.f;
 float x=sx*(hw-14)+14*std::cos(a),y=sy*(hh-14)+14*std::sin(a)+cy;
 front[i]=Transform({x,y,-depth/2});back[i]=Transform({x*.96f,y,depth/2});inner[i]=Transform({x*.83f,cy+(y-cy)*.75f,-depth/2-1});}
 for(int i=0;i<24;++i){int j=(i+1)%24;Triangle(front[i],front[j],back[i],ink_);Triangle(front[j],back[j],back[i],ink_);
 Triangle(front[i],inner[i],front[j],ink_);Triangle(front[j],inner[i],inner[j],ink_);
 Triangle(Transform({0,cy,-depth/2-1}),inner[i],inner[j],theme_==ui::Theme::Black?0x001B0B:0x006D2A,1,false);}
 auto line=[&](V a,V b,float width=4){Segment(Transform(a),Transform(b),width,ink_);};
 float z=-depth/2-2,eye_y=cy-2;
 if(scene_==Scene::Nap){line({-37,eye_y,z},{-20,eye_y+2,z});line({22,eye_y,z},{38,eye_y+2,z});}
 else if(scene_==Scene::Error){for(float x:{-29.f,29.f}){line({x-7,eye_y-7,z},{x+7,eye_y+7,z});line({x-7,eye_y+7,z},{x+7,eye_y-7,z});}}
 else if(scene_==Scene::Complete||scene_==Scene::Wave){for(float x:{-29.f,29.f}){line({x-8,eye_y+3,z},{x,eye_y-5,z});line({x,eye_y-5,z},{x+8,eye_y+3,z});}}
 else {line({-34,eye_y-8,z},{-25,eye_y,z});line({-25,eye_y,z},{-34,eye_y+8,z});line({29,eye_y-9,z},{29,eye_y+9,z});}
 if(scene_==Scene::Chill){RobotBox({-29,eye_y,-depth/2-4},{29,16,3},0x17251F,m);RobotBox({29,eye_y,-depth/2-4},{29,16,3},0x17251F,m);line({-15,eye_y,z-4},{15,eye_y,z-4},2);line({-38,eye_y-4,z-6},{-30,eye_y-4,z-6},1);line({20,eye_y-4,z-6},{28,eye_y-4,z-6},1);}
 line({-13,cy+24,z},{13,cy+24,z},3);
 RobotBox({-hw-4,cy,0},{8,27,depth*.8f},ink_);RobotBox({hw+4,cy,0},{8,27,depth*.8f},ink_);
 line({-hw+18,cy-hh,z+3},{-hw+18,cy-hh-15,z+3},7);
 // A moving highlight makes the bevel and perspective legible.
 Segment(front[7],front[10],1.3f,theme_==ui::Theme::Black?0xA0FFD0:0xFFFFFF,m*.65f);
 (void)time;
}
void SceneView::Build(){
 count_=0;overflow_=false;float t=elapsed_/1000.f;float u=std::clamp(t/1.2f,0.f,1.f);morph_=u*u*(3-2*u);time_=std::max(0.f,t-1.2f);
 float m=morph_,s=std::sin(time_*2),beat=std::sin(time_*5);
 yaw_=m*(.25f+.12f*std::sin(time_*.9f));pitch_=m*-.08f;shift_=0;bob_=m*s*2;
 if(scene_==Scene::Fly||scene_==Scene::PaperPlane)yaw_=m*std::sin(time_*1.7f)*.45f;
 if(scene_==Scene::Nap)pitch_=m*(.12f+.22f*(s+1));
 if(scene_==Scene::Music||scene_==Scene::Dance){bob_=m*beat*4;pitch_+=m*beat*.08f;}
 if(scene_==Scene::Complete)bob_-=m*std::max(0.f,std::sin(time_*3))*10;
 if(scene_==Scene::Printer||scene_==Scene::Complete||scene_==Scene::Error||scene_==Scene::Plant)shift_=-28*m;
 if(scene_==Scene::Stretch){yaw_=m*std::sin(time_*.8f)*.38f;pitch_=m*std::sin(time_*.7f)*.16f;}
 if(scene_==Scene::Wave){bob_+=m*std::sin(time_*3)*3;pitch_+=m*std::sin(time_*3)*.08f;}
 // A hovering head and its soft shadow; no torso, arms, hands or legs.
 Disc({shift_,87,50},57,5,theme_==ui::Theme::Black?0x00451B:0x006D2A,m*.65f);
 Head(m,time_);
 auto line=[&](V a,V b,float w=2,uint32_t c=0){Segment(a,b,w,c?c:ink_,m);};
 auto star=[&](float x,float y,float z,float r){for(int i=0;i<4;++i){float a=i*pi/2;line({x+std::cos(a)*r,y+std::sin(a)*r,z},{x,y,z},2);}};
 switch(scene_){
 case Scene::Game:
  Box({0,58,-80},{82,26,24},ink_,m);line({-27,53,-94},{-27,65,-94},3,0x003A16);line({-33,59,-94},{-21,59,-94},3,0x003A16);
  Disc({24,55+beat,-94},3,3,0x003A16,m);Disc({32,62-beat,-94},3,3,0x003A16,m);star(95,-45+beat*3,0,9);break;
 case Scene::Coffee:case Scene::BubbleTea: Cup({76,49-std::max(0.f,s)*20,-78},14,33,scene_==Scene::BubbleTea);break;
 case Scene::Music:
  for(int i=0;i<16;++i){float a=pi+i*pi/16,b=pi+(i+1)*pi/16;line(Transform({94*std::cos(a),-8+73*std::sin(a),0}),Transform({94*std::cos(b),-8+73*std::sin(b),0}),6);}
  RobotBox({-94,-8,0},{14,35,30},ink_,m);RobotBox({94,-8,0},{14,35,30},ink_,m);
  for(int j=0;j<3;++j){float x=-105+j*103,y=-58-std::fmod(time_*16+j*22,38.f);Disc({x,y,0},5,3,ink_,m);line({x+4,y,0},{x+4,y-16,0},2);line({x+4,y-16,0},{x+11,y-12,0},2);}break;
 case Scene::Explore:{float x=91+s*9,y=23+beat*5;for(int i=0;i<24;++i){float a=i*pi/12,b=(i+1)*pi/12;line({x+19*std::cos(a),y+19*std::sin(a),-85},{x+19*std::cos(b),y+19*std::sin(b),-85},3);}line({x-14,y+14,-85},{x-35,y+39,-85},6);star(-95,-65,0,6);break;}
 case Scene::Nap:
  for(int j=0;j<3;++j){float x=70+j*12,y=-80-j*14-std::fmod(time_*8,12.f);line({x,y,0},{x+8,y,0});line({x+8,y,0},{x,y+9,0});line({x,y+9,0},{x+8,y+9,0});}break;
 case Scene::Fly:{float x=std::sin(time_*1.7f)*107,y=-57+std::sin(time_*3.4f)*40;Disc({x,y,-100},5,3,0xA9C8B5,m);Disc({x-3,y-5,-99},5,2+std::abs(beat)*3,0xDDFEEA,m*.6f);Disc({x+4,y-5,-99},5,3,0xDDFEEA,m*.6f);for(int i=1;i<5;++i)Disc({x-i*8,y+std::sin(time_*3-i)*6,-99},1,1,ink_,m*.3f);break;}
 case Scene::Printer:case Scene::Complete:case Scene::Error:{
  float x=scene_==Scene::Complete?92:85;Box({x,80,-65},{62,8,42},ink_,m);Box({x-28,37,-50},{5,82,5},ink_,m);Box({x+28,37,-50},{5,82,5},ink_,m);Box({x,-3,-50},{61,6,8},ink_,m);
  float nx=x+(scene_==Scene::Error?0:std::sin(time_*2)*17);Box({nx,20,-60},{12,16,12},ink_,m);Box({x,66,-66},{20,18+8*(s+1),20},ui::kBambuGreen,m);
  if(scene_==Scene::Error){Triangle({x-15,-25,-75},{x+15,-25,-75},{x,-52,-75},0xF47668,m,false);line({x,-45,-76},{x,-35,-76},2,0x000000);Disc({x,-30,-76},1.5,1.5,0,m);}
  if(scene_==Scene::Complete){line({-22,-83,-80},{-12,-73,-80},4);line({-12,-73,-80},{9,-94,-80},4);for(int i=0;i<16;++i){float cx=-106+(i*37%213),cy=-100+std::fmod(time_*35+i*19,185.f);Box({cx,cy,-110},{3,6,2},i%2?ink_:0xA9F48C,m);}}
  break;}
 case Scene::Race:
  Box({0,74,-65},{146,28,67},ink_,m);Box({0,52,-42},{80,18,40},ink_,m);for(float x:{-61.f,61.f}){Disc({x,89,-104},15,15,0x17251F,m);Disc({x,89,-105},6,6,ink_,m);line({x,89,-106},{x+10*std::sin(time_*8),89+10*std::cos(time_*8),-106},2);}
  for(int i=0;i<5;++i){float y=30+i*18;float x=-115+std::fmod(time_*85+i*24,32.f);line({x,y,10},{x-17,y,10},1);}break;
 case Scene::Chill:
  for(int i=0;i<3;++i)Disc({75+i*10+std::sin(time_*.5f)*10,-93,15},13,7,ink_,m*.5f);Disc({-95,-82,10},13,13,ink_,m*.5f);break;
 case Scene::Dance:
  for(int i=0;i<6;++i)Box({-70.f+i%3*47,107,10.f+i/3*30},{42,2,24},ink_,m*(.2f+.3f*(1+std::sin(time_*5+i))));star(94,-75,0,10+beat*3);break;
 case Scene::Plant:
  Box({83,80,-65},{29,29,28},ink_,m);line({83,66,-68},{83,24,-68},3);Disc({73,38,-70},12,5,ink_,m);Disc({94,29,-70},12,5,ink_,m);Box({38,30,-75},{24,18,22},ink_,m);line({50,28,-75},{66,42,-75},4);for(int j=0;j<3;++j)Disc({64.f+j*3,42+std::fmod(time_*30+j*8,28.f),-77},1.5,3,ink_,m);break;
 case Scene::PaperPlane:{float x=std::sin(time_*1.3f)*105,y=-63+std::cos(time_*1.3f)*18;Triangle({x-15,y+9,-95},{x+22,y,-95},{x-8,y-9,-85},ink_,m);Triangle({x-8,y-9,-85},{x+22,y,-95},{x-2,y+3,-105},Shade(ink_,.6f),m);break;}
 case Scene::Magic:
  line({52,46,-70},{82,-10,-70},4);star(83,-14,-72,9);Box({94,-58+beat*7,-40},{23,23,23},ink_,m);star(-92,-63,-40,6);break;
 case Scene::Stretch:star(-97,-72,0,5);star(97,-72,0,5);break;
 case Scene::Wave:
  for(int i=0;i<18;++i){float a=i*pi/9,b=(i+1)*pi/9;auto hx=[](float q){return 7*std::sin(q)*std::sin(q)*std::sin(q);};auto hy=[](float q){return -(6*std::cos(q)-2.5f*std::cos(2*q)-std::cos(3*q));};Triangle({-96,-75,0},{-96+hx(a),-75+hy(a),0},{-96+hx(b),-75+hy(b),0},ink_,m,false);}break;
 case Scene::Count:break;
 }
 std::sort(items_.begin(),items_.begin()+count_,[](const Item& a,const Item& b){return a.depth>b.depth;});
}
void SceneView::Draw(lv_layer_t* layer){
 // Reused buffers; visibility comes from per-pixel inverse-depth, including face decals.
 std::fill(pixels_.begin(),pixels_.end(),bg_|0xFF000000);std::fill(depth_.begin(),depth_.end(),0.f);
 struct P {float x,y,iz;};
 auto project=[&](V p){float iz=1.f/(520.f+p.z);return P{width_/2.f+p.x*520*iz*scale_,height_/2.f+p.y*520*iz*scale_,iz};};
 auto pixel=[&](int x,int y,float z,const Item& item,float coverage=1.f){
  auto index=static_cast<size_t>(y*width_+x);if(z+1e-8f<depth_[index])return;
  const uint32_t alpha=static_cast<uint32_t>(item.opacity*std::clamp(coverage,0.f,1.f));if(alpha==0)return;
  auto previous=pixels_[index];auto mix=[&](int shift){return ((((item.color>>shift)&255)*alpha+((previous>>shift)&255)*(255-alpha))/255)<<shift;};
  pixels_[index]=0xFF000000|mix(16)|mix(8)|mix(0);depth_[index]=z;
 };
 for(size_t i=0;i<count_;++i){const auto& item=items_[i];if(!item.opacity)continue;
  auto a=project(item.p[0]),b=project(item.p[1]),c=project(item.p[2]);
  float pad=item.line?std::max(.6f,item.width*scale_/2)+1:0;
  int x0=std::clamp(static_cast<int>(std::floor(std::min({a.x,b.x,c.x})-pad)),0,width_-1);
  int x1=std::clamp(static_cast<int>(std::ceil(std::max({a.x,b.x,c.x})+pad)),0,width_-1);
  int y0=std::clamp(static_cast<int>(std::floor(std::min({a.y,b.y,c.y})-pad)),0,height_-1);
  int y1=std::clamp(static_cast<int>(std::ceil(std::max({a.y,b.y,c.y})+pad)),0,height_-1);
  if(item.line){float dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy,r=std::max(.6f,item.width*scale_/2);
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){float t=length>0?std::clamp(((x+.5f-a.x)*dx+(y+.5f-a.y)*dy)/length,0.f,1.f):0;
    float ex=x+.5f-a.x-dx*t,ey=y+.5f-a.y-dy*t,d=std::sqrt(ex*ex+ey*ey);if(d<=r+.5f)pixel(x,y,a.iz+(b.iz-a.iz)*t,item,r+.5f-d);}
  }else{float den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(std::abs(den)<.01f)continue;
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){float u=((b.y-c.y)*(x+.5f-c.x)+(c.x-b.x)*(y+.5f-c.y))/den;
    float v=((c.y-a.y)*(x+.5f-c.x)+(a.x-c.x)*(y+.5f-c.y))/den;float w=1-u-v;
    if(u>=-.0001f&&v>=-.0001f&&w>=-.0001f)pixel(x,y,u*a.iz+v*b.iz+w*c.iz,item);}
  }
 }
 if(round_)for(int y=0;y<height_;++y)for(int x=0;x<width_;++x){float dx=x+.5f-width_/2.f,dy=y+.5f-height_/2.f;
  if(dx*dx+dy*dy>width_*width_/4.f)pixels_[y*width_+x]=0;}
 lv_area_t area;lv_obj_get_coords(root_,&area);
 lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=&image_;lv_draw_image(layer,&d,&area);
}
}
