// SPDX-License-Identifier: GPL-2.0-or-later
#include "controls_guide.h"
#include "game_type_select.h"
#include "regist.h"
#include "font.h"
#include "gui.h"
#include "ogl.h"
#include "winsys.h"
#include "psp_ui.h"
#include "audio.h"

CControlsGuide ControlsGuide;

namespace {
void Label(int x, int y, const char* title, const char* detail = nullptr) {
    FT.SetColor(sf::Color(17,51,79)); FT.SetSize(24);
    FT.DrawString(x,y,title);
    if (detail) { FT.SetSize(18); FT.DrawString(x,y+27,detail); }
}
void Leader(float x, float y, float elbowX, float elbowY, float targetX, float targetY) {
    const sf::Color ink(45,107,144);
    PspUI::Line(x,y,elbowX,elbowY,ink);
    PspUI::Line(elbowX,elbowY,targetX,targetY,ink);
    PspUI::Box(targetX-2,targetY-2,4,4,ink);
}
}

void DrawControlsGuide(bool introduction) {
    static sf::Texture diagram;
    static bool attempted = false, loaded = false;
    if (!attempted) {
        attempted = true;
        diagram.setMaximumSize(512);
        diagram.setSmooth(true);
        loaded = diagram.loadFromFile(param.tex_dir + "/psp-guide.png");
    }
    if (loaded) {
        sf::Sprite background(diagram);
        background.setScale(854.0f/512,480.0f/256);
        Winsys.draw(background);
        FT.SetColor(sf::Color(17,51,79)); FT.SetSize(32);
        FT.DrawString(CENTER,20,"YOUR PSP. YOUR SLOPES.");
        Label(180,80,"L: Brake");
        Leader(226,114,220,123,230,137);
        Label(554,80,"R: Paddle");
        Leader(616,114,629,123,623,137);
        Label(28,181,"Steer","D-pad");
        Leader(122,211,167,211,224,221);
        Label(28,276,"Steer","Analog stick");
        Leader(142,305,176,305,230,289);
        Label(710,141,"Reset","Triangle");
        Leader(699,174,669,174,627,194);
        Label(710,205,"Trick","Square + steer");
        Leader(699,237,678,244,600,222);
        Label(710,269,"Pause","Circle / back");
        Leader(699,301,681,301,654,222);
        Label(710,333,"Jump","Cross / select");
        Leader(699,365,675,365,627,249);
        Label(435,370,"Start: pause / resume");
        Leader(562,369,572,353,574,326);
        FT.SetColor(sf::Color(17,51,79)); FT.SetSize(20);
        FT.DrawString(28,380,"Jump: hold, then release Cross");
    } else {
        // A partial data install must still have a usable instruction screen.
        PspUI::Background();
        PspUI::Controls(420,113);
        PspUI::Text(36,170,"Cross: confirm in menus",22);
        PspUI::Text(36,210,"Circle: back in menus",22);
    }
    PspUI::Box(0,428,854,52,sf::Color(25,64,94,238));
    PspUI::Hint(255,439,PspUI::Start,introduction?"START - OPEN MENU":"START - BACK TO MENU");
}

void CControlsGuide::Enter() {
    ResetGUI();
    Winsys.ShowCursor(false);
    Music.Play(param.menu_music,true);
}
void CControlsGuide::Keyb(sf::Keyboard::Key key, bool release, int, int) {
    if (!release && key == sf::Keyboard::P)
        State::manager.RequestEnterState(GameTypeSelect);
}
void CControlsGuide::Loop(float) {
    ScopedRenderMode mode(GUI);
    Winsys.clear();
    DrawControlsGuide(true);
    Winsys.SwapBuffers();
}
