#!/usr/bin/env python3
"""Compares a device's vkjson (adb shell cmd gpu vkjson) with the requirements of the port's renderer
(uncharted_pc/gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp, TargetVulkanApiVersion=1.3).
Run: python3 scripts/g2/vk_check.py notes/devices/g2-<date>/vkjson.json"""
import json
import sys

REQUIRED_EXT = ['VK_KHR_swapchain', 'VK_KHR_push_descriptor', 'VK_EXT_vertex_attribute_divisor',
                'VK_EXT_robustness2']
OPTIONAL_EXT = [
    'VK_KHR_maintenance5', 'VK_KHR_maintenance8', 'VK_EXT_attachment_feedback_loop_layout',
    'VK_EXT_depth_range_unrestricted', 'VK_EXT_extended_dynamic_state3', 'VK_EXT_custom_border_color',
    'VK_EXT_depth_clip_control', 'VK_EXT_depth_clip_enable', 'VK_EXT_vertex_input_dynamic_state',
    'VK_EXT_primitive_topology_list_restart', 'VK_KHR_fragment_shader_barycentric',
    'VK_EXT_provoking_vertex', 'VK_EXT_shader_stencil_export', 'VK_EXT_shader_atomic_float',
    'VK_EXT_shader_atomic_float2', 'VK_KHR_workgroup_memory_explicit_layout',
    'VK_EXT_image_2d_view_of_3d', 'VK_EXT_image_view_min_lod', 'VK_EXT_memory_budget',
    'VK_KHR_compute_shader_derivatives', 'VK_KHR_shader_clock', 'VK_EXT_device_fault',
    'VK_AMD_shader_explicit_vertex_parameter', 'VK_AMD_gcn_shader', 'VK_AMD_shader_trinary_minmax',
    'VK_AMD_shader_image_load_store_lod', 'VK_AMD_mixed_attachment_samples',
    'VK_NV_framebuffer_mixed_samples']
# Core features the renderer enables "if present": missing = a workaround path or artifacts.
CORE_FEATURES = ['robustBufferAccess', 'imageCubeArray', 'independentBlend', 'geometryShader',
                 'tessellationShader', 'sampleRateShading', 'dualSrcBlend', 'logicOp',
                 'multiDrawIndirect', 'depthClamp', 'depthBiasClamp', 'fillModeNonSolid',
                 'depthBounds', 'wideLines', 'multiViewport', 'samplerAnisotropy',
                 'vertexPipelineStoresAndAtomics', 'fragmentStoresAndAtomics',
                 'shaderImageGatherExtended', 'shaderStorageImageExtendedFormats',
                 'shaderStorageImageMultisample', 'shaderStorageImageWriteWithoutFormat',
                 'shaderClipDistance', 'shaderFloat64', 'shaderInt64', 'shaderInt16',
                 'sparseBinding', 'sparseResidencyBuffer']


def ver(v):
    v = int(v)
    return f'{v >> 22}.{(v >> 12) & 0x3ff}.{v & 0xfff}'


def find(obj, key):
    """The first value of key anywhere in the tree (vkjson spreads features over structures)."""
    if isinstance(obj, dict):
        if key in obj:
            return obj[key]
        for v in obj.values():
            r = find(v, key)
            if r is not None:
                return r
    elif isinstance(obj, list):
        for v in obj:
            r = find(v, key)
            if r is not None:
                return r
    return None


def main(path):
    data = json.load(open(path, encoding='utf-8'))
    dev = data['devices'][0]
    props = dev['properties']
    print(f"device: {props['deviceName']}  api {ver(props['apiVersion'])}  "
          f"driver 0x{int(props['driverVersion']):x}  vendor 0x{int(props['vendorID']):x}")
    drv = find(dev, 'driverInfo') or find(dev, 'driverName')
    if drv:
        print(f'driver info: {drv}')
    api_ok = int(props['apiVersion']) >= (1 << 22 | 3 << 12)
    print(f"Vulkan 1.3: {'OK' if api_ok else 'NO'}")
    exts = {e['extensionName'] for e in dev.get('extensions', [])}
    for layer in dev.get('layers', []):
        pass
    print(f'device extensions: {len(exts)}')
    print('required:')
    for e in REQUIRED_EXT:
        print(f"  {'OK ' if e in exts else 'NO '} {e}")
    for f in ['robustBufferAccess2', 'robustImageAccess2', 'nullDescriptor']:
        v = find(dev, f)
        print(f"  {'OK ' if v else 'NO '} robustness2.{f} = {v}")
    print('optional extensions:')
    for e in OPTIONAL_EXT:
        print(f"  {'yes' if e in exts else ' - '} {e}")
    print('core features (off = workaround/possible artifacts):')
    feats = dev.get('features', {})
    print('  off: ' + ', '.join(f for f in CORE_FEATURES if not feats.get(f)))
    for f in ['shaderBufferInt64Atomics', 'shaderFloat16', 'shaderInt8', 'bufferDeviceAddress',
              'shaderOutputLayer', 'timelineSemaphore', 'dynamicRendering', 'synchronization2',
              'vertexInputDynamicState', 'customBorderColorWithoutFormat',
              'vertexAttributeInstanceRateDivisor', 'vertexAttributeInstanceRateZeroDivisor']:
        print(f'  {f} = {find(dev, f)}')
    mem = dev.get('memory', {})
    for i, h in enumerate(mem.get('memoryHeaps', [])):
        print(f"heap {i}: {int(str(h['size']), 0) / 2**30:.2f} GiB flags={h['flags']}")
    lim = props.get('limits', {})
    for k in ['maxStorageBufferRange', 'maxPushConstantsSize', 'maxBoundDescriptorSets',
              'maxPerStageDescriptorStorageBuffers', 'maxComputeSharedMemorySize',
              'minStorageBufferOffsetAlignment', 'maxImageDimension2D']:
        print(f'  {k} = {lim.get(k)}')


if __name__ == '__main__':
    main(sys.argv[1])
