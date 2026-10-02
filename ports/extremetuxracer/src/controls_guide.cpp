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
#include "translation.h"

CControlsGuide ControlsGuide;

namespace {
// The largest of the sizes at which the text is no wider than its room: a
// translation is often longer than the English the picture was laid out for.
void Fit(const sf::String& text, int size, int room) {
    for (; size > 12; --size) {
        FT.SetSize(size);
        if (FT.GetTextWidth(text) <= room) return;
    }
}
void Label(int x, int y, const sf::String& title, const sf::String& detail = sf::String(), int room = 144) {
    FT.SetColor(sf::Color(17,51,79));
    Fit(title, 24, room);
    FT.DrawString(x,y,title);
    if (!detail.isEmpty()) { Fit(detail, 18, room); FT.DrawString(x,y+27,detail); }
}
// A label that ends where the others begin: at the line that leads from it.
void LabelLeft(int right, int y, const sf::String& title) {
    FT.SetColor(sf::Color(17,51,79));
    Fit(title, 24, right - 20);
    FT.DrawString(right - (int)FT.GetTextWidth(title), y, title);
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
        // A photograph of a PSP-1000 (Evan-Amos, public domain) with a real
        // frame of the game set into its screen; see docs/artwork/psp-guide.md.
        // The labels and lines are drawn here, onto the buttons as they
        // stand in the photograph.
        sf::Sprite background(diagram);
        background.setScale(854.0f/512,480.0f/256);
        Winsys.draw(background);
        FT.SetColor(sf::Color(17,51,79)); FT.SetSize(32);
        FT.DrawString(CENTER,14,Trans.Text(TXT_GUIDE_TITLE));
        LabelLeft(232,66,sf::String("L: ") + Trans.Text(TXT_BRAKE));
        Leader(226,96,248,102,269,109);
        Label(640,100,sf::String("R: ") + Trans.Text(TXT_PADDLE),sf::String(),206);
        Leader(690,134,650,148,620,156);
        Label(28,158,Trans.Text(TXT_STEER),Trans.Text(TXT_DPAD),150);
        Leader(110,188,180,190,236,184);
        Label(28,250,Trans.Text(TXT_STEER),Trans.Text(TXT_ANALOG),150);
        Leader(142,280,180,264,215,248);
        Label(710,156,Trans.Text(TXT_RESET),Trans.Text(TXT_TRIANGLE),138);
        Leader(703,186,650,204,607,215);
        Label(710,216,Trans.Text(TXT_TRICK),Trans.Text(TXT_TRICK_KEYS),138);
        Leader(703,246,650,228,574,240);
        Label(710,276,Trans.Text(TXT_BACK),Trans.Text(TXT_BACK_KEYS),138);
        Leader(703,306,668,270,631,252);
        Label(710,336,Trans.Text(TXT_JUMP),Trans.Text(TXT_JUMP_KEYS),138);
        Leader(703,366,640,330,596,276);
        Label(400,376,Trans.Text(TXT_START_PAUSE),sf::String(),300);
        Leader(520,378,522,356,522,337);
        FT.SetColor(sf::Color(17,51,79));
        Fit(Trans.Text(TXT_JUMP_HOW), 20, 360);
        FT.DrawString(28,392,Trans.Text(TXT_JUMP_HOW));
    } else {
        // A partial data install must still have a usable instruction screen.
        PspUI::Background();
        PspUI::Controls(420,113);
        PspUI::Text(36,170,"Cross: confirm in menus",22);
        PspUI::Text(36,210,"Circle: back in menus",22);
    }
    PspUI::Box(0,428,854,52,sf::Color(25,64,94,238));
    PspUI::Hint(255,439,PspUI::Start,Trans.Text(introduction ? TXT_START_MENU : TXT_START_BACK));
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
