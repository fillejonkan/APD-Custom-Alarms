PROG     = apdcustomalarms

CFLAGS   += 
LDFLAGS  += -lm

PKGS = glib-2.0 gio-2.0 fixmath axhttp axparameter axevent
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

SRCS      = main.c cJSON.c overlays.c camera/camera.c
OBJS      = $(SRCS:.c=.o)

all: $(PROG) $(OBJS)

$(PROG): $(OBJS)
	$(CC) $^ $(CFLAGS) $(LIBS) $(LDFLAGS) -lm $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROG) $(OBJS)
