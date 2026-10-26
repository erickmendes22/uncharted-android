/* g2-arm64: game_prepare for the Android app's launcher (dev.unchartedg2.game.Launcher), in a
 * library of its own (libbbprepare.so): the launcher does not load the game's libraries. */
#include <jni.h>
#include <stddef.h>

#include "game_prepare.h"

/* static String prepareGame(String gameDir, String exe, String outDir): null on success, else the
 * reason. */
JNIEXPORT jstring JNICALL Java_dev_unchartedg2_game_Launcher_prepareGame(JNIEnv *env, jclass cls, jstring game,
                                                                        jstring exe, jstring out) {
    (void)cls;
    const char *g = (*env)->GetStringUTFChars(env, game, NULL);
    const char *e = (*env)->GetStringUTFChars(env, exe, NULL);
    const char *o = (*env)->GetStringUTFChars(env, out, NULL);
    char error[512] = "";
    int failed = g && e && o ? game_prepare(g, e, o, error, sizeof(error)) : -1;
    if (o) (*env)->ReleaseStringUTFChars(env, out, o);
    if (e) (*env)->ReleaseStringUTFChars(env, exe, e);
    if (g) (*env)->ReleaseStringUTFChars(env, game, g);
    return failed ? (*env)->NewStringUTF(env, error[0] ? error : "out of memory") : NULL;
}

/* static String sfoValue(String gameDir, String key): the value as text, null if missing. */
JNIEXPORT jstring JNICALL Java_dev_unchartedg2_game_Launcher_sfoValue(JNIEnv *env, jclass cls, jstring game,
                                                                     jstring key) {
    (void)cls;
    const char *g = (*env)->GetStringUTFChars(env, game, NULL);
    const char *k = (*env)->GetStringUTFChars(env, key, NULL);
    char value[512];
    int found = g && k && game_sfo_value(g, k, value, sizeof(value)) == 0;
    if (k) (*env)->ReleaseStringUTFChars(env, key, k);
    if (g) (*env)->ReleaseStringUTFChars(env, game, g);
    return found ? (*env)->NewStringUTF(env, value) : NULL;
}

/* KGSL device-info ABI, adapted from RedVorteX-TV/Uncharted-arm kgsl_chip_id.c. */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
struct daedalus_kgsl_devinfo {
    unsigned int device_id, chip_id, mmu_enabled;
    unsigned long gmem_gpubaseaddr;
    unsigned int gpu_id;
    size_t gmem_sizebytes;
};
struct daedalus_kgsl_property { unsigned int type; void *value; size_t sizebytes; };
JNIEXPORT jlong JNICALL Java_dev_unchartedg2_game_Launcher_gpuChipId(JNIEnv *env, jclass cls) {
    (void)env; (void)cls;
    int fd = open("/dev/kgsl-3d0", O_RDWR | O_CLOEXEC);
    if (fd < 0) return 0;
    struct daedalus_kgsl_devinfo info = {0};
    struct daedalus_kgsl_property prop = {1, &info, sizeof(info)};
    int result = ioctl(fd, _IOWR(0x09, 0x2, struct daedalus_kgsl_property), &prop);
    close(fd);
    return result == 0 ? (jlong)info.chip_id : 0;
}
