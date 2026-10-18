#pragma once
#include <lvgl.h>
#include <array>
#include <cmath>

namespace bambu::ui {
// Original LVGL redraw of the user-supplied framed 3D printer reference.
// Shared geometry with docs/ui/printer-icon.svg; no raster or watermark assets.
namespace printer_icon_detail {
inline constexpr lv_point_precise_t path_0[] = {{5,8},{6,4},{9,1},{171,1},{174,4},{175,8},{175,174},{173,179},{170,181},{10,181},{6,179},{5,175},{5,8}};
inline constexpr lv_point_precise_t path_1[] = {{5,23},{175,23}};
inline constexpr lv_point_precise_t path_2[] = {{5,145},{175,145}};
inline constexpr lv_point_precise_t path_3[] = {{21,23},{21,145}};
inline constexpr lv_point_precise_t path_4[] = {{27,23},{27,145}};
inline constexpr lv_point_precise_t path_5[] = {{153,23},{153,145}};
inline constexpr lv_point_precise_t path_6[] = {{159,23},{159,145}};
inline constexpr lv_point_precise_t path_7[] = {{22,38},{32,38},{32,56},{22,56},{22,38}};
inline constexpr lv_point_precise_t path_8[] = {{149,38},{159,38},{159,56},{149,56},{149,38}};
inline constexpr lv_point_precise_t path_9[] = {{32,43},{75,43}};
inline constexpr lv_point_precise_t path_10[] = {{32,51},{75,51}};
inline constexpr lv_point_precise_t path_11[] = {{104,43},{149,43}};
inline constexpr lv_point_precise_t path_12[] = {{104,51},{149,51}};
inline constexpr lv_point_precise_t path_13[] = {{85,34},{85,21},{87,15},{91,11},{97,8},{105,8},{111,10},{117,15},{120,21},{120,23}};
inline constexpr lv_point_precise_t path_14[] = {{80,34},{80,30},{91,30},{91,34}};
inline constexpr lv_point_precise_t path_15[] = {{78,34},{101,34},{104,37},{104,60},{101,63},{77,63},{75,60},{75,37},{78,34}};
inline constexpr lv_point_precise_t path_16[] = {{82,42},{96,42}};
inline constexpr lv_point_precise_t path_17[] = {{82,49},{96,49}};
inline constexpr lv_point_precise_t path_18[] = {{83,63},{83,68},{95,68},{95,63}};
inline constexpr lv_point_precise_t path_19[] = {{86,68},{88,73},{91,73},{93,68}};
inline constexpr lv_point_precise_t path_20[] = {{63,89},{86,81},{109,89},{109,116},{86,126},{63,117},{63,89}};
inline constexpr lv_point_precise_t path_21[] = {{63,89},{86,98},{109,89}};
inline constexpr lv_point_precise_t path_22[] = {{86,98},{86,126}};
inline constexpr lv_point_precise_t path_23[] = {{63,119},{39,119},{32,130},{140,130},{133,119},{105,119}};
inline constexpr lv_point_precise_t path_24[] = {{32,130},{32,136},{140,136},{140,130}};
inline constexpr lv_point_precise_t path_25[] = {{43,136},{43,145}};
inline constexpr lv_point_precise_t path_26[] = {{49,136},{49,145}};
inline constexpr lv_point_precise_t path_27[] = {{123,136},{123,145}};
inline constexpr lv_point_precise_t path_28[] = {{129,136},{129,145}};
inline constexpr lv_point_precise_t path_29[] = {{31,153},{69,153},{71,155},{71,171},{69,173},{31,173},{29,171},{29,155},{31,153}};
inline constexpr lv_point_precise_t path_30[] = {{13,181},{13,186},{16,188},{26,188},{29,186},{29,181}};
inline constexpr lv_point_precise_t path_31[] = {{151,181},{151,186},{154,188},{164,188},{167,186},{167,181}};
inline constexpr lv_point_precise_t path_32[] = {{152,163},{152,165},{151,167},{149,169},{147,170},{145,170},{143,170},{141,169},{139,167},{138,165},{138,163},{138,161},{139,159},{141,157},{143,156},{145,156},{147,156},{149,157},{151,159},{152,161},{152,163}};
struct Path {const lv_point_precise_t* points;size_t count;};
inline constexpr std::array<Path,33> paths{{
    {path_0,std::size(path_0)},
    {path_1,std::size(path_1)},
    {path_2,std::size(path_2)},
    {path_3,std::size(path_3)},
    {path_4,std::size(path_4)},
    {path_5,std::size(path_5)},
    {path_6,std::size(path_6)},
    {path_7,std::size(path_7)},
    {path_8,std::size(path_8)},
    {path_9,std::size(path_9)},
    {path_10,std::size(path_10)},
    {path_11,std::size(path_11)},
    {path_12,std::size(path_12)},
    {path_13,std::size(path_13)},
    {path_14,std::size(path_14)},
    {path_15,std::size(path_15)},
    {path_16,std::size(path_16)},
    {path_17,std::size(path_17)},
    {path_18,std::size(path_18)},
    {path_19,std::size(path_19)},
    {path_20,std::size(path_20)},
    {path_21,std::size(path_21)},
    {path_22,std::size(path_22)},
    {path_23,std::size(path_23)},
    {path_24,std::size(path_24)},
    {path_25,std::size(path_25)},
    {path_26,std::size(path_26)},
    {path_27,std::size(path_27)},
    {path_28,std::size(path_28)},
    {path_29,std::size(path_29)},
    {path_30,std::size(path_30)},
    {path_31,std::size(path_31)},
    {path_32,std::size(path_32)},
}};
}
class PrinterIconView {
public:
    PrinterIconView(lv_obj_t* parent,int width,uint32_t color,int stroke=1) {
        root_=lv_obj_create(parent);lv_obj_remove_style_all(root_);
        lv_obj_set_size(root_,width,static_cast<int>(std::lround(width*192.f/180)));
        lv_obj_remove_flag(root_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE));
        for(size_t i=0;i<lines_.size();++i) {
            const auto& path=printer_icon_detail::paths[i];
            for(size_t j=0;j<path.count;++j)points_[i][j]={
                static_cast<lv_value_precise_t>(std::lround(path.points[j].x*width/180.f)),
                static_cast<lv_value_precise_t>(std::lround(path.points[j].y*width/180.f))};
            lines_[i]=lv_line_create(root_);lv_line_set_points(lines_[i],points_[i].data(),path.count);
            lv_obj_set_style_line_width(lines_[i],stroke,0);lv_obj_set_style_line_rounded(lines_[i],true,0);
            lv_obj_remove_flag(lines_[i],LV_OBJ_FLAG_CLICKABLE);
        }
        SetColor(color);
    }
    ~PrinterIconView(){lv_obj_delete(root_);}
    PrinterIconView(const PrinterIconView&)=delete;
    PrinterIconView& operator=(const PrinterIconView&)=delete;
    lv_obj_t* root() const {return root_;}
    void SetColor(uint32_t color){for(auto line:lines_)lv_obj_set_style_line_color(line,lv_color_hex(color),0);}
private:
    lv_obj_t* root_=nullptr;
    std::array<lv_obj_t*,printer_icon_detail::paths.size()> lines_{};
    std::array<std::array<lv_point_precise_t,24>,printer_icon_detail::paths.size()> points_{};
};
}
