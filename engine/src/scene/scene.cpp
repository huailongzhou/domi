#include "domi/scene/scene.h"
#include "domi/render/render_node.h"
#include "domi/render/render_list.h"

namespace domi {

Scene::~Scene() {}

void Scene::render(RenderList& list) {
    if (rootNode_) {
        rootNode_->build(list, RenderLayer::Background);
    }
}

void Scene::setRootNode(std::unique_ptr<RenderNode> root) {
    rootNode_ = std::move(root);
}

} // namespace domi
