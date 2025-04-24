#include <reanimated/Fabric/ShadowTreeCloner.h>
#include <reanimated/Tools/ReanimatedSystraceSection.h>

#ifdef ANDROID
#include <android/log.h>
#endif

#include <ranges>
#include <utility>

namespace reanimated {

/**
 * Checks it the props of a are identical to the props in b.
 * Doesn't mean they are deeply the same, just that b has everything that a has.
 */
bool checkPropsEqual(const folly::dynamic& a, const folly::dynamic& b) {
    ReanimatedSystraceSection s("ShadowTreeCloner::checkPropsEqual");
    for (const auto& pair : a.items()) {
        const auto& key = pair.first;
        auto it = b.find(key);
        if (it == b.items().end()) {
            return false; // Key from A not found in B
        }

        const auto& valueA = pair.second;
        const auto& valueB = it->second;

        return valueA == valueB;
    }

    return false;
}

std::pair<Props::Shared, bool> mergeProps(
    const ShadowNode &shadowNode,
    const PropsMap &propsMap,
    const ShadowNodeFamily &family) {
  ReanimatedSystraceSection s("ShadowTreeCloner::mergeProps");

  const auto it = propsMap.find(&family);

  if (it == propsMap.end()) {
    return {ShadowNodeFragment::propsPlaceholder(), false};
  }

  PropsParserContext propsParserContext{
      shadowNode.getSurfaceId(), *shadowNode.getContextContainer()};

  const auto &propsVector = it->second;
  auto newProps = shadowNode.getProps();

#ifdef ANDROID
  if (propsVector.size() > 1) {
    folly::dynamic newPropsDynamic = folly::dynamic::object;
    for (const auto &props : propsVector) {
      newPropsDynamic = folly::dynamic::merge(
          props.operator folly::dynamic(), newPropsDynamic);
    }
    // TODO: not handled yet as this case wasn't occuring yet for me
      __android_log_print(ANDROID_LOG_DEBUG, "Hanno", "nativeId: %s NOT HANDLED", shadowNode.getProps()->nativeId.c_str());
    return {shadowNode.getComponentDescriptor().cloneProps(
            propsParserContext, newProps, RawProps(newPropsDynamic)), false};
  }
#endif

  bool isSame = false;
  for (const auto &props : propsVector) {
    // TODO: right now there is only one RawProps in the vector so we just compare that
    RawProps propForOverwriting(props);
      {
        ReanimatedSystraceSection s("ShadowTreeCloner::mergeProps::checkEquality");
        folly::dynamic dynamicPropForOverwriting(propForOverwriting);
        auto shadowNodeProps = shadowNode.getProps()->rawProps;
        isSame = dynamicPropForOverwriting == shadowNodeProps || checkPropsEqual(dynamicPropForOverwriting, shadowNodeProps);
    }

    if (!isSame) {
        newProps = shadowNode.getComponentDescriptor().cloneProps(
                propsParserContext, newProps, std::move(propForOverwriting));
    }
  }

  return {newProps, isSame};
}

ShadowNode::Unshared cloneShadowTreeWithNewPropsRecursive(
    const ShadowNode &shadowNode,
    const ChildrenMap &childrenMap,
    const PropsMap &propsMap,
    std::vector<const ShadowNodeFamily*>& familiesToRemove) {
  const auto family = &shadowNode.getFamily();
  const auto affectedChildrenIt = childrenMap.find(family);
  // RootNode getChildren
  auto children = shadowNode.getChildren();

  if (affectedChildrenIt != childrenMap.end()) {
      // If we have affected children, loop through their index numbers
    for (const auto index : affectedChildrenIt->second) {
      // 1. Get the child
      const ShadowNode& child = *children[index];
      // 2. clone it recursively
      children[index] = cloneShadowTreeWithNewPropsRecursive(
          child, childrenMap, propsMap, familiesToRemove);
    }
  }


//  __android_log_print(ANDROID_LOG_DEBUG, "HannoDebug", "Cloning shadowNode '%s' props %s", family->getComponentName(), shadowNode.getProps()->getDebugDescription().c_str());
  // Check if props from ShadowNode are the same
  const auto& [mergedProps, isSame] = mergeProps(shadowNode, propsMap, *family);
  if (isSame) {
      familiesToRemove.push_back(family);
  }

  return shadowNode.clone(
      {mergedProps,
       std::make_shared<ShadowNode::ListOfShared>(children),
       shadowNode.getState()});
}

CloneResult cloneShadowTreeWithNewProps(
    const RootShadowNode &oldRootNode,
    const PropsMap &propsMap) {
  ReanimatedSystraceSection s("ShadowTreeCloner::cloneShadowTreeWithNewProps");

  ChildrenMap childrenMap;

  {
    ReanimatedSystraceSection s("ShadowTreeCloner::prepareChildrenMap");

    for (auto &[family, _] : propsMap) {
        // root node -> ancestor -> parent of family -> (this family)
      const auto ancestors = family->getAncestors(oldRootNode);
//        std::vector<
//              std::pair<
//                std::reference_wrapper<const ShadowNode> /* parentNode */,
//                int /* childIndex */
//              >
//        >;

      // parent of family -> ancestor -> root node
      for (const auto &[parentNode, index] :
           std::ranges::reverse_view(ancestors)) {
        const auto parentFamily = &parentNode.get().getFamily();
        auto &affectedChildren = childrenMap[parentFamily];

        if (affectedChildren.contains(index)) {
          continue;
        }

        // the family's children expressed as ints (???)
        affectedChildren.insert(index);
      }
    }
  }

  std::vector<const ShadowNodeFamily*> familiesToRemove{};
  // This cast is safe, because this function returns a clone
  // of the oldRootNode, which is an instance of RootShadowNode
  auto newRoot = std::static_pointer_cast<RootShadowNode>(
      cloneShadowTreeWithNewPropsRecursive(oldRootNode, childrenMap, propsMap, familiesToRemove));

  return {
      .newRoot = std::move(newRoot),
      .familiesToRemove = std::move(familiesToRemove)
  };
}

} // namespace reanimated
