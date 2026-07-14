#include "domi/render_queue.h"
#include "domi/backend/render_backend.h"

namespace domi {

RenderQueue::RenderQueue()
    : currentFillColor_(1.0f, 1.0f, 1.0f, 1.0f),
      currentStrokeColor_(0.0f, 0.0f, 0.0f, 1.0f),
      currentLineWidth_(1.0f) {}

RenderQueue::~RenderQueue() {}

void RenderQueue::clear() {
    commands_.clear();
    paths_.clear();
}

void RenderQueue::setFillColor(const Color& c) {
    currentFillColor_ = c;
    RenderCommand cmd;
    cmd.type = RenderCommand::SetFillColor;
    cmd.params.color = { c.r, c.g, c.b, c.a };
    commands_.push_back(cmd);
}

void RenderQueue::setStrokeColor(const Color& c) {
    currentStrokeColor_ = c;
    RenderCommand cmd;
    cmd.type = RenderCommand::SetStrokeColor;
    cmd.params.color = { c.r, c.g, c.b, c.a };
    commands_.push_back(cmd);
}

void RenderQueue::setLineWidth(float w) {
    currentLineWidth_ = w;
    RenderCommand cmd;
    cmd.type = RenderCommand::SetLineWidth;
    cmd.params.lineWidth.lineWidth = w;
    commands_.push_back(cmd);
}

void RenderQueue::fillRect(float x, float y, float w, float h) {
    RenderCommand cmd;
    cmd.type = RenderCommand::FillRect;
    cmd.params.rect = { x, y, w, h };
    commands_.push_back(cmd);
}

void RenderQueue::strokeRect(float x, float y, float w, float h) {
    RenderCommand cmd;
    cmd.type = RenderCommand::StrokeRect;
    cmd.params.rect = { x, y, w, h };
    commands_.push_back(cmd);
}

void RenderQueue::clearRect(float x, float y, float w, float h) {
    RenderCommand cmd;
    cmd.type = RenderCommand::ClearRect;
    cmd.params.rect = { x, y, w, h };
    commands_.push_back(cmd);
}

void RenderQueue::drawLine(float x1, float y1, float x2, float y2) {
    RenderCommand cmd;
    cmd.type = RenderCommand::DrawLine;
    cmd.params.line = { x1, y1, x2, y2 };
    commands_.push_back(cmd);
}

void RenderQueue::fillCircle(float x, float y, float radius, int segments) {
    RenderCommand cmd;
    cmd.type = RenderCommand::FillCircle;
    cmd.params.circle = { x, y, radius, segments };
    commands_.push_back(cmd);
}

void RenderQueue::fillPath(const std::vector<Vec2>& points, bool closed) {
    pushPathCommand(RenderCommand::FillPath, points, closed);
}

void RenderQueue::strokePath(const std::vector<Vec2>& points, bool closed) {
    pushPathCommand(RenderCommand::StrokePath, points, closed);
}

void RenderQueue::drawMaterial(float x, float y, void* handle) {
    RenderCommand cmd;
    cmd.type = RenderCommand::DrawMaterial;
    cmd.params.material = { x, y, handle };
    commands_.push_back(cmd);
}

void RenderQueue::pushPathCommand(RenderCommand::Type type,
                                  const std::vector<Vec2>& points, bool closed) {
    int index = static_cast<int>(paths_.size());
    StoredPath sp;
    sp.points = points;
    sp.closed = closed;
    paths_.push_back(sp);

    RenderCommand cmd;
    cmd.type = type;
    cmd.params.path.pathIndex = index;
    commands_.push_back(cmd);
}

void RenderQueue::renderPath(IRenderBackend* backend, const StoredPath& path,
                             const Color& color, bool fill, float lineWidth) {
    if (!backend) return;
    if (fill) {
        backend->fillPath(path.points, path.closed, color);
    } else {
        backend->strokePath(path.points, path.closed, color, lineWidth);
    }
}

void RenderQueue::flush(IRenderBackend* backend) {
    if (!backend || commands_.empty()) {
        clear();
        return;
    }

    for (size_t i = 0; i < commands_.size(); ++i) {
        const RenderCommand& cmd = commands_[i];
        switch (cmd.type) {
            case RenderCommand::SetFillColor:
                currentFillColor_ = Color(cmd.params.color.r, cmd.params.color.g,
                                          cmd.params.color.b, cmd.params.color.a);
                break;
            case RenderCommand::SetStrokeColor:
                currentStrokeColor_ = Color(cmd.params.color.r, cmd.params.color.g,
                                            cmd.params.color.b, cmd.params.color.a);
                break;
            case RenderCommand::SetLineWidth:
                currentLineWidth_ = cmd.params.lineWidth.lineWidth;
                break;
            case RenderCommand::FillRect:
                backend->setFillColor(currentFillColor_);
                backend->fillRect(cmd.params.rect.x, cmd.params.rect.y,
                                  cmd.params.rect.w, cmd.params.rect.h);
                break;
            case RenderCommand::StrokeRect:
                backend->setStrokeColor(currentStrokeColor_);
                backend->strokeRect(cmd.params.rect.x, cmd.params.rect.y,
                                    cmd.params.rect.w, cmd.params.rect.h);
                break;
            case RenderCommand::ClearRect:
                backend->setFillColor(currentFillColor_);
                backend->fillRect(cmd.params.rect.x, cmd.params.rect.y,
                                  cmd.params.rect.w, cmd.params.rect.h);
                break;
            case RenderCommand::DrawLine:
                backend->setStrokeColor(currentStrokeColor_);
                backend->drawLine(cmd.params.line.x1, cmd.params.line.y1,
                                  cmd.params.line.x2, cmd.params.line.y2);
                break;
            case RenderCommand::FillCircle:
                backend->setFillColor(currentFillColor_);
                backend->fillCircle(cmd.params.circle.x, cmd.params.circle.y,
                                    cmd.params.circle.radius,
                                    cmd.params.circle.segments);
                break;
            case RenderCommand::FillPath:
                renderPath(backend, paths_[cmd.params.path.pathIndex],
                           currentFillColor_, true, currentLineWidth_);
                break;
            case RenderCommand::StrokePath:
                renderPath(backend, paths_[cmd.params.path.pathIndex],
                           currentStrokeColor_, false, currentLineWidth_);
                break;
            case RenderCommand::DrawMaterial:
                if (cmd.params.material.handle) {
                    backend->drawMaterial(cmd.params.material.x,
                                          cmd.params.material.y,
                                          cmd.params.material.handle);
                }
                break;
        }
    }

    clear();
}

} // namespace domi
