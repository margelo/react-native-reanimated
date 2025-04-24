import { useEffect, useState } from 'react';
import { Switch, View } from 'react-native';
import Animated, {
  useAnimatedStyle,
  withSequence,
  withTiming,
  useSharedValue,
} from 'react-native-reanimated';

export default function App() {
  const a = useSharedValue(0);
  useEffect(() => {
    a.value = withTiming(100, { duration: 1000 })
  }, [])
  const styleA = useAnimatedStyle(() => ({
    width: a.value,
  }));
  const styleB = useAnimatedStyle(() => ({
    width: withSequence(withTiming(0), withTiming(200, { duration: 1000 })),
  }));

  const [state, setState] = useState(false);

  useEffect(() => {
    setTimeout(() => {
      setState(true);
    }, 3_000); 
  }, []);

  return (
    <View
      style={{
        alignItems: 'center',
        flex: 1,
        justifyContent: 'center',
      }}>
      <Switch value={state} onChange={() => setState(!state)} />
      <Animated.View
        style={[
          styleA,
          {
            backgroundColor: 'red',
            height: 50,
          },
        ]}
      />
      <Animated.View
        style={[
          styleB,
          {
            backgroundColor: 'blue',
            height: 50,
          },
        ]}
      />
    </View>
  );
}

