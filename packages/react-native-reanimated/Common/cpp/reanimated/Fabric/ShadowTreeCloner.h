#pragma once

#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/uimanager/UIManager.h>

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace facebook;
using namespace react;

namespace reanimated {

struct CloneResult {
    RootShadowNode::Unshared newRoot;
    std::vector<const ShadowNodeFamily*> familiesToRemove;
};

using PropsMap =
    std::unordered_map<const ShadowNodeFamily *, std::vector<RawProps>>;
using ChildrenMap =
    std::unordered_map<const ShadowNodeFamily *, std::unordered_set<int>>;

    CloneResult cloneShadowTreeWithNewProps(
    const RootShadowNode &oldRootNode,
    const PropsMap &propsMap);

} // namespace reanimated
