// A G2 probe from the process of an ordinary app (NativeActivity, no Java):
//   1) vaprobe: the address space in a process forked from zygote (ART, its own mappings);
//   2) Vulkan: extension features (robustness2 etc.) missing from `cmd gpu vkjson`.
// Report: files/report.txt (adb exec-out run-as dev.unchartedg2.probe cat files/report.txt).
#define VAPROBE_LIB
#include "../vaprobe.c"

#include <android/log.h>
#include <android/native_activity.h>
#include <pthread.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#define CHAIN(var, type, stype) type var = {.sType = stype}; *next = &var; next = &var.pNext

static void vulkan_report(FILE *out) {
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance inst;
    VkResult r = vkCreateInstance(&ici, NULL, &inst);
    if (r != VK_SUCCESS) { fprintf(out, "vkCreateInstance: %d\n", r); return; }
    uint32_t n = 1;
    VkPhysicalDevice pd;
    vkEnumeratePhysicalDevices(inst, &n, &pd);
    if (!n) { fprintf(out, "no physical devices\n"); return; }

    void **next;
    VkPhysicalDeviceFeatures2 f2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    next = &f2.pNext;
    CHAIN(rob2, VkPhysicalDeviceRobustness2FeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
    CHAIN(div, VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT);
    CHAIN(cbc, VkPhysicalDeviceCustomBorderColorFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT);
    CHAIN(vid, VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT);
    CHAIN(dcc, VkPhysicalDeviceDepthClipControlFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT);
    CHAIN(dce, VkPhysicalDeviceDepthClipEnableFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT);
    CHAIN(lr, VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT);
    CHAIN(af, VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT);
    CHAIN(wg, VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR);
    CHAIN(i2d, VkPhysicalDeviceImage2DViewOf3DFeaturesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT);
    CHAIN(v11, VkPhysicalDeviceVulkan11Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    CHAIN(v12, VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    CHAIN(v13, VkPhysicalDeviceVulkan13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
    vkGetPhysicalDeviceFeatures2(pd, &f2);

    VkPhysicalDeviceProperties2 p2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    next = &p2.pNext;
    CHAIN(drv, VkPhysicalDeviceDriverProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    CHAIN(p13, VkPhysicalDeviceVulkan13Properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES);
    CHAIN(p11, VkPhysicalDeviceVulkan11Properties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES);
    CHAIN(rob2p, VkPhysicalDeviceRobustness2PropertiesEXT, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT);
    CHAIN(pushp, VkPhysicalDevicePushDescriptorPropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR);
    vkGetPhysicalDeviceProperties2(pd, &p2);
    VkPhysicalDeviceProperties *p = &p2.properties;
    VkPhysicalDeviceFeatures *f = &f2.features;

    fprintf(out, "device: %s, api %u.%u.%u, driver %s / %s (conformance %u.%u.%u.%u)\n", p->deviceName,
            VK_VERSION_MAJOR(p->apiVersion), VK_VERSION_MINOR(p->apiVersion), VK_VERSION_PATCH(p->apiVersion),
            drv.driverName, drv.driverInfo, drv.conformanceVersion.major, drv.conformanceVersion.minor,
            drv.conformanceVersion.subminor, drv.conformanceVersion.patch);
#define F(s, x) fprintf(out, "  %-44s %u\n", #s "." #x, (unsigned)(s).x)
    fprintf(out, "required by renderer (vk_instance.cpp):\n");
    F(rob2, robustBufferAccess2); F(rob2, robustImageAccess2); F(rob2, nullDescriptor);
    fprintf(out, "optional:\n");
    F(div, vertexAttributeInstanceRateDivisor); F(div, vertexAttributeInstanceRateZeroDivisor);
    F(cbc, customBorderColors); F(cbc, customBorderColorWithoutFormat);
    F(vid, vertexInputDynamicState); F(dcc, depthClipControl); F(dce, depthClipEnable);
    F(lr, primitiveTopologyListRestart); F(lr, primitiveTopologyPatchListRestart);
    F(af, shaderBufferFloat32Atomics); F(af, shaderBufferFloat32AtomicAdd); F(af, shaderImageFloat32Atomics);
    F(wg, workgroupMemoryExplicitLayout); F(i2d, image2DViewOf3D); F(i2d, sampler2DViewOf3D);
    F(*f, geometryShader); F(*f, tessellationShader); F(*f, dualSrcBlend); F(*f, logicOp);
    F(*f, depthClamp); F(*f, depthBounds); F(*f, shaderFloat64); F(*f, shaderInt64); F(*f, shaderInt16);
    F(*f, shaderStorageImageWriteWithoutFormat); F(*f, shaderStorageImageMultisample);
    F(*f, sparseBinding); F(*f, sparseResidencyBuffer); F(*f, robustBufferAccess);
    F(v11, storageBuffer16BitAccess); F(v11, shaderDrawParameters);
    F(v12, shaderFloat16); F(v12, shaderInt8); F(v12, shaderBufferInt64Atomics); F(v12, shaderSharedInt64Atomics);
    F(v12, scalarBlockLayout); F(v12, bufferDeviceAddress); F(v12, timelineSemaphore); F(v12, drawIndirectCount);
    F(v12, samplerMirrorClampToEdge); F(v12, shaderOutputLayer); F(v12, separateDepthStencilLayouts);
    F(v13, robustImageAccess); F(v13, shaderDemoteToHelperInvocation); F(v13, subgroupSizeControl);
    F(v13, synchronization2); F(v13, dynamicRendering); F(v13, shaderIntegerDotProduct); F(v13, maintenance4);
    fprintf(out, "limits:\n");
    fprintf(out, "  maxBufferSize                %llu MiB\n", (unsigned long long)(p13.maxBufferSize >> 20));
    fprintf(out, "  maxStorageBufferRange        %u MiB\n", p->limits.maxStorageBufferRange >> 20);
    fprintf(out, "  maxUniformBufferRange        %u KiB\n", p->limits.maxUniformBufferRange >> 10);
    fprintf(out, "  maxMemoryAllocationSize      %llu MiB\n", (unsigned long long)(p11.maxMemoryAllocationSize >> 20));
    fprintf(out, "  maxMemoryAllocationCount     %u\n", p->limits.maxMemoryAllocationCount);
    fprintf(out, "  sparseAddressSpaceSize       %llu GiB\n", (unsigned long long)(p->limits.sparseAddressSpaceSize >> 30));
    fprintf(out, "  maxPushDescriptors           %u\n", pushp.maxPushDescriptors);
    fprintf(out, "  robustStorageBufferAccessSizeAlignment %llu\n", (unsigned long long)rob2p.robustStorageBufferAccessSizeAlignment);
    fprintf(out, "  minStorageBufferOffsetAlignment %llu\n", (unsigned long long)p->limits.minStorageBufferOffsetAlignment);
    fprintf(out, "  subgroupSize %u (min %u, max %u)\n", p11.subgroupSize, p13.minSubgroupSize, p13.maxSubgroupSize);
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++)
        fprintf(out, "  heap %u: %llu MiB flags 0x%x\n", i, (unsigned long long)(mp.memoryHeaps[i].size >> 20),
                mp.memoryHeaps[i].flags);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        fprintf(out, "  type %u: heap %u flags 0x%x\n", i, mp.memoryTypes[i].heapIndex, mp.memoryTypes[i].propertyFlags);
    vkDestroyInstance(inst, NULL);
}

static void *run(void *arg) {
    ANativeActivity *activity = arg;
    char path[512];
    snprintf(path, sizeof path, "%s/report.txt", activity->internalDataPath);
    FILE *out = fopen(path, "w");
    if (!out) { __android_log_print(ANDROID_LOG_ERROR, "G2PROBE", "fopen %s failed", path); return NULL; }
    fprintf(out, "=== vulkan\n");
    vulkan_report(out);
    fflush(out);
    fprintf(out, "=== vaprobe (app process)\n");
    vaprobe_report(out);
    FILE *st = fopen("/proc/self/status", "r");
    if (st) {
        char line[256];
        fprintf(out, "=== /proc/self/status (Vm*)\n");
        while (fgets(line, sizeof line, st)) if (!strncmp(line, "Vm", 2)) fputs(line, out);
        fclose(st);
    }
    fprintf(out, "=== done\n");
    fclose(out);
    __android_log_print(ANDROID_LOG_INFO, "G2PROBE", "report written to %s", path);
    ANativeActivity_finish(activity);
    return NULL;
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity, void *saved, size_t saved_size) {
    (void)saved; (void)saved_size;
    pthread_t t;
    pthread_create(&t, NULL, run, activity);
    pthread_detach(t);
}
