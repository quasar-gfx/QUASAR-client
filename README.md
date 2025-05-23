# QUASAR Client for Meta Quest Headsets

## Getting Started

### Cloning the repo
```
git clone --recursive git@github.com:EdwardLu2018/QuestClient.git
```

If you accidentally cloned this repo with `--recursive-submodules`, you can do:
```
git submodule update --init --recursive
```

## Build and Run Code

The following steps assume you have [Android Studio](https://developer.android.com/studio) installed on your computer.

First, connect your headset to your host machine. The headset can either be connected using a cable or wirelessly (see below).

Then, you can build and run code by opening `QuestClientApps` in Android Studio, selecting an app in the `Configurations`, and clicking the play button.

## Debugging

To debug/view print statements, see the `Logcat` tab on Android Studio if the headset is connected.

To wirelessly connect to your headset, type this into your terminal __with your headset plugged in__:
```
adb tcpip 5555
adb connect <headset's ip address>:5555
```
Now, you don't need to plug in your headset to upload code!

## Credit

A majority of the OpenXR code is based on the [OpenXR Android OpenGL ES tutorial](https://openxr-tutorial.com/android/opengles/index.html).

The renderer source code can be found [here](https://github.com/EdwardLu2018/QUASAR).

## Citation
If you find this project helpful for any research-related purposes, please consider citing our paper:
```
@article{quasar,
    title={QUASAR: Quad-based Adaptive Streaming And Rendering},
    author={Lu, Edward and Rowe, Anthony},
    journal={ACM Transactions on Graphics (TOG)},
    volume={44},
    number={4},
    year={2025},
    publisher={ACM New York, NY, USA},
    url={https://doi.org/10.1145/3731213},
    doi={10.1145/3731213},
}
```
