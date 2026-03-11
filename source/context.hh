#ifndef DCC_CONTEXT_HH
#define DCC_CONTEXT_HH

#include <cstdint>
#include <filesystem>
#include <string>

namespace dcc::ctx
{
    enum class OptimizationLevel : uint8_t
    {
        None = 0,
        Debug,
        Release,
        ReleaseSize
    };

    enum class CPUFeature : uint32_t
    {
        None = 0,
        SSE = 1 << 0,
        SSE2 = 1 << 1,
        AVX = 1 << 2,
        AVX2 = 1 << 3,
    };

    inline CPUFeature operator|(CPUFeature a, CPUFeature b)
    {
        return static_cast<CPUFeature>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline CPUFeature operator&(CPUFeature a, CPUFeature b)
    {
        return static_cast<CPUFeature>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline CPUFeature operator~(CPUFeature a)
    {
        return static_cast<CPUFeature>(~static_cast<uint32_t>(a));
    }

    inline CPUFeature& operator|=(CPUFeature& a, CPUFeature b)
    {
        a = a | b;
        return a;
    }

    inline CPUFeature& operator&=(CPUFeature& a, CPUFeature b)
    {
        a = a & b;
        return a;
    }

    enum class FloatModel : uint8_t
    {
        IEEE_754,
        FastMath,
    };

    enum class MemoryRelocationModel : uint8_t
    {
        Static,
        Dynamic,
        PIC
    };

    struct Context
    {
        OptimizationLevel level{OptimizationLevel::None};
        std::string cpu_architecture;
        CPUFeature cpu_features{CPUFeature::None};
        FloatModel float_model{FloatModel::IEEE_754};
        MemoryRelocationModel memory_relocation_model{MemoryRelocationModel::Static};
        int warning_strictness{0};

        bool enable_extended_library{false};
        std::filesystem::path extended_library_path{};

        bool has_feature(CPUFeature feature) const { return (cpu_features & feature) == feature; }

        void add_feature(CPUFeature feature) { cpu_features |= feature; }
        void remove_feature(CPUFeature feature) { cpu_features &= ~feature; }
    };

}; // namespace dcc::ctx

#endif /* DCC_CONTEXT_HH */
