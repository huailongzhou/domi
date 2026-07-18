/* C99 translation unit: the only file that includes clay.h. Implements the
 * C interface declared in clay_layout.h so the rest of the engine (C++11)
 * never touches clay.h. */

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "clay_layout.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CLAYLAY_MAX_COMMANDS 2048

static void* s_arenaMemory = NULL;
static ClayLayMeasureFn s_measureFn = NULL;
static void* s_measureUser = NULL;
static ClayLayCmd s_commands[CLAYLAY_MAX_COMMANDS];
static int s_commandCount = 0;

static Clay_String claylay_str(const char* s) {
    Clay_String cs;
    cs.isStaticallyAllocated = false;
    cs.length = (int32_t)strlen(s);
    cs.chars = s;
    return cs;
}

static Clay_SizingAxis sizingOf(float v) {
    Clay_SizingAxis a;
    memset(&a, 0, sizeof(a));
    if (v > 0.0f) {
        a.type = CLAY__SIZING_TYPE_FIXED;
        a.size.minMax.min = v;
        a.size.minMax.max = v;
    } else {
        a.type = CLAY__SIZING_TYPE_FIT;
    }
    return a;
}

static Clay_LayoutAlignmentX alignXOf(int a) {
    switch (a) {
        case 0: return CLAY_ALIGN_X_LEFT;
        case 2: return CLAY_ALIGN_X_RIGHT;
        default: return CLAY_ALIGN_X_CENTER;
    }
}

static Clay_LayoutAlignmentY alignYOf(int a) {
    switch (a) {
        case 0: return CLAY_ALIGN_Y_TOP;
        case 2: return CLAY_ALIGN_Y_BOTTOM;
        default: return CLAY_ALIGN_Y_CENTER;
    }
}

static Clay_Dimensions measureWrapper(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    Clay_Dimensions dims = { 0.0f, 0.0f };
    char buf[512];
    size_t len;
    (void)userData;
    if (!s_measureFn || !config) return dims;
    len = text.length < 0 ? 0 : (size_t)text.length;
    if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
    memcpy(buf, text.chars, len);
    buf[len] = '\0';
    s_measureFn(buf, config->fontSize > 0 ? config->fontSize : 20,
                &dims.width, &dims.height, s_measureUser);
    return dims;
}

static void errorHandler(Clay_ErrorData errorData) {
    fprintf(stderr, "[CLAY] error %d: %.*s\n", (int)errorData.errorType,
            (int)errorData.errorText.length, errorData.errorText.chars);
}

int claylay_init(void) {
    uint32_t memSize;
    Clay_Arena arena;
    Clay_ErrorHandler handler;

    if (s_arenaMemory) return 1; /* already initialized */

    memSize = Clay_MinMemorySize();
    s_arenaMemory = malloc(memSize);
    if (!s_arenaMemory) {
        fprintf(stderr, "[CLAY] arena allocation failed (%u bytes)\n", memSize);
        return 0;
    }
    arena = Clay_CreateArenaWithCapacityAndMemory(memSize, s_arenaMemory);
    handler.errorHandlerFunction = errorHandler;
    handler.userData = NULL;
    Clay_Initialize(arena, (Clay_Dimensions){ 1280.0f, 720.0f }, handler);
    Clay_SetMeasureTextFunction(measureWrapper, NULL);
    return 1;
}

void claylay_shutdown(void) {
    free(s_arenaMemory);
    s_arenaMemory = NULL;
}

void claylay_set_measure(ClayLayMeasureFn fn, void* userData) {
    s_measureFn = fn;
    s_measureUser = userData;
}

void claylay_begin(float width, float height, float mouseX, float mouseY, int mouseDown) {
    Clay_SetLayoutDimensions((Clay_Dimensions){ width, height });
    Clay_SetPointerState((Clay_Vector2){ mouseX, mouseY }, mouseDown != 0);
    Clay_BeginLayout();
}

void claylay_box_open(const ClayLayBox* box) {
    Clay_ElementDeclaration d;
    if (!box) return;
    memset(&d, 0, sizeof(d));

    d.layout.sizing.width = sizingOf(box->width);
    d.layout.sizing.height = sizingOf(box->height);
    d.layout.padding.left = (uint16_t)box->paddingL;
    d.layout.padding.right = (uint16_t)box->paddingR;
    d.layout.padding.top = (uint16_t)box->paddingT;
    d.layout.padding.bottom = (uint16_t)box->paddingB;
    d.layout.childGap = (uint16_t)box->childGap;
    d.layout.childAlignment.x = alignXOf(box->alignX);
    d.layout.childAlignment.y = alignYOf(box->alignY);
    d.layout.layoutDirection = box->horizontal ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;

    if (box->bgA > 0.0f) {
        d.backgroundColor = (Clay_Color){ box->bgR, box->bgG, box->bgB, box->bgA };
    }
    if (box->cornerRadius > 0.0f) {
        d.cornerRadius = (Clay_CornerRadius){
            box->cornerRadius, box->cornerRadius,
            box->cornerRadius, box->cornerRadius };
    }
    if (box->borderWidth > 0) {
        d.border.color = (Clay_Color){ box->borderR, box->borderG, box->borderB, box->borderA };
        d.border.width.left = (uint16_t)box->borderWidth;
        d.border.width.right = (uint16_t)box->borderWidth;
        d.border.width.top = (uint16_t)box->borderWidth;
        d.border.width.bottom = (uint16_t)box->borderWidth;
    }

    if (box->id) {
        Clay__OpenElementWithId(Clay__HashString(claylay_str(box->id), 0));
    } else {
        Clay__OpenElement();
    }
    Clay__ConfigureOpenElement(d);
}

void claylay_box_close(void) {
    Clay__CloseElement();
}

void claylay_text(const char* text, int fontSize, float r, float g, float b, float a) {
    Clay_TextElementConfig config;
    if (!text) return;
    memset(&config, 0, sizeof(config));
    config.textColor = (Clay_Color){ r, g, b, a };
    config.fontSize = (uint16_t)(fontSize > 0 ? fontSize : 20);
    Clay__OpenTextElement(claylay_str(text), config);
}

int claylay_hovered(const char* id) {
    if (!id) return 0;
    return Clay_PointerOver(Clay__HashString(claylay_str(id), 0)) ? 1 : 0;
}

void claylay_end(void) {
    Clay_RenderCommandArray commands = Clay_EndLayout(1.0f / 60.0f);
    int32_t i;
    s_commandCount = 0;
    for (i = 0; i < commands.length && s_commandCount < CLAYLAY_MAX_COMMANDS; ++i) {
        const Clay_RenderCommand* cmd = &commands.internalArray[i];
        const Clay_BoundingBox* bb = &cmd->boundingBox;
        ClayLayCmd* out = &s_commands[s_commandCount];
        memset(out, 0, sizeof(*out));
        out->x = bb->x;
        out->y = bb->y;
        out->w = bb->width;
        out->h = bb->height;
        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                out->type = CLAYLAY_CMD_RECT;
                out->r = cmd->renderData.rectangle.backgroundColor.r;
                out->g = cmd->renderData.rectangle.backgroundColor.g;
                out->b = cmd->renderData.rectangle.backgroundColor.b;
                out->a = cmd->renderData.rectangle.backgroundColor.a;
                ++s_commandCount;
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                const Clay_BorderRenderData* border = &cmd->renderData.border;
                float lw = (float)border->width.left;
                out->type = CLAYLAY_CMD_BORDER;
                out->r = border->color.r;
                out->g = border->color.g;
                out->b = border->color.b;
                out->a = border->color.a;
                if (border->width.right > lw) lw = (float)border->width.right;
                if (border->width.top > lw) lw = (float)border->width.top;
                if (border->width.bottom > lw) lw = (float)border->width.bottom;
                out->lineWidth = lw;
                ++s_commandCount;
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                const Clay_TextRenderData* text = &cmd->renderData.text;
                size_t len = text->stringContents.length < 0
                                 ? 0 : (size_t)text->stringContents.length;
                out->type = CLAYLAY_CMD_TEXT;
                out->r = text->textColor.r;
                out->g = text->textColor.g;
                out->b = text->textColor.b;
                out->a = text->textColor.a;
                out->fontSize = text->fontSize > 0 ? text->fontSize : 20;
                if (len > sizeof(out->text) - 1) len = sizeof(out->text) - 1;
                memcpy(out->text, text->stringContents.chars, len);
                out->text[len] = '\0';
                ++s_commandCount;
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                out->type = CLAYLAY_CMD_CLIP_START;
                ++s_commandCount;
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                out->type = CLAYLAY_CMD_CLIP_END;
                ++s_commandCount;
                break;
            default:
                break;
        }
    }
}

const ClayLayCmd* claylay_commands(int* count) {
    if (count) *count = s_commandCount;
    return s_commands;
}
