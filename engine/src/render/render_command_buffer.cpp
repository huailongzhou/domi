#include "domi/render_command_buffer.h"
#include "domi/canvas2d.h"
#include "domi/render_texture.h"

namespace domi {

CommandBuffer::CommandBuffer(Canvas2D* canvas)
    : canvas_(canvas), currentTarget_(NULL) {}

CommandBuffer::~CommandBuffer() {}

void CommandBuffer::setTarget(RenderTexture* target) {
    if (target == currentTarget_) return;

    // Flush queued commands for the previous target before switching.
    if (canvas_) {
        canvas_->flush();
        canvas_->setRenderTarget(target);
    }
    currentTarget_ = target;
}

void CommandBuffer::clear(const Color& c) {
    if (canvas_) canvas_->clear(c);
}

void CommandBuffer::setFillColor(const Color& c) {
    if (canvas_) canvas_->setFillColor(c);
}

void CommandBuffer::setStrokeColor(const Color& c) {
    if (canvas_) canvas_->setStrokeColor(c);
}

void CommandBuffer::setLineWidth(float w) {
    if (canvas_) canvas_->setLineWidth(w);
}

void CommandBuffer::fillRect(float x, float y, float w, float h) {
    if (canvas_) canvas_->fillRect(x, y, w, h);
}

void CommandBuffer::strokeRect(float x, float y, float w, float h) {
    if (canvas_) canvas_->strokeRect(x, y, w, h);
}

void CommandBuffer::clearRect(float x, float y, float w, float h) {
    if (canvas_) canvas_->clearRect(x, y, w, h);
}

void CommandBuffer::drawLine(float x1, float y1, float x2, float y2) {
    if (canvas_) canvas_->drawLine(x1, y1, x2, y2);
}

void CommandBuffer::fillCircle(float x, float y, float radius, int segments) {
    if (canvas_) canvas_->fillCircle(x, y, radius, segments);
}

void CommandBuffer::beginPath() {
    if (canvas_) canvas_->beginPath();
}

void CommandBuffer::moveTo(float x, float y) {
    if (canvas_) canvas_->moveTo(x, y);
}

void CommandBuffer::lineTo(float x, float y) {
    if (canvas_) canvas_->lineTo(x, y);
}

void CommandBuffer::closePath() {
    if (canvas_) canvas_->closePath();
}

void CommandBuffer::fill() {
    if (canvas_) canvas_->fill();
}

void CommandBuffer::stroke() {
    if (canvas_) canvas_->stroke();
}

void CommandBuffer::drawMaterial(float x, float y, const Material& material) {
    if (canvas_) canvas_->drawMaterial(x, y, material);
}

void CommandBuffer::drawTexture(float x, float y, RenderTexture* texture) {
    if (canvas_) canvas_->drawTexture(x, y, texture);
}

void CommandBuffer::drawTexture(float x, float y, RenderTexture* texture, BlendMode mode) {
    if (canvas_) canvas_->drawTexture(x, y, texture, mode);
}

void CommandBuffer::flush() {
    if (canvas_) canvas_->flush();
}

void CommandBuffer::setClipRect(float x, float y, float w, float h) {
    if (canvas_) canvas_->setClipRect(x, y, w, h);
}

void CommandBuffer::resetClipRect() {
    if (canvas_) canvas_->resetClipRect();
}

} // namespace domi
