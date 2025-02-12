# Quest Headset Client for OpenGL Remote Rendering (WIP)

```
git clone --recursive git@github.com:EdwardLu2018/QuestClient.git
```

## Build and Run Code on Headset

You can build and run code by opening this project in Android Studio, selecting an app in the Configurations, and clicking the play button with a headset connected to your host machine.

The headset can either be connected using a cable or wirelessly (see below).

## Debugging

To debug/view print statements, see the `Logcat` tab on Android Studio.

To wirelessly connect to your headset, type this into your terminal __with your headset plugged in__:
```
adb tcpip 5555
adb connect <headset's ip address>:5555
```
Now, you don't need to plug in your headset to upload code!

## Credit

The OpenXR code is based on: https://openxr-tutorial.com/android/opengles/index.html

The renderer can be found here: https://github.com/EdwardLu2018/opengl-remote-rendering
