#ifndef ETR_CONTROLS_GUIDE_H
#define ETR_CONTROLS_GUIDE_H
#include "states.h"
class CControlsGuide : public State {
    void Enter() override;
    void Loop(float time_step) override;
    void Keyb(sf::Keyboard::Key key, bool release, int x, int y) override;
};
extern CControlsGuide ControlsGuide;
void DrawControlsGuide(bool introduction);
#endif
