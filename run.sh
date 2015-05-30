./build.sh $1

export BUILD_MODULE=Oculus360VideosSDK
echo "========================== Install & Run "${BUILD_MODULE}" ==========================="

if [ "$1" != "clean" ]; then
    #For the time being, just build debug releases until we have a keystore for signing
    #We do the rename to avoid confusion, since ant will name this as -debug.apk but
    #the native code is all built with release optimizations.
    ant -quiet debug

    cp bin/${BUILD_MODULE}-debug.apk bin/${BUILD_MODULE}.apk
    adb install -r bin/${BUILD_MODULE}.apk
    adb shell am start com.oculus.oculus360videossdk/com.oculus.oculus360videossdk.MainActivity
fi 


