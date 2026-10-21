#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <volk.h>

static const uint32_t kTriangleVert[] = {
#include "triangle.vert.inc"
};

static const uint32_t kTriangleFrag[] = {
#include "triangle.frag.inc"
};

static void Check(VkResult result, const char* call) {
  if (result < 0) {
    fprintf(stderr, "%s failed: %d\n", call, result);
    exit(EXIT_FAILURE);
  }
}

static VkShaderModule CreateShaderModule(VkDevice device, const uint32_t* code,
                                         size_t size) {
  VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = code,
  };
  VkShaderModule module;
  Check(vkCreateShaderModule(device, &info, NULL, &module),
        "vkCreateShaderModule");
  return module;
}

int main(void) {
#ifndef NDEBUG
#ifdef _WIN32
  _putenv_s("VK_ADD_LAYER_PATH", RAY_LAYER_PATH);
#else
  setenv("VK_ADD_LAYER_PATH", RAY_LAYER_PATH, 1);
#endif
#endif
  Check(volkInitialize(), "volkInitialize");
  if (!glfwInit()) {
    fprintf(stderr, "glfwInit failed\n");
    return EXIT_FAILURE;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1280, 720, "ray", NULL, NULL);
  uint32_t extension_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  if (!window || !extensions) {
    fprintf(stderr, "glfw window or vulkan surface unavailable\n");
    return EXIT_FAILURE;
  }

  VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo =
          &(VkApplicationInfo){
              .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
              .apiVersion = VK_API_VERSION_1_3,
          },
      .enabledExtensionCount = extension_count,
      .ppEnabledExtensionNames = extensions,
  };
#ifndef NDEBUG
  const char* layer = "VK_LAYER_KHRONOS_validation";
  instance_info.enabledLayerCount = 1;
  instance_info.ppEnabledLayerNames = &layer;
#endif
  VkInstance instance;
  Check(vkCreateInstance(&instance_info, NULL, &instance), "vkCreateInstance");
  volkLoadInstance(instance);

  VkSurfaceKHR surface;
  Check(glfwCreateWindowSurface(instance, window, NULL, &surface),
        "glfwCreateWindowSurface");

  uint32_t gpu_count = 1;
  VkPhysicalDevice gpu = VK_NULL_HANDLE;
  Check(vkEnumeratePhysicalDevices(instance, &gpu_count, &gpu),
        "vkEnumeratePhysicalDevices");
  if (gpu_count == 0) {
    fprintf(stderr, "no vulkan device\n");
    return EXIT_FAILURE;
  }
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(gpu, &properties);
  printf("%s\n", properties.deviceName);

  VkQueueFamilyProperties families[32];
  uint32_t family_count = 32;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, families);
  uint32_t family = UINT32_MAX;
  for (uint32_t i = 0; i < family_count && family == UINT32_MAX; ++i) {
    VkBool32 present = VK_FALSE;
    Check(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &present),
          "vkGetPhysicalDeviceSurfaceSupportKHR");
    if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
      family = i;
    }
  }
  if (family == UINT32_MAX) {
    fprintf(stderr, "no graphics queue with present support\n");
    return EXIT_FAILURE;
  }

  float priority = 1.0f;
  VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos =
          &(VkDeviceQueueCreateInfo){
              .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
              .queueFamilyIndex = family,
              .queueCount = 1,
              .pQueuePriorities = &priority,
          },
  };
  VkDevice device;
  Check(vkCreateDevice(gpu, &device_info, NULL, &device), "vkCreateDevice");
  volkLoadDevice(device);

  VkShaderModule vert =
      CreateShaderModule(device, kTriangleVert, sizeof(kTriangleVert));
  VkShaderModule frag =
      CreateShaderModule(device, kTriangleFrag, sizeof(kTriangleFrag));

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }

  vkDestroyShaderModule(device, frag, NULL);
  vkDestroyShaderModule(device, vert, NULL);
  vkDestroyDevice(device, NULL);
  vkDestroySurfaceKHR(instance, surface, NULL);
  vkDestroyInstance(instance, NULL);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
