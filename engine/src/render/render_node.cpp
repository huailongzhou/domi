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

void PathNode::build(RenderList& list, RenderLayer layer) const {
    if (hasFill_) {
        list.setFillColor(layer, z, fillColor_);
    }
    if (hasStroke_) {
        list.setStrokeColor(layer, z, strokeColor_);
        list.setLineWidth(layer, z, lineWidth_);
    }

    list.beginPath(layer, z);
    for (size_t i = 0; i < segments_.size(); ++i) {
        const Segment& s = segments_[i];
        switch (s.type) {
            case Segment::MoveTo:
                list.moveTo(layer, z, s.x1, s.y1);
                break;
            case Segment::LineTo:
                list.lineTo(layer, z, s.x1, s.y1);
                break;
            case Segment::QuadTo:
                list.quadraticCurveTo(layer, z, s.x1, s.y1, s.x2, s.y2);
                break;
            case Segment::BezierTo:
                list.bezierCurveTo(layer, z, s.x1, s.y1, s.x2, s.y2, s.x3, s.y3);
                break;
            case Segment::Close:
                list.closePath(layer, z);
                break;
        }
    }
    if (hasFill_) {
        list.fill(layer, z);
    }
    if (hasStroke_) {
        list.stroke(layer, z);
    }
}

} // namespace domi
