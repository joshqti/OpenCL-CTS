//
// Copyright (c) 2023 The Khronos Group Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include "harness/typeWrappers.h"
#include "harness/extensionHelpers.h"
#include "harness/errorHelpers.h"
#include <android/hardware_buffer.h>
#include "debug_ahb.h"

struct ahb_format_table {
    AHardwareBuffer_Format aHardwareBufferFormat;
    cl_image_format clImageFormat;
    cl_mem_object_type clMemObjectType;
};

struct ahb_usage_table {
    AHardwareBuffer_UsageFlags usageFlags;
};

struct ahb_image_size_table {
    uint32_t width;
    uint32_t height;
};

ahb_image_size_table test_sizes[] = {
        {128,128}
};

ahb_usage_table test_usages[] = {
        {static_cast<AHardwareBuffer_UsageFlags>(AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                                                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE|
                                                 AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER)},
        {static_cast<AHardwareBuffer_UsageFlags>(AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)},
        {static_cast<AHardwareBuffer_UsageFlags>(AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER)},
};

ahb_format_table test_formats[] = {
        {
                AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT,
                {CL_RGBA, CL_HALF_FLOAT},
                CL_MEM_OBJECT_IMAGE2D
        },

        {
                AHARDWAREBUFFER_FORMAT_R16G16_UINT,
                {CL_RG, CL_UNSIGNED_INT16},
                CL_MEM_OBJECT_IMAGE2D
        },

        {
                AHARDWAREBUFFER_FORMAT_R16_UINT,
                {CL_R, CL_UNSIGNED_INT16},
                CL_MEM_OBJECT_IMAGE2D
        },

        {
                AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
                {CL_RGBA, CL_UNORM_INT8},
                CL_MEM_OBJECT_IMAGE2D
        },

        {
                AHARDWAREBUFFER_FORMAT_R8_UNORM,
                {CL_R, CL_UNORM_INT8},
                CL_MEM_OBJECT_IMAGE2D
        },
};

// Confirm that a signal followed by a wait will complete successfully
int test_images(cl_device_id deviceID, cl_context context,
                             cl_command_queue defaultQueue, int num_elements)
{
    cl_int err;

    if (!is_extension_available(deviceID, "cl_khr_external_memory_android_hardware_buffer"))
    {
        log_info("cl_khr_semaphore is not supported on this platform. "
                 "Skipping test.\n");
        return TEST_SKIPPED_ITSELF;
    }

    for(auto format : test_formats) {
        AHardwareBuffer_Desc aHardwareBufferDesc = {0};
        aHardwareBufferDesc.format = format.aHardwareBufferFormat;
        for(auto usage : test_usages) {
            aHardwareBufferDesc.usage = usage.usageFlags;
            for(auto resolution : test_sizes) {
                aHardwareBufferDesc.width = resolution.width;
                aHardwareBufferDesc.height = resolution.height;
                aHardwareBufferDesc.layers = 1;
                if(!AHardwareBuffer_isSupported(&aHardwareBufferDesc))
                {
                    char *usage_string = ahardwareBufferDecodeUsageFlagsToString(static_cast<AHardwareBuffer_UsageFlags>(aHardwareBufferDesc.usage));
                    log_info("Unsupported format %s:\n   Usage flags %s\n   Size (%u, %u, layers = %u)\n",
                             ahardwareBufferFormatToString(format.aHardwareBufferFormat),
                             usage_string,
                             aHardwareBufferDesc.width,
                             aHardwareBufferDesc.height,
                             aHardwareBufferDesc.layers);
                    delete [] usage_string;
                    continue;
                }

                AHardwareBuffer *aHardwareBuffer = nullptr;
                int ahb_result = AHardwareBuffer_allocate(&aHardwareBufferDesc, &aHardwareBuffer);
                if(ahb_result != 0) {
                    log_error("AHardwareBuffer_allocate failed with code %d\n", ahb_result);
                    return TEST_FAIL;
                }
                log_info("Testing %s\n", ahardwareBufferFormatToString(format.aHardwareBufferFormat));

                cl_mem_properties props[] = {
                        CL_EXTERNAL_MEMORY_HANDLE_AHB_KHR,
                        reinterpret_cast<cl_mem_properties>(aHardwareBuffer),
                        0
                };

                cl_mem image = clCreateImageWithProperties(context, props, CL_MEM_READ_WRITE, nullptr, nullptr, nullptr, &err);
                test_error(err, "Failed to create CL image from AHardwareBuffer");

                cl_image_format imageFormat = {0};
                err = clGetImageInfo(image, CL_IMAGE_FORMAT, sizeof(cl_image_format), &imageFormat, nullptr);
                test_error(err, "Failed to query image format");

                if(imageFormat.image_channel_order != format.clImageFormat.image_channel_order) {
                    log_error("Expected channel order %d, got %d\n", format.clImageFormat.image_channel_order, imageFormat.image_channel_order);
                    return TEST_FAIL;
                }

                if(imageFormat.image_channel_data_type != format.clImageFormat.image_channel_data_type) {
                    log_error("Expected image_channel_data_type %d, got %d\n", format.clImageFormat.image_channel_data_type, imageFormat.image_channel_data_type);
                    return TEST_FAIL;
                }

                test_error(clReleaseMemObject(image), "Failed to release image");
                AHardwareBuffer_release(aHardwareBuffer);
                aHardwareBuffer = nullptr;
            }
        }
    }

    return TEST_PASS;
}

// Confirm that a signal followed by a wait will complete successfully
int test_blob(cl_device_id deviceID, cl_context context,
                             cl_command_queue defaultQueue, int num_elements)
{
    cl_int err;

    if (!is_extension_available(deviceID, "cl_khr_external_memory_android_hardware_buffer"))
    {
        log_info("cl_khr_semaphore is not supported on this platform. "
                 "Skipping test.\n");
        return TEST_SKIPPED_ITSELF;
    }

    AHardwareBuffer_Desc aHardwareBufferDesc = {0};
    aHardwareBufferDesc.format = AHARDWAREBUFFER_FORMAT_BLOB;
    aHardwareBufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER;
        for(auto resolution : test_sizes) {
            aHardwareBufferDesc.width = resolution.width * resolution.height;
            aHardwareBufferDesc.height = 1;
            aHardwareBufferDesc.layers = 1;
            aHardwareBufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER;

            if(!AHardwareBuffer_isSupported(&aHardwareBufferDesc))
            {
                char *usage_string = ahardwareBufferDecodeUsageFlagsToString(static_cast<AHardwareBuffer_UsageFlags>(aHardwareBufferDesc.usage));
                log_info("Unsupported format %s, usage flags %s\n",
                         ahardwareBufferFormatToString(static_cast<AHardwareBuffer_Format>(aHardwareBufferDesc.format)),
                         usage_string);
                delete [] usage_string;
                continue;
            }

            AHardwareBuffer *aHardwareBuffer = nullptr;
            int ahb_result = AHardwareBuffer_allocate(&aHardwareBufferDesc, &aHardwareBuffer);
            if(ahb_result != 0) {
                log_error("AHardwareBuffer_allocate failed with code %d\n", ahb_result);
                return TEST_FAIL;
            }
            log_info("Testing %s\n", ahardwareBufferFormatToString(static_cast<AHardwareBuffer_Format>(aHardwareBufferDesc.format)));

            cl_mem_properties props[] = {
                    CL_EXTERNAL_MEMORY_HANDLE_AHB_KHR,
                    reinterpret_cast<cl_mem_properties>(aHardwareBuffer),
                    0
            };

            cl_mem buffer = clCreateBufferWithProperties(context, props, CL_MEM_READ_WRITE, 0, nullptr, &err);
            test_error(err, "Failed to create CL buffer from AHardwareBuffer");

            test_error(clReleaseMemObject(buffer), "Failed to release buffer");
            AHardwareBuffer_release(aHardwareBuffer);
            aHardwareBuffer = nullptr;
    }

    return TEST_PASS;
}

