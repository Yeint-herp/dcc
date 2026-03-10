#ifndef DCC_UTIL_SOURCE_MANAGER_HH
#define DCC_UTIL_SOURCE_MANAGER_HH

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <util/utf8.hh>
#include <vector>

namespace dcc::sm
{
    enum class Error : uint8_t
    {
        FileNotFound,
        PermissionDenied,
        FileTooLarge,
        MmapFailed,
        InvalidUtf8,
        OutOfRange,
    };
    [[nodiscard]] std::string_view to_string(Error e) noexcept;

    enum class FileId : uint32_t
    {
        Invalid = 0
    };
    using Offset = uint32_t;

    struct Location
    {
        FileId fileId{FileId::Invalid};
        Offset offset{};

        [[nodiscard]] constexpr bool valid() const noexcept { return fileId != FileId::Invalid; }
        [[nodiscard]] constexpr bool operator==(const Location&) const noexcept = default;
    };

    struct SourceRange
    {
        Location begin;
        Location end;

        [[nodiscard]] constexpr bool valid() const noexcept { return begin.valid() && end.fileId == begin.fileId && end.offset >= begin.offset; }
        [[nodiscard]] constexpr Offset byte_length() const noexcept { return end.offset - begin.offset; }
    };

    struct LineCol
    {
        uint32_t line;
        uint32_t column;
        uint32_t byte_col;
    };

    class SourceFile
    {
    public:
        explicit SourceFile(FileId id, std::filesystem::path path, void* mapping, std::size_t size, bool is_mmaped = true) noexcept;

        SourceFile(const SourceFile&) = delete;
        SourceFile& operator=(const SourceFile&) = delete;

        SourceFile(SourceFile&&) noexcept;
        SourceFile& operator=(SourceFile&&) noexcept;
        ~SourceFile();

        [[nodiscard]] FileId id() const noexcept { return m_id; }
        [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

        [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return {static_cast<const std::byte*>(m_mapping), m_size}; }

        [[nodiscard]] std::string_view text() const noexcept { return {static_cast<const char*>(m_mapping), m_size}; }

        [[nodiscard]] std::expected<std::string_view, Error> text(Offset begin, Offset end) const noexcept;
        [[nodiscard]] std::expected<std::string_view, Error> text(SourceRange range) const noexcept;

        [[nodiscard]] std::size_t size() const noexcept { return m_size; }
        [[nodiscard]] std::expected<LineCol, Error> line_col(Offset offset) const;

        [[nodiscard]] std::expected<std::string_view, Error> line_text(uint32_t line_number) const;
        [[nodiscard]] uint32_t line_count() const;

        [[nodiscard]] utf8::CodepointRange codepoints() const noexcept { return utf8::codepoints(text()); }
        [[nodiscard]] utf8::CodepointRange codepoints(Offset begin, Offset end) const noexcept;

    private:
        void close_mapping() noexcept;

        FileId m_id;
        std::filesystem::path m_path;
        void* m_mapping{nullptr};
        std::size_t m_size{};
        bool m_is_mmaped{false};

        mutable std::vector<Offset> m_line_start;
        mutable bool m_line_index_built{false};

        void ensure_line_index() const;
    };

    class SourceManager
    {
    public:
        SourceManager() = default;
        ~SourceManager() = default;

        SourceManager(const SourceManager&) = delete;
        SourceManager& operator=(const SourceManager&) = delete;
        SourceManager(SourceManager&&) = default;
        SourceManager& operator=(SourceManager&&) = default;

        [[nodiscard]] std::expected<FileId, Error> load(const std::filesystem::path& path);

        [[nodiscard]] FileId add_synthetic(std::string name, std::string content);

        [[nodiscard]] const SourceFile* get(FileId id) const noexcept;
        [[nodiscard]] const SourceFile* get(Location loc) const noexcept { return get(loc.fileId); }

        [[nodiscard]] constexpr Location location(FileId id, Offset offset) const noexcept { return Location{id, offset}; }

        [[nodiscard]] std::expected<LineCol, Error> line_col(Location loc) const;
        [[nodiscard]] std::expected<std::string_view, Error> text(SourceRange range) const;

        [[nodiscard]] std::size_t file_count() const noexcept { return m_files.size(); }

        template <typename Fn> void for_each_file(Fn&& fn) const
        {
            for (const auto& f : m_files)
                fn(*f);
        }

    private:
        std::vector<std::unique_ptr<SourceFile>> m_files;

        struct SyntheticContent
        {
            FileId id;
            std::string content;
        };
        std::vector<SyntheticContent> m_synthetic_content;

        [[nodiscard]] FileId next_id() const noexcept { return static_cast<FileId>(m_files.size()); }
    };

} // namespace dcc::sm

#endif /* DCC_UTIL_SOURCE_MANAGER_HH */
