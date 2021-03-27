#include "stdafx.h"
#pragma hdrstop

#include "cpuid.h"

#include <array>
#include <bitset>
#include <memory>

#include <SDL_cpuinfo.h>

#ifdef XR_PLATFORM_WINDOWS
u32 countSetBits(ULONG_PTR bitMask)
{
    u32 LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
    u32 bitSetCount = 0;
    ULONG_PTR bitTest = static_cast<ULONG_PTR>(1) << LSHIFT;
    u32 i;

    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest) ? 1 : 0);
        bitTest /= 2;
    }

    return bitSetCount;
}
#else
#include <thread>
#endif

#ifdef _EDITOR
unsgined int query_processor_info(processor_info* pinfo)
{
    ZeroMemory(pinfo, sizeof(processor_info));

    pinfo->feature = static_cast<u32>(CpuFeature::Mmx) | static_cast<u32>(CpuFeature::Sse);
    return pinfo->feature;
}
#else
bool query_processor_info(processor_info* pinfo)
{
    ZeroMemory(pinfo, sizeof(processor_info));

    pinfo->features.set(static_cast<u32>(CpuFeature::MMX), SDL_HasMMX());
    pinfo->features.set(static_cast<u32>(CpuFeature::_3DNow), SDL_Has3DNow());
    pinfo->features.set(static_cast<u32>(CpuFeature::AltiVec), SDL_HasAltiVec());
    pinfo->features.set(static_cast<u32>(CpuFeature::SSE), SDL_HasSSE());
    pinfo->features.set(static_cast<u32>(CpuFeature::SSE2), SDL_HasSSE2());
    pinfo->features.set(static_cast<u32>(CpuFeature::SSE3), SDL_HasSSE3());
    pinfo->features.set(static_cast<u32>(CpuFeature::SSE41), SDL_HasSSE41());
    pinfo->features.set(static_cast<u32>(CpuFeature::SSE42), SDL_HasSSE42());
    pinfo->features.set(static_cast<u32>(CpuFeature::AVX), SDL_HasAVX());
    pinfo->features.set(static_cast<u32>(CpuFeature::AVX2), SDL_HasAVX2());

    // Calculate available processors
#ifdef XR_PLATFORM_WINDOWS
    ULONG_PTR pa_mask_save, sa_mask_stub = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &pa_mask_save, &sa_mask_stub);
#elif defined(XR_PLATFORM_LINUX) || defined(XR_PLATFORM_FREEBSD)
    u32 pa_mask_save = 0;
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &my_set);
    pa_mask_save = CPU_COUNT(&my_set);
#else
#pragma TODO("No function to obtain process affinity")
    u32 pa_mask_save = 0;
#endif // XR_PLATFORM_WINDOWS

    u32 processorCoreCount = 0;
    u32 logicalProcessorCount = 0;

#ifdef XR_PLATFORM_WINDOWS
    DWORD returnedLength = 0;
    GetLogicalProcessorInformation(nullptr, &returnedLength);

    auto* buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)xr_alloca(returnedLength);
    GetLogicalProcessorInformation(buffer, &returnedLength);

    u32 byteOffset = 0;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnedLength)
    {
        switch (buffer->Relationship)
        {
            case RelationProcessorCore:
                processorCoreCount++;

                // A hyperthreaded core supplies more than one logical processor.
                logicalProcessorCount += countSetBits(buffer->ProcessorMask);
                break;

            default:
                break;
        }

        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        buffer++;
    }
#elif defined(XR_PLATFORM_LINUX) || defined(XR_PLATFORM_FREEBSD)
    processorCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
    logicalProcessorCount = std::thread::hardware_concurrency();
#else
#pragma TODO("No function to obtain processor's core count")
    logicalProcessorCount = std::thread::hardware_concurrency();
    processorCoreCount = logicalProcessorCount;
#endif

    if (logicalProcessorCount != processorCoreCount)
        pinfo->features.set(static_cast<u32>(CpuFeature::HyperThreading), true);

    // All logical processors
    pinfo->n_threads = logicalProcessorCount;
    pinfo->affinity_mask = pa_mask_save;
    pinfo->n_cores = processorCoreCount;

    return pinfo->features.get() != 0;
}
#endif
