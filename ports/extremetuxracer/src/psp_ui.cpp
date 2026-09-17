// Lightweight PSP-native menu artwork; no additional textures or asset loads.
#include "psp_ui.h"
#include "gui.h"
#include "font.h"
#include "ogl.h"
#include "winsys.h"
#include <cmath>
namespace PspUI {
void Box(int x, int y, int w, int h, sf::Color color) {
    DrawFrameX(x, y, w, h, 0, color, color, 1.0f);
}
void Text(int x, int y, const char* text, unsigned size) {
    FT.SetColor(sf::Color(225, 239, 248)); FT.SetSize(size);
    FT.DrawString(x, y, text);
}
void Line(float x, float y, float xx, float yy, sf::Color c) {
    float len = std::sqrt((xx-x)*(xx-x)+(yy-y)*(yy-y));
    float dx = -(yy-y)*1.5f/len, dy = (xx-x)*1.5f/len;
    GLfloat v[] = {x+dx,y+dy, xx+dx,yy+dy, xx-dx,yy-dy,
                   x+dx,y+dy, xx-dx,yy-dy, x-dx,y-dy};
    glDisable(GL_TEXTURE_2D); glColor4ub(c.r,c.g,c.b,255);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2,GL_FLOAT,0,v); glDrawArrays(GL_TRIANGLES,0,6);
    glDisableClientState(GL_VERTEX_ARRAY); glEnable(GL_TEXTURE_2D);
}
void Icon(int x, int y, Button b) {
    // One 256x128 atlas (64 KiB in RGBA4444), shared by all menu states.
    // The original vector symbols remain a safe fallback for incomplete installs.
    static sf::Texture atlas;
    static bool attempted = false;
    static bool loaded = false;
    if (!attempted) {
        attempted = true;
        atlas.setSmooth(true);
        loaded = atlas.loadFromFile(param.tex_dir + "/psp-controls.png");
    }
    if (loaded) {
        sf::Sprite sprite;
        sprite.setTexture(atlas);
        sprite.setTextureRect(sf::IntRect((int(b)%4)*64, (int(b)/4)*64, 64, 64));
        sprite.setScale(0.625f, 0.625f);
        sprite.setPosition(x-4, y-4);
        Winsys.draw(sprite);
        return;
    }
    sf::Color c(126, 196, 255);
    if (b==Circle) c=sf::Color(255,137,155);
    if (b==Square) c=sf::Color(228,163,238);
    if (b==Triangle) c=sf::Color(112,232,186);
    Box(x,y,32,32,sf::Color(27,48,69));
    if (b==Cross) { Line(x+9,y+9,x+23,y+23,c); Line(x+23,y+9,x+9,y+23,c); }
    else if (b==Circle) {
        for(int i=0;i<20;i++) {
            float a=i*6.2831853f/20, z=(i+1)*6.2831853f/20;
            Line(x+16+10*std::cos(a),y+16+10*std::sin(a),x+16+10*std::cos(z),y+16+10*std::sin(z),c);
        }
    } else if (b==Square) {
        Line(x+7,y+7,x+25,y+7,c); Line(x+25,y+7,x+25,y+25,c);
        Line(x+25,y+25,x+7,y+25,c); Line(x+7,y+25,x+7,y+7,c);
    } else if (b==Triangle) {
        Line(x+16,y+6,x+27,y+25,c); Line(x+27,y+25,x+5,y+25,c); Line(x+5,y+25,x+16,y+6,c);
    } else if (b==Dpad) {
        Box(x+13,y+5,6,22,c); Box(x+5,y+13,22,6,c);
    } else if(b==Start) {
        Line(x+8,y+8,x+24,y+16,c); Line(x+24,y+16,x+8,y+24,c); Line(x+8,y+24,x+8,y+8,c);
    } else Text(x+9,y+1,b==ShoulderL?"L":"R",24);
}
void Hint(int x,int y,Button button,const char* text) { Icon(x,y,button); Text(x+44,y+1,text); }
void Background() {
    Box(0,0,854,480,sf::Color(12,25,41));
    Box(0,0,854,106,sf::Color(18,38,58));
    Box(32,103,790,2,sf::Color(66,174,212));
    Box(32,427,790,1,sf::Color(44,69,88));
    Text(36,22,"EXTREME TUX RACER",40);
    Text(38,70,"PSP EDITION  /  HIT THE SLOPES",18);
}
void Controls(int x,int y) {
    Text(x,y,"ON THE SLOPES",24);
    Hint(x,y+42,Dpad,"Steer / analog stick");
    Hint(x,y+82,Cross,"Hold, release: jump");
    Hint(x,y+122,Square,"Hold + direction: trick");
    Hint(x,y+162,Triangle,"Reset to course");
    Hint(x,y+202,ShoulderL,"Brake / D-pad down");
    Hint(x,y+242,ShoulderR,"Paddle / D-pad up");
    Hint(x,y+282,Start,"Start: pause / controls");
}
void Confirm(const char* title,const char* detail) {
    Box(0,0,854,480,sf::Color(5,13,23,220));
    Box(118,151,618,190,sf::Color(22,45,65));
    Box(118,151,618,3,sf::Color(107,211,232));
    Text(148,176,title,30); Text(148,222,detail,22);
    Hint(148,285,Cross,"Confirm"); Hint(450,285,Circle,"Cancel");
}
}
