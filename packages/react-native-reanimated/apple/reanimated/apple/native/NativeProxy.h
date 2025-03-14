#if __cplusplus

#import <reanimated/NativeModules/ReanimatedModuleProxy.h>
#import <reanimated/apple/REAModule.h>

#import <worklets/apple/WorkletsModule.h>

namespace reanimated {

std::shared_ptr<reanimated::ReanimatedModuleProxy> createReanimatedModule(
    REAModule *reaModule,
    RCTModuleRegistry *moduleRegistry,
    const std::shared_ptr<facebook::react::CallInvoker> &jsInvoker,
    WorkletsModule *workletsModule);

} // namespace reanimated

#endif //__cplusplus
