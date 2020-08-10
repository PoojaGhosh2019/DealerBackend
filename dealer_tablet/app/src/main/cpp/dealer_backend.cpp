#include <jni.h>
#include <string>

#include <android/log.h>
#include "dealer_tablet_backend.h"


extern "C" JNIEXPORT jint JNICALL
Java_com_example_dealer_1tablet_MainActivity_startBackend(
        JNIEnv* env,
        jobject /* this */,jstring macAddr) {
    const char * mac = env->GetStringUTFChars(macAddr, nullptr);

    int ret = backend_main(mac);
    //int ret = 0;

    return ret;
}


