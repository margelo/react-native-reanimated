/* eslint-disable @typescript-eslint/no-redundant-type-constituents, @typescript-eslint/no-explicit-any */
'use strict';

import type { MutableRefObject } from 'react';
import { runOnJS, runOnUI } from 'react-native-worklets';

import { processColorsInProps } from '../Colors';
import type {
  AnimatedStyle,
  ShadowNodeWrapper,
  StyleProps,
} from '../commonTypes';
import { ReanimatedError } from '../errors';
import type { Descriptor } from '../hook/commonTypes';
import { isJest, shouldBeUseWeb } from '../PlatformChecker';
import type {ReanimatedHTMLElement  } from '../ReanimatedModule/js-reanimated';
import { _updatePropsJS  } from '../ReanimatedModule/js-reanimated';
import { ComponentRegistry } from './ComponentRegistry';
import { processTransformOrigin } from './processTransformOrigin';

let updateProps: (
  viewDescriptors: ViewDescriptorsWrapper,
  updates: StyleProps | AnimatedStyle<any>,
  isAnimatedProps?: boolean
) => void;

if (shouldBeUseWeb()) {
  updateProps = (viewDescriptors, updates, isAnimatedProps) => {
    'worklet';
    viewDescriptors.value?.forEach((viewDescriptor) => {
      const component = viewDescriptor.tag as ReanimatedHTMLElement;
      _updatePropsJS(updates, component, isAnimatedProps);
    });
  };
} else {
  updateProps = (viewDescriptors, updates) => {
    'worklet';
    processColorsInProps(updates);
    if ('transformOrigin' in updates) {
      updates.transformOrigin = processTransformOrigin(updates.transformOrigin);
    }
    global.UpdatePropsManager.update(viewDescriptors, updates);
  };
}

export const updatePropsJestWrapper = (
  viewDescriptors: ViewDescriptorsWrapper,
  updates: AnimatedStyle<any>,
  animatedValues: MutableRefObject<AnimatedStyle<any>>,
  adapters: ((updates: AnimatedStyle<any>) => void)[]
): void => {
  adapters.forEach((adapter) => {
    adapter(updates);
  });
  animatedValues.current.value = {
    ...animatedValues.current.value,
    ...updates,
  };

  updateProps(viewDescriptors, updates);
};

export default updateProps;

function updateJSProps(tag: number, props: StyleProps) {
  const component = ComponentRegistry.getComponent(tag);
  if (component) {
    component._updateStylePropsJS(props);
  }
}

function createUpdatePropsManager() {
  'worklet';
  const operations: {
    shadowNodeWrapper: ShadowNodeWrapper;
    updates: StyleProps | AnimatedStyle<any>;
    tag: number;
  }[] = [];


  const lastUpdateFrameTimeByTag: Record<number, number | undefined> = {};
  const lastUpdateByTag: Record<number, StyleProps | AnimatedStyle<any> | undefined> = {};
  const scheduledFrameIds: Record<number, number | undefined> = {};

  function checkUpdate(tag: number) {
    'worklet';

    const currentFrameTime = global.__frameTimestamp;
    const lastUpdateFrameTime = lastUpdateFrameTimeByTag[tag];
    if (!currentFrameTime || !lastUpdateFrameTime) {
      return;
    }

    if (currentFrameTime - lastUpdateFrameTime >= 36) { // ~ 2x frames
      // Animation appears to have settled - update component props on JS
      runOnJS(updateJSProps)(tag, lastUpdateByTag[tag]);
      lastUpdateByTag[tag] = undefined;
      return;
    }

    if (scheduledFrameIds[tag]) {
      // Note: REA/Worklets doesn't support cancelAnimationFrame
      return;
    }

    scheduledFrameIds[tag] = requestAnimationFrame(() => {
      'worklet';
      scheduledFrameIds[tag] = undefined;
      checkUpdate(tag);
    });
  }

  return {
    update(
      viewDescriptors: ViewDescriptorsWrapper,
      updates: StyleProps | AnimatedStyle<any>
    ) {
      viewDescriptors.value.forEach((viewDescriptor) => {
        const tag = viewDescriptor.tag as number; // on mobile it should be a number
        operations.push({
          shadowNodeWrapper: viewDescriptor.shadowNodeWrapper,
          tag,
          updates,
        });
        if (operations.length === 1) {
          queueMicrotask(this.flush);
        }

        lastUpdateByTag[tag] = updates;
        lastUpdateFrameTimeByTag[tag] = global.__frameTimestamp;
      });
    },
    flush(this: void) {
      global._updateProps!(operations);
      operations.forEach(({ tag }) => {
        checkUpdate(tag);
      });
      operations.length = 0;
    },
  };
}

if (shouldBeUseWeb()) {
  const maybeThrowError = () => {
    // Jest attempts to access a property of this object to check if it is a Jest mock
    // so we can't throw an error in the getter.
    if (!isJest()) {
      throw new ReanimatedError(
        '`UpdatePropsManager` is not available on non-native platform.'
      );
    }
  };
  global.UpdatePropsManager = new Proxy(
    {},
    {
      get: maybeThrowError,
      set: () => {
        maybeThrowError();
        return false;
      },
    }
  );
} else {
  runOnUI(() => {
    'worklet';
    global.UpdatePropsManager = createUpdatePropsManager();
  })();
}

/**
 * This used to be `SharedValue<Descriptors[]>` but objects holding just a
 * single `value` prop are fine too.
 */
interface ViewDescriptorsWrapper {
  value: Readonly<Descriptor[]>;
}
