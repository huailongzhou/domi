#ifndef DOMI_CLAY_LAYOUT_H
#define DOMI_CLAY_LAYOUT_H

/* Pure C interface over third_party/clay (clay.h is included only by
 * clay_layout.c, compiled as C99). C++ translation units include this
 * header and therefore never see clay.h, so they can stay C++11. */

#ifdef __cplusplus
extern "C" {
#endif

/* Box (container) declaration. Colors are 0-255 floats. */
typedef struct ClayLayBox {
    const char* id;        /* optional, may be NULL */
    float width;           /* <= 0 means "fit content" */
    float height;          /* <= 0 means "fit content" */
    int horizontal;        /* 0 = top-to-bottom, 1 = left-to-right */
    int childGap;
    int alignX;            /* 0 = left, 1 = center, 2 = right */
    int alignY;            /* 0 = top, 1 = center, 2 = bottom */
    int paddingL, paddingR, paddingT, paddingB;
    float bgR, bgG, bgB, bgA;          /* bgA <= 0: no rectangle */
    float borderR, borderG, borderB, borderA;
    int borderWidth;                   /* <= 0: no border */
    float cornerRadius;                /* layout-only for now */
} ClayLayBox;

/* Render command produced by claylay_end(). */
enum {
    CLAYLAY_CMD_RECT = 1,
    CLAYLAY_CMD_BORDER = 2,
    CLAYLAY_CMD_TEXT = 3,
    CLAYLAY_CMD_CLIP_START = 4,
    CLAYLAY_CMD_CLIP_END = 5
};

typedef struct ClayLayCmd {
    int type;
    float x, y, w, h;
    float r, g, b, a;      /* 0-255 */
    float lineWidth;       /* border only */
    int fontSize;          /* text only */
    char text[512];        /* text only, null-terminated */
} ClayLayCmd;

/* Text measurement bridge: the C++ side registers a callback that maps
 * (text, fontSize) to pixel dimensions. */
typedef void (*ClayLayMeasureFn)(const char* text, int fontSize,
                                 float* outW, float* outH, void* userData);

int  claylay_init(void);
void claylay_shutdown(void);
void claylay_set_measure(ClayLayMeasureFn fn, void* userData);

void claylay_begin(float width, float height, float mouseX, float mouseY, int mouseDown);
/* Runs the layout and stores the render commands internally; fetch them
 * with claylay_commands(). */
void claylay_end(void);

void claylay_box_open(const ClayLayBox* box);
void claylay_box_close(void);
void claylay_text(const char* text, int fontSize, float r, float g, float b, float a);

int claylay_hovered(const char* id);

const ClayLayCmd* claylay_commands(int* count);

#ifdef __cplusplus
}
#endif

#endif /* DOMI_CLAY_LAYOUT_H */
