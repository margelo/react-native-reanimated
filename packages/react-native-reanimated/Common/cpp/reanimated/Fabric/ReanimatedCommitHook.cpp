#include <reanimated/Fabric/ReanimatedCommitHook.h>
#include <reanimated/Fabric/ReanimatedCommitShadowNode.h>
#include <reanimated/Fabric/ShadowTreeCloner.h>
#include <reanimated/Tools/ReanimatedSystraceSection.h>

#include <react/renderer/core/ComponentDescriptor.h>

#include <unordered_map>
#include <vector>

#include <android/log.h>
#include <chrono>

using namespace facebook::react;

namespace reanimated {

ReanimatedCommitHook::ReanimatedCommitHook(
    const std::shared_ptr<UIManager> &uiManager,
    const std::shared_ptr<UpdatesRegistryManager> &updatesRegistryManager,
    const std::shared_ptr<LayoutAnimationsProxy> &layoutAnimationsProxy)
    : uiManager_(uiManager),
      updatesRegistryManager_(updatesRegistryManager),
      layoutAnimationsProxy_(layoutAnimationsProxy) {
  uiManager_->registerCommitHook(*this);
}

ReanimatedCommitHook::~ReanimatedCommitHook() noexcept {
  uiManager_->unregisterCommitHook(*this);
}

void ReanimatedCommitHook::maybeInitializeLayoutAnimations(
    SurfaceId surfaceId) {
  auto lock = std::unique_lock<std::mutex>(mutex_);
  if (surfaceId > currentMaxSurfaceId_) {
    // when a new surfaceId is observed we call setMountingOverrideDelegate
    // for all yet unseen surfaces
    uiManager_->getShadowTreeRegistry().enumerate(
        [strongThis = shared_from_this()](
            const ShadowTree &shadowTree, bool &stop) {
          // Executed synchronously.
          if (shadowTree.getSurfaceId() <= strongThis->currentMaxSurfaceId_) {
            // the set function actually adds our delegate to a list, so we
            // shouldn't invoke it twice for the same surface
            return;
          }
          shadowTree.getMountingCoordinator()->setMountingOverrideDelegate(
              strongThis->layoutAnimationsProxy_);
        });
    currentMaxSurfaceId_ = surfaceId;
  }
}

RootShadowNode::Unshared ReanimatedCommitHook::shadowTreeWillCommit(
    ShadowTree const &,
    RootShadowNode::Shared const &,
    RootShadowNode::Unshared const &newRootShadowNode) noexcept {
  ReanimatedSystraceSection s("ReanimatedCommitHook::shadowTreeWillCommit");
  auto start = std::chrono::high_resolution_clock::now();

  maybeInitializeLayoutAnimations(newRootShadowNode->getSurfaceId());

  auto reaShadowNode =
      std::reinterpret_pointer_cast<ReanimatedCommitShadowNode>(
          newRootShadowNode);

  if (reaShadowNode->hasReanimatedCommitTrait()) {
//      __android_log_print(
//              ANDROID_LOG_DEBUG,
//              "HannoDebug",
//              "Skip commit from REA"
//      );
    // ShadowTree commited by Reanimated, no need to apply updates from
    // the updates registry manager
    reaShadowNode->unsetReanimatedCommitTrait();
    reaShadowNode->setReanimatedMountTrait();
    return newRootShadowNode;
  }

  // ShadowTree not commited by Reanimated, apply updates from the updates
  // registry manager
  reaShadowNode->unsetReanimatedMountTrait();
  RootShadowNode::Unshared rootNode = newRootShadowNode;

  {
    auto lock = updatesRegistryManager_->lock();

    PropsMap propsMap = updatesRegistryManager_->collectProps();
    updatesRegistryManager_->cancelCommitAfterPause();

    auto propMapCount = propsMap.size();

    CloneResult result = cloneShadowTreeWithNewProps(*rootNode, propsMap);
    rootNode = std::move(result.newRoot);

    // Check for families that we want to remove from the registry
//      __android_log_print(ANDROID_LOG_DEBUG, "Hanno", "commitHook Families to remove %zu", result.familiesToRemove.size());
    for (const auto& family : result.familiesToRemove) {
//        __android_log_print(ANDROID_LOG_DEBUG, "Hanno", "- remove %d", family->getTag());
        updatesRegistryManager_->forceRemoveNode(family->getTag());
    }

    // If the commit comes from React Native then pause commits from
    // Reanimated since the ShadowTree to be committed by Reanimated may not
    // include the new changes from React Native yet and all changes of animated
    // props will be applied in ReanimatedCommitHook by UpdatesRegistryManager
    // This is very important, since if we didn't pause Reanimated commits,
    // it could lead to RN commits being delayed until the animation is finished
    // (very bad).
    updatesRegistryManager_->pauseReanimatedCommits();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    __android_log_print(
        ANDROID_LOG_DEBUG,
        "HannoDebug",
        "Props map count %zu took %f ms",
        propMapCount,
        duration / 1000000.0f);
  }

  return rootNode;
}

} // namespace reanimated
