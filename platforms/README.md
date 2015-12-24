This directory contains the files required to build Qfusion for the platforms that do not use CMake.

Android
=======
The Android version is provided as an Eclipse project. You need Eclipse with the ADT plugin, Android NDK and Android SDK for the API version specified in `targetSdkVersion` in `AndroidManifest.xml`.

To build Qfusion, do the following steps:
* Import the `qfusion` project into your workspace.
* Copy or symlink the `source`, `libsrcs` and `icons` directories from the repository root to `qfusion/jni`.
* In `qfusion/jni/Android.mk`, find `NDK_APP_DST_DIR`, uncomment it and set it to the temporary folder where the `cgame` and `game` modules will be placed before you add them to your modules PK3.
* Run the project or export and sign the APK.

The engine loads the game files from the APK expansion files which are uncompressed zip archives containing game PK3s. Only OBBs that are created for the current major and minor version will be loaded, so the games must be redownloaded entirely in case of a major update.

The user data is located in `/sdcard/Android/data/package name/files/APP_VERSION_MAJOR.APP_VERSION_MINOR/game directory`.
