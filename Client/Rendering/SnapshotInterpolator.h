#pragma once
#include "Common/Protocol/WorldSnapshot.h"
namespace sandbox {
// Presentation-only, one snapshot interval behind. Never extrapolates server physics.
class SnapshotInterpolator {
    WorldSnapshot from_, target_;
    double elapsed_{}, duration_{1.0/20.0};
    bool initialized_{};
public:
    void advance(double dt) {
        if (std::isfinite(dt) && dt>0) elapsed_=std::min(duration_,elapsed_+dt);
    }
    void push(const WorldSnapshot& next) {
        if (initialized_ && next.tick<=target_.tick) return;
        from_=initialized_ ? sample() : next;
        duration_=initialized_ ? std::clamp(static_cast<double>(next.tick-target_.tick)/60.0,1.0/60.0,0.1) : 0.05;
        target_=next; elapsed_=0; initialized_=true;
    }
    WorldSnapshot sample() const {
        WorldSnapshot result=target_;
        const double t=std::clamp(elapsed_/duration_,0.0,1.0);
        for (auto& p : result.players) {
            const auto old=std::find_if(from_.players.begin(),from_.players.end(),[&](const auto& v) { return v.id==p.id; });
            if (old==from_.players.end()) continue;
            p.position=old->position*(1-t)+p.position*t;
            p.velocity=old->velocity*(1-t)+p.velocity*t;
            p.angularVelocity=old->angularVelocity*(1-t)+p.angularVelocity*t;
            p.orientation=interpolate(old->orientation,p.orientation,t);
        }
        return result;
    }
};
}
