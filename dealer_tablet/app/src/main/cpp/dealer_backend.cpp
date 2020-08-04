#include <jni.h>
#include <string>

#include <android/log.h>
#include "dealer_tablet_backend.h"


extern "C" JNIEXPORT jint JNICALL
Java_com_example_dealer_1tablet_MainActivity_startBackend(
        JNIEnv* env,
        jobject /* this */) {

    int ret = main();
    //int ret = 0;

    return ret;
}


