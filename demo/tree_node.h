#ifndef DEMO_TREE_NODE_H
#define DEMO_TREE_NODE_H

#include "domi/render_node.h"
#include "domi/draw_batch.h"
#include "domi/material.h"

// A 2D tree sprite with separate trunk and foliage materials.
// The trunk is sorted by the tree bottom; foliage draws slightly after it.
class TreeNode : public domi::RenderNode {
public:
    const domi::Material* trunk = nullptr;
    const domi::Material* foliage = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;

    void build(domi::RenderList& list, domi::RenderLayer layer) const override {
        const float treeHeight = 80.0f;
        float bottomZ = y + treeHeight * 0.5f;
        if (trunk && trunk->width > 0) {
            domi::DrawBatch batch;
            batch.save();
            batch.translate(x, y + 40.0f);
            batch.scale(scale, scale);
            batch.drawMaterial(-trunk->width * 0.5f, -trunk->height, *trunk);
            batch.restore();
            list.submit(layer, bottomZ, batch);
        }
        if (foliage && foliage->width > 0) {
            domi::DrawBatch batch;
            batch.save();
            batch.translate(x, y + 40.0f);
            batch.scale(scale, scale);
            batch.drawMaterial(-foliage->width * 0.5f, -foliage->height, *foliage);
            batch.restore();
            list.submit(layer, bottomZ + 0.1f, batch);
        }
    }
};

#endif // DEMO_TREE_NODE_H
