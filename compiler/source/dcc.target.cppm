export module dcc.target;

import std;

export namespace dcc::target
{
    enum class Arch : std::uint8_t
    {
        X86_64,
        X86,
    };

    enum class Os : std::uint8_t
    {
        Linux,
        Windows,
        Freestanding,
    };

    enum class ObjectFormat : std::uint8_t
    {
        Elf,
        Coff,
    };

    struct Layout
    {
        std::uint64_t size{};
        std::uint64_t align{};
    };

    struct TargetConfig
    {
        std::string triple;
        Arch arch{Arch::X86_64};
        Os os{Os::Linux};
        ObjectFormat object_format{ObjectFormat::Elf};
        std::uint8_t pointer_bits{64};
        std::uint8_t pointer_align{8};
        bool little_endian{true};

        [[nodiscard]] Layout int_layout(std::uint8_t bits) const { return Layout{static_cast<std::uint64_t>(bits / 8), static_cast<std::uint64_t>(bits / 8)}; }

        [[nodiscard]] Layout float_layout(std::uint8_t bits) const
        {
            return Layout{static_cast<std::uint64_t>(bits / 8), static_cast<std::uint64_t>(bits / 8)};
        }

        [[nodiscard]] Layout pointer_layout() const { return Layout{static_cast<std::uint64_t>(pointer_bits / 8), static_cast<std::uint64_t>(pointer_align)}; }

        [[nodiscard]] Layout slice_layout() const
        {
            if (pointer_bits == 64)
                return Layout{16, 8};
            else
                return Layout{8, 4};
        }

        [[nodiscard]] static TargetConfig host_default()
        {
            TargetConfig cfg;
            cfg.triple = "x86_64-elf";
            cfg.arch = Arch::X86_64;
            cfg.os = Os::Linux;
            cfg.object_format = ObjectFormat::Elf;
            cfg.pointer_bits = 64;
            cfg.pointer_align = 8;
            cfg.little_endian = true;
            return cfg;
        }

        [[nodiscard]] static std::optional<TargetConfig> parse_triple(std::string_view triple)
        {
            TargetConfig cfg;
            cfg.triple = std::string{triple};

            if (triple == "x86_64-elf")
            {
                cfg.arch = Arch::X86_64;
                cfg.os = Os::Linux;
                cfg.object_format = ObjectFormat::Elf;
                cfg.pointer_bits = 64;
                cfg.pointer_align = 8;
                cfg.little_endian = true;
                return cfg;
            }

            if (triple == "x86-elf")
            {
                cfg.arch = Arch::X86;
                cfg.os = Os::Linux;
                cfg.object_format = ObjectFormat::Elf;
                cfg.pointer_bits = 32;
                cfg.pointer_align = 4;
                cfg.little_endian = true;
                return cfg;
            }

            if (triple == "x86_64-coff")
            {
                cfg.arch = Arch::X86_64;
                cfg.os = Os::Windows;
                cfg.object_format = ObjectFormat::Coff;
                cfg.pointer_bits = 64;
                cfg.pointer_align = 8;
                cfg.little_endian = true;
                return cfg;
            }

            if (triple == "x86-coff")
            {
                cfg.arch = Arch::X86;
                cfg.os = Os::Windows;
                cfg.object_format = ObjectFormat::Coff;
                cfg.pointer_bits = 32;
                cfg.pointer_align = 4;
                cfg.little_endian = true;
                return cfg;
            }

            return std::nullopt;
        }
    };

} // namespace dcc::target
