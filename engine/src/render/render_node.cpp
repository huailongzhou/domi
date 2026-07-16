#include "domi/render_node.h"

namespace domi {

PathNode& PathNode::moveTo(float x, float y) {
    Segment s;
    s.type = Segment::MoveTo;
    s.x1 = x; s.y1 = y;
    segments_.push_back(s);
    return *this;
}

PathNode& PathNode::lineTo(float x, float y) {
    Segment s;
    s.type = Segment::LineTo;
    s.x1 = x; s.y1 = y;
    segments_.push_back(s);
    return *this;
}

PathNode& PathNode::quadraticCurveTo(float cpx, float cpy, float x, float y) {
    Segment s;
    s.type = Segment::QuadTo;
    s.x1 = cpx; s.y1 = cpy;
    s.x2 = x;   s.y2 = y;
    segments_.push_back(s);
    return *this;
}

PathNode& PathNode::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
    Segment s;
    s.type = Segment::BezierTo;
    s.x1 = cp1x; s.y1 = cp1y;
    s.x2 = cp2x; s.y2 = cp2y;
    s.x3 = x;    s.y3 = y;
    segments_.push_back(s);
    return *this;
}

PathNode& PathNode::closePath() {
    Segment s;
    s.type = Segment::Close;
    segments_.push_back(s);
    return *this;
}

void PathNode::getYRange(float& y1, float& y2) const {
    float minY = 0.0f;
    float maxY = 0.0f;
    bool first = true;
    for (size_t i = 0; i < segments_.size(); ++i) {
        const Segment& s = segments_[i];
        float ys[3];
        int n = 0;
        switch (s.type) {
            case Segment::MoveTo:
            case Segment::LineTo:
                ys[n++] = s.y1;
                break;
            case Segment::QuadTo:
                ys[n++] = s.y1;
                ys[n++] = s.y2;
                break;
            case Segment::BezierTo:
                ys[n++] = s.y1;
                ys[n++] = s.y2;
                ys[n++] = s.y3;
                break;
            case Segment::Close:
                break;
        }
        for (int k = 0; k < n; ++k) {
            if (first || ys[k] < minY) minY = ys[k];
            if (first || ys[k] > maxY) maxY = ys[k];
            first = false;
        }
    }
    y1 = minY;
    y2 = maxY;
}

void PathNode::build(RenderList& list, RenderLayer layer) const {
    const float key = sortKey();

    if (hasFill_) {
        list.setFillColor(layer, key, fillColor_);
    }
    if (hasStroke_) {
        list.setStrokeColor(layer, key, strokeColor_);
        list.setLineWidth(layer, key, lineWidth_);
    }

    list.beginPath(layer, key);
    for (size_t i = 0; i < segments_.size(); ++i) {
        const Segment& s = segments_[i];
        switch (s.type) {
            case Segment::MoveTo:
                list.moveTo(layer, key, s.x1, s.y1);
                break;
            case Segment::LineTo:
                list.lineTo(layer, key, s.x1, s.y1);
                break;
            case Segment::QuadTo:
                list.quadraticCurveTo(layer, key, s.x1, s.y1, s.x2, s.y2);
                break;
            case Segment::BezierTo:
                list.bezierCurveTo(layer, key, s.x1, s.y1, s.x2, s.y2, s.x3, s.y3);
                break;
            case Segment::Close:
                list.closePath(layer, key);
                break;
        }
    }
    if (hasFill_) {
        list.fill(layer, key);
    }
    if (hasStroke_) {
        list.stroke(layer, key);
    }
}

} // namespace domi
