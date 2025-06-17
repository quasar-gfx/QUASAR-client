# QUASAR Client

## Getting Started

### Cloning the Repo:
```
git clone --recursive git@github.com:EdwardLu2018/QUASAR-client.git
```

If you accidentally cloned this repo without `--recursive-submodules`, you can do:
```
git submodule update --init --recursive
```

## Build and Run Code

The following steps assume you have the latest [Android Studio](https://developer.android.com/studio) installed on your computer:

1. Connect your headset to your host machine. The headset can either be connected using a cable or wirelessly (see  [Debugging](#Debugging)).

2. If this is the first time you are launching Android Studio, select Open an existing Android Studio project. If you have launched Android Studio before, click File > Open instead.

3. Open `QuestClientApps/build.gradle` in Android Studio, select an app in the `Configurations` menu at the top (dropdown to the left of play/Run button), and click the play/Run button to build and upload (first time opening and building may take a while).
   * You could also open an individual sample app from the `QuestClientApps/Apps/` folders. For example, `QuestClientApps/Apps/QUASARViewer/build.gradle`

Note: This code has been tested on Meta Quest 2, Meta Quest Pro, and Meta Quest 3.

## Sample Apps

All apps allow you to move through a scene using the controller joysticks. You will move in the direction you are looking.

### SceneViewer

The Scene Viewer app loads a GLTF/GLB scene on the headset to view (you can download example scenes from https://drive.google.com/file/d/1zL_hsmtjyOcAbNbud92aNCxjO1kwEqlK/view?usp=drive_link).

Download and unzip into `QuestClientApps/Apps/SceneViewer/assets/models/scenes/` (this will be gitignored).

Note: __You can only have ONE glb in the `scenes/` directory at once since Android will run out of storage if you have them all.__ So, just have `RobotLab.glb` in `scenes/`.

### ATWClient

The ATW Client app allows the headset to act as a reciever for a stereo video stream from the server sent across a network. The headset will reproject each eye using a homography to warp the images along a plane.

First, open `QuestClientApps/Apps/ATWClient/include/ATWClient.h` and change `std::string serverIP = "192.168.4.140";` to your __server's__ IP address. Then, on the [QUASAR](https://github.com/quasar-gfx/QUASAR) repo, build and run `build/apps/atw/streamer/atw_streamer`, with your arguments of choice. Make sure the arguments match the ones in `ATWClient.h`! Then, run the app on the headset!

Example:
```
# in build directory
cd apps/atw/streamer
./atw_streamer --size 2048x1024 --scene ../assets/scenes/robot_lab.json --video-url <headset's IP>:12345 --vr 1
```

To get your headset's IP address, you look in the Settings or you can run
```
adb shell ip addr show wlan0  # look for address after 'inet'
```
in a terminal with your headset connected. Make sure you have adb installed.

### MeshWarpViewer

The MeshWarp Viewer app will load a saved static frame from MeshWarp to view on the headset. Simply run the app on the headset to view!

### MeshWarpClient

The MeshWarp Client app allows the headset to act as a reciever for a video and depth stream from the server sent across a network. The headset will reconstruct a mesh using the depth stream and texture map it with the video for reprojection.

First, open `QuestClientApps/Apps/MeshWarpClient/include/MeshWarpClient.h` and change `std::string serverIP = "192.168.4.140";` to your __server's__ IP address. Then, on the [QUASAR](https://github.com/quasar-gfx/QUASAR) repo, build and run `build/apps/meshwarp/streamer/mw_streamer`, with your arguments of choice. Make sure the arguments match the ones in `MeshWarpClient.h`! Then, run the app on the headset!

Example:
```
# in build directory
cd apps/meshwarp/streamer
./mw_streamer --size 1920x1080 --scene ../assets/scenes/robot_lab.json --video-url <headset's IP>:12345 --depth-url <headset's IP>:65432 --depth-factor 4
```

### QuadsViewer

The Quads Viewer app will load a saved static frame from QuadWarp to view on the headset. You can change the scene by editing `std::string sceneName = "robot_lab";` in `QuestClientApps/Apps/QuadsViewer/include/QuadsViewer.h`.

### QUASARViewer

The QUASAR Viewer app will load a saved static frame from QUASAR to view on the headset. You can change the scene by editing `std::string sceneName = "robot_lab";` in `QuestClientApps/Apps/QUASARViewer/include/QUASARViewer.h`.

## Debugging

To wirelessly connect to your headset, type this into your terminal __with your headset plugged in__:
```
adb tcpip 5555
adb connect <headset's ip address>:5555
```
Now, you don't need to plug in your headset to upload code!

To debug/view print statements, see the `Logcat` tab on Android Studio if the headset is connected.

## Credit

A majority of the OpenXR code is based on the [OpenXR Android OpenGL ES tutorial](https://openxr-tutorial.com/android/opengles/index.html).

The renderer source code can be found [here](https://github.com/quasar-gfx/QUASAR).

## Citation
If you find this project helpful for any research-related purposes, please consider citing our paper:
```
@article{lu2025quasar,
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
