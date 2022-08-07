You have to build irrlicht library from source/Irrlicht/Android-SDL2 directory first.

Then build in this directory with:

export ANDROID_HOME=/path/to/android-sdk/
export SDL2_PATH=/path/to/SDL2

./gradlew assembleDebug

Compiled apk will be available in app/build/outputs/apk/debug.
