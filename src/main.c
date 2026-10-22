#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <volk.h>

enum { kMaxImages = 8 };

typedef struct Swapchain {
  VkSwapchainKHR handle;
  VkExtent2D extent;
  uint32_t image_count;
  VkImage images[kMaxImages];
  VkImageView views[kMaxImages];
  VkFence presented[kMaxImages];
} Swapchain;

static const uint32_t kTriangleVert[] = {
#include "triangle.vert.inc"
};

static const uint32_t kTriangleFrag[] = {
#include "triangle.frag.inc"
};

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static void Check(VkResult result, const char* call) {
  if (result < 0) {
    fprintf(stderr, "%s failed: %d\n", call, result);
    exit(EXIT_FAILURE);
  }
}

static void OnFramebufferSize(GLFWwindow* window, int width, int height) {
  *(int*)glfwGetWindowUserPointer(window) = 1;
}

static int Supports(VkPhysicalDevice gpu, const char* name) {
  static VkExtensionProperties extensions[512];
  uint32_t count = 512;
  if (gpu == VK_NULL_HANDLE) {
    Check(vkEnumerateInstanceExtensionProperties(NULL, &count, extensions),
          "vkEnumerateInstanceExtensionProperties");
  } else {
    Check(vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, extensions),
          "vkEnumerateDeviceExtensionProperties");
  }
  for (uint32_t i = 0; i < count; ++i) {
    if (strcmp(extensions[i].extensionName, name) == 0) {
      return 1;
    }
  }
  return 0;
}

static VkPipeline CreatePipeline(VkDevice device, VkPipelineLayout layout,
                                 VkFormat format) {
  VkPipelineShaderStageCreateInfo stages[] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext =
              &(VkShaderModuleCreateInfo){
                  .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                  .codeSize = sizeof(kTriangleVert),
                  .pCode = kTriangleVert,
              },
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext =
              &(VkShaderModuleCreateInfo){
                  .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                  .codeSize = sizeof(kTriangleFrag),
                  .pCode = kTriangleFrag,
              },
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pName = "main",
      },
  };
  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
                                     VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
  VkGraphicsPipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext =
          &(VkPipelineRenderingCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
              .colorAttachmentCount = 1,
              .pColorAttachmentFormats = &format,
          },
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState =
          &(VkPipelineVertexInputStateCreateInfo){
              .sType =
                  VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
          },
      .pInputAssemblyState =
          &(VkPipelineInputAssemblyStateCreateInfo){
              .sType =
                  VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
              .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          },
      .pViewportState =
          &(VkPipelineViewportStateCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          },
      .pRasterizationState =
          &(VkPipelineRasterizationStateCreateInfo){
              .sType =
                  VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
              .lineWidth = 1.0f,
          },
      .pMultisampleState =
          &(VkPipelineMultisampleStateCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
          },
      .pColorBlendState =
          &(VkPipelineColorBlendStateCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
              .attachmentCount = 1,
              .pAttachments =
                  &(VkPipelineColorBlendAttachmentState){
                      .colorWriteMask =
                          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                  },
          },
      .pDynamicState =
          &(VkPipelineDynamicStateCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
              .dynamicStateCount = 2,
              .pDynamicStates = dynamic_states,
          },
      .layout = layout,
  };
  VkPipeline pipeline;
  Check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
                                  &pipeline),
        "vkCreateGraphicsPipelines");
  return pipeline;
}

static void CreateSwapchain(VkPhysicalDevice gpu, VkDevice device,
                            VkSurfaceKHR surface, VkSurfaceFormatKHR format,
                            VkExtent2D framebuffer, VkSwapchainKHR old,
                            int present_fences, Swapchain* swapchain) {
  VkSurfaceCapabilitiesKHR capabilities;
  Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      image_count > capabilities.maxImageCount) {
    image_count = capabilities.maxImageCount;
  }
  *swapchain = (Swapchain){0};
  swapchain->extent = capabilities.currentExtent.width == UINT32_MAX
                          ? framebuffer
                          : capabilities.currentExtent;
  VkSwapchainCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = image_count,
      .imageFormat = format.format,
      .imageColorSpace = format.colorSpace,
      .imageExtent = swapchain->extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = old,
  };
  Check(vkCreateSwapchainKHR(device, &info, NULL, &swapchain->handle),
        "vkCreateSwapchainKHR");
  Check(vkGetSwapchainImagesKHR(device, swapchain->handle,
                                &swapchain->image_count, NULL),
        "vkGetSwapchainImagesKHR");
  if (swapchain->image_count > kMaxImages) {
    Fail("too many swapchain images");
  }
  Check(vkGetSwapchainImagesKHR(device, swapchain->handle,
                                &swapchain->image_count, swapchain->images),
        "vkGetSwapchainImagesKHR");
  for (uint32_t i = 0; i < swapchain->image_count; ++i) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = swapchain->images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format.format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    Check(vkCreateImageView(device, &view_info, NULL, &swapchain->views[i]),
          "vkCreateImageView");
    if (present_fences) {
      Check(vkCreateFence(device,
                          &(VkFenceCreateInfo){
                              .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                              .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                          },
                          NULL, &swapchain->presented[i]),
            "vkCreateFence");
    }
  }
}

static void DestroySwapchain(VkDevice device, Swapchain* swapchain) {
  if (swapchain->presented[0] != VK_NULL_HANDLE) {
    Check(vkWaitForFences(device, swapchain->image_count, swapchain->presented,
                          VK_TRUE, UINT64_MAX),
          "vkWaitForFences");
  }
  for (uint32_t i = 0; i < swapchain->image_count; ++i) {
    vkDestroyFence(device, swapchain->presented[i], NULL);
    vkDestroyImageView(device, swapchain->views[i], NULL);
  }
  vkDestroySwapchainKHR(device, swapchain->handle, NULL);
  *swapchain = (Swapchain){0};
}

static void Transition(VkCommandBuffer command_buffer, VkImage image,
                       VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                           ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                           : VK_ACCESS_2_NONE,
      .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                           ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                           : VK_ACCESS_2_NONE,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .levelCount = 1,
              .layerCount = 1,
          },
  };
  vkCmdPipelineBarrier2(command_buffer,
                        &(VkDependencyInfo){
                            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                            .imageMemoryBarrierCount = 1,
                            .pImageMemoryBarriers = &barrier,
                        });
}

int main(void) {
  Check(volkInitialize(), "volkInitialize");
  glfwInitVulkanLoader(vkGetInstanceProcAddr);
  if (!glfwInit()) {
    Fail("glfwInit failed");
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
  GLFWwindow* window = glfwCreateWindow(1280, 720, "ray", NULL, NULL);
  uint32_t extension_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
  if (!window || !extensions) {
    Fail("glfw window or vulkan surface unavailable");
  }
  int recreate = 0;
  glfwSetWindowUserPointer(window, &recreate);
  glfwSetFramebufferSizeCallback(window, OnFramebufferSize);

  const char* instance_extensions[16];
  if (extension_count > 14) {
    Fail("too many instance extensions");
  }
  for (uint32_t i = 0; i < extension_count; ++i) {
    instance_extensions[i] = extensions[i];
  }
  int maintenance1 =
      Supports(VK_NULL_HANDLE, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  if (maintenance1) {
    instance_extensions[extension_count++] =
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
    instance_extensions[extension_count++] =
        VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
  }

  VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo =
          &(VkApplicationInfo){
              .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
              .apiVersion = VK_API_VERSION_1_4,
          },
      .enabledExtensionCount = extension_count,
      .ppEnabledExtensionNames = instance_extensions,
  };
#ifndef NDEBUG
  const char* layer = "VK_LAYER_KHRONOS_validation";
  instance_info.enabledLayerCount = 1;
  instance_info.ppEnabledLayerNames = &layer;
#endif
  VkInstance instance;
  VkResult result = vkCreateInstance(&instance_info, NULL, &instance);
  if (result == VK_ERROR_LAYER_NOT_PRESENT) {
    fprintf(stderr, "validation layer not found\n");
    instance_info.enabledLayerCount = 0;
    result = vkCreateInstance(&instance_info, NULL, &instance);
  }
  Check(result, "vkCreateInstance");
  volkLoadInstance(instance);

  VkSurfaceKHR surface;
  Check(glfwCreateWindowSurface(instance, window, NULL, &surface),
        "glfwCreateWindowSurface");

  VkPhysicalDevice gpus[16];
  uint32_t gpu_count = 16;
  Check(vkEnumeratePhysicalDevices(instance, &gpu_count, gpus),
        "vkEnumeratePhysicalDevices");
  if (gpu_count == 0) {
    Fail("no vulkan device");
  }
  VkPhysicalDevice gpu = gpus[0];
  VkPhysicalDeviceProperties properties;
  for (uint32_t i = 0; i < gpu_count; ++i) {
    vkGetPhysicalDeviceProperties(gpus[i], &properties);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      gpu = gpus[i];
      break;
    }
  }
  vkGetPhysicalDeviceProperties(gpu, &properties);
  if (properties.apiVersion < VK_API_VERSION_1_4) {
    Fail("vulkan 1.4 required");
  }
  printf("%s\n", properties.deviceName);
  maintenance1 = maintenance1 &&
                 Supports(gpu, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);

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
    Fail("no graphics queue with present support");
  }

  float priority = 1.0f;
  const char* device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
  };
  VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext =
          &(VkPhysicalDeviceVulkan13Features){
              .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
              .pNext =
                  &(VkPhysicalDeviceVulkan14Features){
                      .sType =
                          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
                      .pNext =
                          maintenance1
                              ? &(VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR){
                                    .sType =
                                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
                                    .swapchainMaintenance1 = VK_TRUE,
                                }
                              : NULL,
                      .maintenance5 = VK_TRUE,
                  },
              .synchronization2 = VK_TRUE,
              .dynamicRendering = VK_TRUE,
          },
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos =
          &(VkDeviceQueueCreateInfo){
              .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
              .queueFamilyIndex = family,
              .queueCount = 1,
              .pQueuePriorities = &priority,
          },
      .enabledExtensionCount = maintenance1 ? 2 : 1,
      .ppEnabledExtensionNames = device_extensions,
  };
  VkDevice device;
  Check(vkCreateDevice(gpu, &device_info, NULL, &device), "vkCreateDevice");
  volkLoadDevice(device);
  VkQueue queue;
  vkGetDeviceQueue(device, family, 0, &queue);

  VkSurfaceFormatKHR format;
  uint32_t format_count = 1;
  Check(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &format_count,
                                             &format),
        "vkGetPhysicalDeviceSurfaceFormatsKHR");
  int width;
  int height;
  glfwGetFramebufferSize(window, &width, &height);
  Swapchain swapchain;
  Swapchain retired = {0};
  CreateSwapchain(gpu, device, surface, format,
                  (VkExtent2D){(uint32_t)width, (uint32_t)height},
                  VK_NULL_HANDLE, maintenance1, &swapchain);

  VkPipelineLayout layout;
  Check(vkCreatePipelineLayout(
            device,
            &(VkPipelineLayoutCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            },
            NULL, &layout),
        "vkCreatePipelineLayout");
  VkPipeline pipeline = CreatePipeline(device, layout, format.format);

  VkCommandPool pool;
  Check(vkCreateCommandPool(
            device,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = family,
            },
            NULL, &pool),
        "vkCreateCommandPool");
  VkCommandBuffer command_buffer;
  Check(vkAllocateCommandBuffers(
            device,
            &(VkCommandBufferAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            },
            &command_buffer),
        "vkAllocateCommandBuffers");

  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkSemaphore acquired;
  Check(vkCreateSemaphore(device, &semaphore_info, NULL, &acquired),
        "vkCreateSemaphore");
  VkSemaphore rendered[kMaxImages];
  for (int i = 0; i < kMaxImages; ++i) {
    Check(vkCreateSemaphore(device, &semaphore_info, NULL, &rendered[i]),
          "vkCreateSemaphore");
  }
  VkFence fence;
  Check(vkCreateFence(device,
                      &(VkFenceCreateInfo){
                          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                      },
                      NULL, &fence),
        "vkCreateFence");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) {
      glfwWaitEvents();
      continue;
    }
    if (recreate) {
      if (!maintenance1) {
        Check(vkDeviceWaitIdle(device), "vkDeviceWaitIdle");
      }
      DestroySwapchain(device, &retired);
      retired = swapchain;
      CreateSwapchain(gpu, device, surface, format,
                      (VkExtent2D){(uint32_t)width, (uint32_t)height},
                      retired.handle, maintenance1, &swapchain);
      if (!maintenance1) {
        DestroySwapchain(device, &retired);
      }
      recreate = 0;
    }
    if (retired.handle != VK_NULL_HANDLE &&
        vkWaitForFences(device, retired.image_count, retired.presented, VK_TRUE,
                        0) == VK_SUCCESS) {
      DestroySwapchain(device, &retired);
    }

    Check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX),
          "vkWaitForFences");
    uint32_t index;
    result = vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX,
                                   acquired, VK_NULL_HANDLE, &index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate = 1;
      continue;
    }
    Check(result, "vkAcquireNextImageKHR");
    Check(vkResetFences(device, 1, &fence), "vkResetFences");

    Check(vkBeginCommandBuffer(
              command_buffer,
              &(VkCommandBufferBeginInfo){
                  .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
              }),
          "vkBeginCommandBuffer");
    Transition(command_buffer, swapchain.images[index],
               VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkCmdBeginRendering(
        command_buffer,
        &(VkRenderingInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = swapchain.extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments =
                &(VkRenderingAttachmentInfo){
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = swapchain.views[index],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                },
        });
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
    vkCmdSetViewportWithCount(command_buffer, 1,
                              &(VkViewport){
                                  .width = (float)swapchain.extent.width,
                                  .height = (float)swapchain.extent.height,
                                  .maxDepth = 1.0f,
                              });
    vkCmdSetScissorWithCount(command_buffer, 1,
                             &(VkRect2D){.extent = swapchain.extent});
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRendering(command_buffer);
    Transition(command_buffer, swapchain.images[index],
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    Check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos =
            &(VkSemaphoreSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = acquired,
                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            },
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos =
            &(VkCommandBufferSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = command_buffer,
            },
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos =
            &(VkSemaphoreSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = rendered[index],
                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            },
    };
    Check(vkQueueSubmit2(queue, 1, &submit_info, fence), "vkQueueSubmit2");
    if (maintenance1) {
      Check(vkWaitForFences(device, 1, &swapchain.presented[index], VK_TRUE,
                            UINT64_MAX),
            "vkWaitForFences");
      Check(vkResetFences(device, 1, &swapchain.presented[index]),
            "vkResetFences");
    }
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = maintenance1 ? &(VkSwapchainPresentFenceInfoKHR){
                                    .sType =
                                        VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
                                    .swapchainCount = 1,
                                    .pFences = &swapchain.presented[index],
                                }
                              : NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &rendered[index],
        .swapchainCount = 1,
        .pSwapchains = &swapchain.handle,
        .pImageIndices = &index,
    };
    result = vkQueuePresentKHR(queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      recreate = 1;
    } else {
      Check(result, "vkQueuePresentKHR");
    }
  }

  Check(vkDeviceWaitIdle(device), "vkDeviceWaitIdle");
  DestroySwapchain(device, &retired);
  DestroySwapchain(device, &swapchain);
  vkDestroyFence(device, fence, NULL);
  for (int i = 0; i < kMaxImages; ++i) {
    vkDestroySemaphore(device, rendered[i], NULL);
  }
  vkDestroySemaphore(device, acquired, NULL);
  vkDestroyCommandPool(device, pool, NULL);
  vkDestroyPipeline(device, pipeline, NULL);
  vkDestroyPipelineLayout(device, layout, NULL);
  vkDestroyDevice(device, NULL);
  vkDestroySurfaceKHR(instance, surface, NULL);
  vkDestroyInstance(instance, NULL);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
