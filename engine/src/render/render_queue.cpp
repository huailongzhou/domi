#include "domi/render/render_queue.h"
#include "domi/backend/render_backend.h"

namespace domi {

RenderQueue::RenderQueue()
    : currentFillColor_(1.0f, 1.0f, 1.0f, 1.0f),
      currentStrokeColor_(0.0f, 0.0f, 0.0f, 1.0f),
      currentLineWidth_(1.0f) {}

RenderQueue::~RenderQueue() {}

void RenderQueue::fillPath(const std::vector<Vec2>& points, bool closed) {
    const Color color = currentFillColor_;
    push([points, closed, color](IRenderBackend* backend) {
        backend->fillPath(points, closed, color);
    });
}

void RenderQueue::strokePath(const std::vector<Vec2>& points, bool closed) {
    const Color color = currentStrokeColor_;
    const float lineWidth = currentLineWidth_;
    push([points, closed, color, lineWidth](IRenderBackend* backend) {
        backend->strokePath(points, closed, color, lineWidth);
    });
}

void RenderQueue::drawLine(float x1, float y1, float x2, float y2) {
    const Color color = currentStrokeColor_;
    push([x1, y1, x2, y2, color](IRenderBackend* backend) {
        backend->setStrokeColor(color);
        backend->drawLine(x1, y1, x2, y2);
    });
}

void RenderQueue::drawMaterial(float x, float y, void* handle,
                               float angle, float centerX, float centerY,
                               float scaleX, float scaleY) {
    push([x, y, handle, angle, centerX, centerY, scaleX, scaleY](IRenderBackend* backend) {
        if (handle) {
            backend->drawMaterial(x, y, handle, angle, centerX, centerY,
                                  scaleX, scaleY);
        }
    });
}

void RenderQueue::drawMaterialRegion(float x, float y, void* handle,
                                     int srcX, int srcY, int srcW, int srcH,
                                     const Color& tint,
                                     float angle, float centerX, float centerY,
                                     float scaleX, float scaleY) {
    push([=](IRenderBackend* backend) {
        if (handle) {
            backend->drawMaterialRegion(x, y, handle, srcX, srcY, srcW, srcH,
                                        tint, angle, centerX, centerY,
                                        scaleX, scaleY);
        }
    });
}

} // namespace domi
