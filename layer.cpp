#include "vulkan/vulkan.h"
#include "vulkan/vk_layer.h"
#include "vulkan/vk_layer_dispatch_table.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C"

static std::map<void*, VkLayerInstanceDispatchTable> gInstanceDispatch;
static std::map<void*, VkLayerDispatchTable> gDeviceDispatch;

static constexpr char kEnvVariable[] = "VULKAN_DEVICE_INDEX";

template <typename DispatchableType>
inline void* GetKey(DispatchableType object)
{
    return *(void**)object;
}

template <typename DispatchableType>
inline VkLayerInstanceDispatchTable& GetInstanceDispatch(DispatchableType object)
{
    return gInstanceDispatch[GetKey(object)];
}

template <typename DispatchableType>
inline VkLayerDispatchTable& GetDeviceDispatch(DispatchableType object)
{
    return gDeviceDispatch[GetKey(object)];
}

static VkResult ChooseDevice(VkInstance                          instance,
                             const VkLayerInstanceDispatchTable& dispatch,
                             const char* const                   env,
                             VkPhysicalDevice&                   outDevice)
{
    std::vector<VkPhysicalDevice> devices;
    uint32_t count = 0;

    VkResult result = dispatch.EnumeratePhysicalDevices(instance, &count, nullptr);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (count == 0)
    {
        outDevice = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }

    devices.resize(count);

    result = dispatch.EnumeratePhysicalDevices(instance, &count, &devices[0]);

    if (result != VK_SUCCESS)
    {
        return result;
    }

    int deviceIndex = atoi(env);

    if (deviceIndex >= count)
    {
        fprintf(stderr, "Device index %d does not exist, returning device 0\n", deviceIndex);
        deviceIndex = 0;
    }
    else
    {
        printf("Using Vulkan device index %d\n", deviceIndex);
    }

    outDevice = devices[deviceIndex];
    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDevices(VkInstance        instance,
                                            uint32_t*         pPhysicalDeviceCount,
                                            VkPhysicalDevice* pPhysicalDevices)
{
    const VkLayerInstanceDispatchTable& dispatch = GetInstanceDispatch(instance);

    const char* const env = getenv(kEnvVariable);
    if (!env)
    {
        return dispatch.EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    }

    VkPhysicalDevice device;
    VkResult result = ChooseDevice(instance, dispatch, env, device);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (device == VK_NULL_HANDLE)
    {
        *pPhysicalDeviceCount = 0;
    }
    else if (!pPhysicalDevices)
    {
        *pPhysicalDeviceCount = 1;
    }
    else if (*pPhysicalDeviceCount > 0)
    {
        *pPhysicalDevices     = device;
        *pPhysicalDeviceCount = 1;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDeviceGroupsKHR(VkInstance                          instance,
                                                    uint32_t*                           pPhysicalDeviceGroupCount,
                                                    VkPhysicalDeviceGroupPropertiesKHR* pPhysicalDeviceGroups)
{
    const VkLayerInstanceDispatchTable& dispatch = GetInstanceDispatch(instance);

    const char* const env = getenv(kEnvVariable);
    if (!env)
    {
        return dispatch.EnumeratePhysicalDeviceGroupsKHR(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroups);
    }

    /* Just return a single device group containing the requested device. */
    VkPhysicalDevice device;
    VkResult result = ChooseDevice(instance, dispatch, env, device);

    if (result != VK_SUCCESS)
    {
        return result;
    }
    else if (device == VK_NULL_HANDLE)
    {
        *pPhysicalDeviceGroupCount = 0;
    }
    else if (!pPhysicalDeviceGroups)
    {
        *pPhysicalDeviceGroupCount = 1;
    }
    else if (*pPhysicalDeviceGroupCount > 0)
    {
        *pPhysicalDeviceGroupCount = 1;

        pPhysicalDeviceGroups[0].physicalDeviceCount = 1;
        pPhysicalDeviceGroups[0].physicalDevices[0]  = device;
        pPhysicalDeviceGroups[0].subsetAllocation    = VK_FALSE;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumeratePhysicalDeviceGroups(VkInstance                       instance,
                                                 uint32_t*                        pPhysicalDeviceGroupCount,
                                                 VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroups)
{
    return DeviceChooserLayer_EnumeratePhysicalDeviceGroupsKHR(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroups);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_CreateInstance(const VkInstanceCreateInfo*  pCreateInfo,
                                  const VkAllocationCallbacks* pAllocator,
                                  VkInstance*                  pInstance)
{
    VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*)pCreateInfo->pNext;

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerInstanceCreateInfo*)layerCreateInfo->pNext;
    }

    if (!layerCreateInfo)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;

    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");

    VkResult ret = createFunc(pCreateInfo, pAllocator, pInstance);
    if (ret != VK_SUCCESS)
    {
        return ret;
    }

    #define GET(func) \
        dispatchTable.func = (PFN_vk##func)gpa(*pInstance, "vk" #func);

    VkLayerInstanceDispatchTable dispatchTable;
    dispatchTable.GetInstanceProcAddr = gpa;
    GET(EnumerateDeviceExtensionProperties);
    GET(DestroyInstance);
    GET(EnumeratePhysicalDevices);
    GET(EnumeratePhysicalDeviceGroups);
    GET(EnumeratePhysicalDeviceGroupsKHR);

    #undef GET

    gInstanceDispatch[GetKey(*pInstance)] = dispatchTable;

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DeviceChooserLayer_DestroyInstance(VkInstance                   instance,
                                   const VkAllocationCallbacks* pAllocator)
{
    void* const key = GetKey(instance);

    gInstanceDispatch[key].DestroyInstance(instance, pAllocator);

    gInstanceDispatch.erase(key);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_CreateDevice(VkPhysicalDevice             physicalDevice,
                                const VkDeviceCreateInfo*    pCreateInfo,
                                const VkAllocationCallbacks* pAllocator,
                                VkDevice*                    pDevice)
{
    VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*)pCreateInfo->pNext;

    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                               layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerDeviceCreateInfo*)layerCreateInfo->pNext;
    }

    if (!layerCreateInfo)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr gdpa   = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;

    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

    VkResult ret = createFunc(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (ret != VK_SUCCESS)
    {
        return ret;
    }

    #define GET(func) \
        dispatchTable.func = (PFN_vk##func)gdpa(*pDevice, "vk" #func);

    VkLayerDispatchTable dispatchTable;
    dispatchTable.GetDeviceProcAddr = gdpa;
    GET(DestroyDevice);

    #undef GET

    gDeviceDispatch[GetKey(*pDevice)] = dispatchTable;

    return VK_SUCCESS;
}

VK_LAYER_EXPORT void VKAPI_CALL
DeviceChooserLayer_DestroyDevice(VkDevice                     device,
                                 const VkAllocationCallbacks* pAllocator)
{
    void* const key = GetKey(device);

    gDeviceDispatch[key].DestroyDevice(device, pAllocator);

    gDeviceDispatch.erase(key);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateInstanceLayerProperties(uint32_t*          pPropertyCount,
                                                    VkLayerProperties* pProperties)
{
    if (pPropertyCount)
    {
        *pPropertyCount = 1;
    }

    if (pProperties)
    {
        strcpy(pProperties->layerName,   "VK_LAYER_AEJS_DeviceChooserLayer");
        strcpy(pProperties->description, "Device chooser layer");

        pProperties->implementationVersion = 1;
        pProperties->specVersion           = VK_API_VERSION_1_0;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateDeviceLayerProperties(VkPhysicalDevice   physicalDevice,
                                                  uint32_t*          pPropertyCount,
                                                  VkLayerProperties* pProperties)
{
    return DeviceChooserLayer_EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateInstanceExtensionProperties(const char*            pLayerName,
                                                        uint32_t*              pPropertyCount,
                                                        VkExtensionProperties* pProperties)
{
    if (!pLayerName || strcmp(pLayerName, "VK_LAYER_AEJS_DeviceChooserLayer"))
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    if (pPropertyCount)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

VK_LAYER_EXPORT VkResult VKAPI_CALL
DeviceChooserLayer_EnumerateDeviceExtensionProperties(VkPhysicalDevice       physicalDevice,
                                                      const char*            pLayerName,
                                                      uint32_t*              pPropertyCount,
                                                      VkExtensionProperties* pProperties)
{
    if (!pLayerName || strcmp(pLayerName, "VK_LAYER_AEJS_DeviceChooserLayer"))
    {
        if (physicalDevice == VK_NULL_HANDLE)
        {
            return VK_SUCCESS;
        }

        return GetInstanceDispatch(physicalDevice).EnumerateDeviceExtensionProperties(physicalDevice,
                                                                                      pLayerName,
                                                                                      pPropertyCount,
                                                                                      pProperties);
    }

    if (pPropertyCount)
    {
        *pPropertyCount = 0;
    }

    return VK_SUCCESS;
}

#define GETPROCADDR(func) if(!strcmp(pName, "vk" #func)) return (PFN_vkVoidFunction)&DeviceChooserLayer_##func;

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DeviceChooserLayer_GetDeviceProcAddr(VkDevice    device,
                                     const char* pName)
{
    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(EnumerateDeviceLayerProperties);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);

    return GetDeviceDispatch(device).GetDeviceProcAddr(device, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL
DeviceChooserLayer_GetInstanceProcAddr(VkInstance  instance,
                                       const char* pName)
{
    GETPROCADDR(GetInstanceProcAddr);
    GETPROCADDR(EnumerateInstanceLayerProperties);
    GETPROCADDR(EnumerateInstanceExtensionProperties);
    GETPROCADDR(CreateInstance);
    GETPROCADDR(DestroyInstance);
    GETPROCADDR(EnumeratePhysicalDevices);
    GETPROCADDR(EnumeratePhysicalDeviceGroups);
    GETPROCADDR(EnumeratePhysicalDeviceGroupsKHR);

    GETPROCADDR(GetDeviceProcAddr);
    GETPROCADDR(EnumerateDeviceLayerProperties);
    GETPROCADDR(EnumerateDeviceExtensionProperties);
    GETPROCADDR(CreateDevice);
    GETPROCADDR(DestroyDevice);

    return GetInstanceDispatch(instance).GetInstanceProcAddr(instance, pName);
}
