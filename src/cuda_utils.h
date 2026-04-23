#ifndef TRTX_CUDA_UTILS_H_
#define TRTX_CUDA_UTILS_H_

#include <cuda_runtime_api.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

inline void ThrowCudaError(cudaError_t error_code, const char* expression, const char* file, int line)
{
    if (error_code == cudaSuccess) {
        return;
    }

    std::ostringstream oss;
    oss << "CUDA call failed: " << expression
        << " at " << file << ":" << line
        << " (" << static_cast<int>(error_code) << ": " << cudaGetErrorString(error_code) << ")";
    throw std::runtime_error(oss.str());
}

inline void LogCudaError(cudaError_t error_code, const char* expression, const char* file, int line) noexcept
{
    if (error_code == cudaSuccess) {
        return;
    }

    std::cerr << "CUDA cleanup warning: " << expression
              << " at " << file << ":" << line
              << " (" << static_cast<int>(error_code) << ": " << cudaGetErrorString(error_code) << ")"
              << std::endl;
}

#ifndef CUDA_CHECK
#define CUDA_CHECK(callstr) ThrowCudaError((callstr), #callstr, __FILE__, __LINE__)
#endif  // CUDA_CHECK

#ifndef CUDA_CHECK_NOEXCEPT
#define CUDA_CHECK_NOEXCEPT(callstr) LogCudaError((callstr), #callstr, __FILE__, __LINE__)
#endif  // CUDA_CHECK_NOEXCEPT

#endif  // TRTX_CUDA_UTILS_H_
