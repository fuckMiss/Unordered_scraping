#pragma once

#include <cuda_runtime.h>
#include <cstdint>
#include <mutex>
#include <opencv2/opencv.hpp>

void cuda_preprocess_init(int max_image_size);
void cuda_preprocess_destroy() noexcept;
void cuda_preprocess(uint8_t* src, int src_width, int src_height,
    float* dst, int dst_width, int dst_height,
    cudaStream_t stream);
std::mutex& cuda_preprocess_io_mutex();
