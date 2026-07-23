#pragma once

class ScrollController {
public:
    float wallSpeed()  const { return wallSpeed_; }
    float floorSpeed() const { return floorSpeed_; }

    void setWallSpeed(float periodsPerSecond)  { wallSpeed_  = periodsPerSecond; }
    void setFloorSpeed(float periodsPerSecond) { floorSpeed_ = periodsPerSecond; }

private:
    float wallSpeed_  = 1.65f;
    float floorSpeed_ = 1.55f;
};
