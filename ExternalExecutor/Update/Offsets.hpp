#pragma once
#include <cstdint>

namespace Offsets {
    constexpr uintptr_t FakeDataModelPointer = 0x7D03628;
    constexpr uintptr_t FakeDataModelToDataModel = 0x1C0;
    constexpr uintptr_t TaskSchedulerPointer = 0x7E1CB88;
    constexpr uintptr_t TaskSchedulerMaxFPS = 0x1B0;
    
    constexpr uintptr_t Name = 0xB0;
    constexpr uintptr_t Children = 0x70;
    constexpr uintptr_t ChildrenEnd = 0x8;
    constexpr uintptr_t ClassDescriptor = 0x18;
    constexpr uintptr_t ClassDescriptorToClassName = 0x8;
    constexpr uintptr_t Value = 0xD0;
    
    constexpr uintptr_t LocalScriptByteCode = 0x1A8;
    constexpr uintptr_t ModuleScriptByteCode = 0x150;
    constexpr uintptr_t EnableLoadModule = 0x679D8F8;
}