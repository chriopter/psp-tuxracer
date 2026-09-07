TARGET = extremetuxracer
GAME_CPP = $(wildcard ../src/*.cpp)
OBJS = $(GAME_CPP:.cpp=.o) platform.o gl_missing.o
INCDIR = include ../src
CFLAGS = -O2 -G0 -g -MMD -MP -I$(PSPDEV)/psp/include/SDL -fno-strict-aliasing -fsingle-precision-constant
CXXFLAGS = $(CFLAGS) -std=gnu++17 -fno-rtti
ASFLAGS = $(CFLAGS)
LDFLAGS = -Wl,--wrap=glBindTexture,--wrap=glTexImage2D,--wrap=glEnable,--wrap=glDisable
LIBS = -lGLU -lGL -lSDL_image -lSDL_ttf -lSDL_mixer -lSDL -lpng -ljpeg -lfreetype -lbz2 -lz -lvorbisidec -lvorbisfile -lvorbis -logg -lFLAC -lmodplug -lmpg123 -lstdc++ -lpspaudio -lpspaudiolib -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsprtc -lpsppower -lpspvfpu -lpsphprm -lpspirkeyb -lm
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Extreme Tux Racer 0.8.4
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
-include $(OBJS:.o=.d)
