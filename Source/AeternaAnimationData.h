#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <stdexcept>
namespace aeterna {
struct Frame { int x,y,w,h,pivotX,pivotY; };
struct Step { int frame; double ms; };
struct Clip { int first,count; bool loop; int next; double rotation; };
inline constexpr int atlasWidth=2048, atlasHeight=2560;
inline constexpr Frame frames[] = {
 {0,0,256,256,128,232},
 {256,0,256,256,128,232},
 {512,0,256,256,128,232},
 {768,0,256,256,128,232},
 {1024,0,256,256,128,232},
 {1280,0,256,256,128,232},
 {1536,0,256,256,128,232},
 {1792,0,256,256,128,232},
 {0,256,256,256,128,232},
 {256,256,256,256,128,232},
 {512,256,256,256,128,232},
 {768,256,256,256,128,232},
 {1024,256,256,256,128,232},
 {1280,256,256,256,128,232},
 {1536,256,256,256,128,232},
 {1792,256,256,256,128,232},
 {0,512,256,256,128,232},
 {256,512,256,256,128,232},
 {512,512,256,256,128,232},
 {768,512,256,256,128,232},
 {1024,512,256,256,128,232},
 {1280,512,256,256,128,232},
 {1536,512,256,256,128,232},
 {1792,512,256,256,128,232},
 {0,768,256,256,128,232},
 {256,768,256,256,128,232},
 {512,768,256,256,128,232},
 {768,768,256,256,128,232},
 {1024,768,256,256,128,232},
 {1280,768,256,256,128,232},
 {1536,768,256,256,128,232},
 {1792,768,256,256,128,232},
 {0,1024,256,256,128,232},
 {256,1024,256,256,128,232},
 {512,1024,256,256,128,232},
 {768,1024,256,256,128,232},
 {1024,1024,256,256,128,232},
 {1280,1024,256,256,128,232},
 {1536,1024,256,256,128,232},
 {1792,1024,256,256,128,232},
 {0,1280,256,256,128,232},
 {256,1280,256,256,128,232},
 {512,1280,256,256,128,232},
 {768,1280,256,256,128,232},
 {1024,1280,256,256,128,232},
 {1280,1280,256,256,128,232},
 {1536,1280,256,256,128,232},
 {1792,1280,256,256,128,232},
 {0,1536,256,256,128,232},
 {256,1536,256,256,128,232},
 {512,1536,256,256,128,232},
 {768,1536,256,256,128,232},
 {1024,1536,256,256,128,232},
 {1280,1536,256,256,128,232},
 {1536,1536,256,256,128,232},
 {1792,1536,256,256,128,232},
 {0,1792,256,256,128,232},
 {256,1792,256,256,128,232},
 {512,1792,256,256,128,232},
 {768,1792,256,256,128,232},
 {1024,1792,256,256,128,232},
 {1280,1792,256,256,128,232},
 {1536,1792,256,256,128,232},
 {1792,1792,256,256,128,232},
 {0,2048,256,256,128,232},
 {256,2048,256,256,128,232},
 {512,2048,256,256,128,232},
 {768,2048,256,256,128,232},
 {1024,2048,256,256,128,232},
 {1280,2048,256,256,128,232},
 {1536,2048,256,256,128,232},
 {1792,2048,256,256,128,232},
 {0,2304,256,256,128,232},
 {256,2304,256,256,128,232},
 {512,2304,256,256,128,232},
 {768,2304,256,256,128,232},
};
inline constexpr std::string_view frameNames[] = {"jump_stand","jump_anticipate","jump_takeoff","jump_rise","jump_apex","jump_fall","jump_land","jump_recover","dance_stand","dance_enter","dance_left","dance_center","dance_right","dance_cheer","dance_dab","dance_exit","antics_startle","antics_stumble","antics_sit_down","antics_sit_idle","antics_sit_wave","antics_get_up_low","antics_get_up_high","antics_recover","walk_stand","walk_contact_a","walk_down","walk_pass_a","walk_contact_b","walk_pass_b","walk_dash","walk_brake","idle_breathe_0","idle_breathe_1","idle_breathe_2","idle_breathe_3","ballet_neutral","ballet_plie","ballet_rounded","ballet_overhead","ballet_arabesque","ballet_passe","ballet_lower","ballet_settle","spin_front","spin_front_right","spin_right","spin_back_right","spin_back","spin_back_left","spin_left","spin_front_left","kira_neutral","kira_excited","kira_raise_v","kira_pose_a","kira_pose_b","kira_frame_face","kira_release","kira_settle","social_neutral","social_enter","social_cross_step","social_open","social_side_step","social_shuffle","social_point","social_settle","refined_neutral","refined_open","refined_cross_step","refined_sway_left","refined_pass","refined_sway_right","refined_curtsy","refined_settle"};
inline constexpr std::string_view clipNames[] = {"idle","walk_start","walk","walk_stop","dash","jump_start","jump_air","land","dance_enter","dance","dance_exit","cheer","dab","stumble","sit_down","sit","wave","get_up","somersault","ballet_enter","ballet","ballet_exit","arabesque","spin_enter","spin","spin_exit","spin_once","kira_enter","kira_hold","kira_exit","kira_flourish","social_enter","social_dance","social_exit","social_finish","refined_enter","refined_dance","refined_exit","curtsy"};
inline constexpr Step steps[] = {{32,350},{33,350},{34,350},{35,350},{0,90},{24,90},{27,90},{25,90},{26,90},{27,90},{28,90},{29,90},{24,90},{31,100},{24,100},{0,100},{30,70},{29,70},{28,70},{27,70},{1,90},{2,90},{3,150},{4,150},{5,150},{6,90},{7,100},{0,90},{8,100},{9,100},{11,100},{10,150},{11,150},{12,150},{11,150},{11,110},{15,110},{0,110},{9,120},{13,450},{15,120},{0,120},{9,100},{14,400},{11,100},{15,100},{16,140},{17,140},{18,140},{1,120},{21,120},{18,120},{19,700},{19,120},{20,350},{19,120},{20,350},{21,130},{22,130},{23,130},{0,130},{2,120},{3,120},{4,120},{4,120},{5,120},{0,130},{36,130},{37,130},{38,130},{38,160},{39,260},{40,360},{41,260},{38,160},{38,150},{42,150},{43,150},{0,150},{36,150},{37,150},{38,150},{40,550},{41,150},{42,150},{43,150},{0,150},{0,120},{38,120},{44,120},{44,85},{45,85},{46,85},{47,85},{48,85},{49,85},{50,85},{51,85},{44,130},{38,130},{43,130},{0,130},{0,110},{38,110},{44,110},{45,110},{46,110},{47,110},{48,110},{49,110},{50,110},{51,110},{44,110},{38,110},{43,110},{0,110},{52,110},{53,110},{54,110},{55,110},{55,600},{54,110},{58,110},{59,110},{0,110},{52,120},{53,120},{54,120},{55,400},{56,400},{57,350},{58,120},{59,120},{0,120},{0,110},{60,110},{61,110},{62,150},{63,150},{64,150},{65,150},{61,150},{61,130},{67,130},{0,130},{61,140},{62,140},{63,140},{64,140},{65,140},{66,400},{67,140},{0,140},{0,160},{68,160},{69,160},{70,160},{70,220},{71,220},{72,220},{73,220},{72,220},{72,180},{74,180},{75,180},{0,180},{68,160},{69,160},{72,160},{74,500},{75,160},{0,160}};
inline constexpr Clip clips[] = {{0,4,true,0,0},{4,3,false,2,0},{7,6,true,0,0},{13,3,false,0,0},{16,4,true,0,0},{20,2,false,6,0},{22,3,false,-1,0},{25,3,false,0,0},{28,3,false,9,0},{31,4,true,0,0},{35,3,false,0,0},{38,4,false,0,0},{42,4,false,0,0},{46,3,false,15,0},{49,3,false,15,0},{52,1,true,0,0},{53,4,false,15,0},{57,4,false,0,0},{61,5,false,7,360},{66,4,false,20,0},{70,5,true,0,0},{75,4,false,0,0},{79,8,false,0,0},{87,3,false,24,0},{90,8,true,0,0},{98,4,false,0,0},{102,14,false,0,0},{116,4,false,28,0},{120,1,true,0,0},{121,4,false,0,0},{125,9,false,0,0},{134,3,false,32,0},{137,5,true,0,0},{142,3,false,0,0},{145,8,false,0,0},{153,4,false,36,0},{157,5,true,0,0},{162,4,false,0,0},{166,6,false,0,0}};

class Player {
 public:
  bool play(std::string_view name) {
    for (int i=0;i<(int)std::size(clipNames);++i) if(clipNames[i]==name) {
      state=i;step=0;elapsed=0;clipElapsed=0;held=false;return true;
    } return false;
  }
  void advance(double dtMs) {
    if(!std::isfinite(dtMs)||dtMs<=0||held)return;
    // Prevent a stalled UI from iterating thousands of frames; the host should
    // reset after seek/loop/reload and reschedule note events from transport time.
    dtMs=std::min(dtMs,1000.0);
    while(dtMs>0&&!held) {
      const auto c=clips[state];const auto s=steps[c.first+step];
      const double take=std::min(dtMs,s.ms-elapsed);
      elapsed+=take;clipElapsed+=take;dtMs-=take;
      if(elapsed>=s.ms-1e-8) {
        elapsed=0;
        if(++step>=c.count) {
          if(c.loop){step=0;clipElapsed=0;}
          else if(c.next>=0){state=c.next;step=0;clipElapsed=0;}
          else {step=c.count-1;elapsed=s.ms;held=true;}
        }
      }
    }
  }
  const Frame& frame() const {return frames[steps[clips[state].first+step].frame];}
  std::string_view frameName() const {return frameNames[steps[clips[state].first+step].frame];}
  std::string_view stateName() const {return clipNames[state];}
  bool awaitingExternalEvent() const{return held;}
  double rotationDegrees() const {
    const auto c=clips[state];double total=0;
    for(int i=0;i<c.count;++i)total+=steps[c.first+i].ms;
    return total>0?c.rotation*std::clamp(clipElapsed/total,0.0,1.0):0;
  }
 private:
  int state=0,step=0;double elapsed=0,clipElapsed=0;bool held=false;
};
} // namespace aeterna
