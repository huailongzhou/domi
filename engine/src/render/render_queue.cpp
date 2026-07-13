#include "domi/render_queue.h"
#include <SDL3/SDL.h>
#include <algorithm>

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

void RenderQueue::applyColor(SDL_Renderer* renderer, const Color& c) {
    SDL_SetRenderDrawColor(renderer,
        (uint8_t)(c.r * 255),
        (uint8_t)(c.g * 255),
        (uint8_t)(c.b * 255),
        (uint8_t)(c.a * 255));
}

void RenderQueue::renderPath(SDL_Renderer* renderer, const StoredPath& path,
                             const Color& color, bool fill, float lineWidth) {
    size_t n = path.points.size();
    if (n < 2) return;

    if (fill) {
        if (n < 3) return;
        std::vector<SDL_Vertex> vertices;
        vertices.reserve(n);
        std::vector<int> indices;
        indices.reserve((n - 2) * 3);

        SDL_FColor col = { color.r, color.g, color.b, color.a };
        SDL_FPoint zero = { 0.0f, 0.0f };
        for (size_t i = 0; i < n; ++i) {
            SDL_Vertex v;
            v.position.x = path.points[i].x;
            v.position.y = path.points[i].y;
            v.color = col;
            v.tex_coord = zero;
            vertices.push_back(v);
        }
        for (size_t i = 1; i + 1 < n; ++i) {
            indices.push_back(0);
            indices.push_back((int)i);
            indices.push_back((int)(i + 1));
        }
        SDL_RenderGeometry(renderer, NULL,
                           vertices.data(), (int)vertices.size(),
                           indices.data(), (int)indices.size());
    } else {
        SDL_SetRenderDrawColor(renderer,
            (uint8_t)(color.r * 255),
            (uint8_t)(color.g * 255),
            (uint8_t)(color.b * 255),
            (uint8_t)(color.a * 255));
        for (size_t i = 0; i + 1 < n; ++i) {
            SDL_RenderLine(renderer, path.points[i].x, path.points[i].y,
                           path.points[i + 1].x, path.points[i + 1].y);
        }
        if (path.closed && n > 2) {
            SDL_RenderLine(renderer, path.points.back().x, path.points.back().y,
                           path.points[0].x, path.points[0].y);
        }
    }
}

void RenderQueue::flush(SDL_Renderer* renderer) {
    if (!renderer || commands_.empty()) {
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
            case RenderCommand::FillRect: {
                applyColor(renderer, currentFillColor_);
                SDL_FRect r = { cmd.params.rect.x, cmd.params.rect.y,
                                cmd.params.rect.w, cmd.params.rect.h };
                SDL_RenderFillRect(renderer, &r);
                break;
            }
            case RenderCommand::StrokeRect: {
                applyColor(renderer, currentStrokeColor_);
                SDL_FRect r = { cmd.params.rect.x, cmd.params.rect.y,
                                cmd.params.rect.w, cmd.params.rect.h };
                SDL_RenderRect(renderer, &r);
                break;
            }
            case RenderCommand::ClearRect: {
                applyColor(renderer, currentFillColor_);
                SDL_FRect r = { cmd.params.rect.x, cmd.params.rect.y,
                                cmd.params.rect.w, cmd.params.rect.h };
                SDL_RenderFillRect(renderer, &r);
                break;
            }
            case RenderCommand::DrawLine: {
                applyColor(renderer, currentStrokeColor_);
                SDL_RenderLine(renderer, cmd.params.line.x1, cmd.params.line.y1,
                               cmd.params.line.x2, cmd.params.line.y2);
                break;
            }
            case RenderCommand::FillCircle: {
                const auto& c = cmd.params.circle;
                int segments = c.segments;
                if (segments < 3) segments = 3;
                std::vector<SDL_Vertex> vertices;
                vertices.reserve(segments + 2);
                std::vector<int> indices;
                indices.reserve(segments * 3);
                SDL_FColor col = { currentFillColor_.r, currentFillColor_.g,
                                   currentFillColor_.b, currentFillColor_.a };
                SDL_FPoint zero = { 0.0f, 0.0f };
                SDL_Vertex center;
                center.position.x = c.x;
                center.position.y = c.y;
                center.color = col;
                center.tex_coord = zero;
                vertices.push_back(center);
                for (int j = 0; j <= segments; ++j) {
                    float angle = (float)j / (float)segments * 2.0f * 3.14159265f;
                    SDL_Vertex v;
                    v.position.x = c.x + c.radius * SDL_cosf(angle);
                    v.position.y = c.y + c.radius * SDL_sinf(angle);
                    v.color = col;
                    v.tex_coord = zero;
                    vertices.push_back(v);
                }
                for (int j = 0; j < segments; ++j) {
                    indices.push_back(0);
                    indices.push_back(j + 1);
                    indices.push_back(j + 2);
                }
                SDL_RenderGeometry(renderer, NULL,
                                   vertices.data(), (int)vertices.size(),
                                   indices.data(), (int)indices.size());
                break;
            }
            case RenderCommand::FillPath: {
                const StoredPath& path = paths_[cmd.params.path.pathIndex];
                renderPath(renderer, path, currentFillColor_, true, currentLineWidth_);
                break;
            }
            case RenderCommand::StrokePath: {
                const StoredPath& path = paths_[cmd.params.path.pathIndex];
                renderPath(renderer, path, currentStrokeColor_, false, currentLineWidth_);
                break;
            }
        }
    }

    clear();
}

} // namespace domi
