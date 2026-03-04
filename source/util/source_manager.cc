#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <util/source_manager.hh>

namespace dcc::sm
{
    std::string_view to_string(Error e) noexcept
    {
        switch (e)
        {
            case Error::FileNotFound:
                return "file not found";
            case Error::PermissionDenied:
                return "permission denied";
            case Error::FileTooLarge:
                return "file too large";
            case Error::MmapFailed:
                return "mmap failed";
            case Error::InvalidUtf8:
                return "invalid UTF-8";
            case Error::OutOfRange:
                return "offset out of range";
        }
        return "unknown";
    }

    SourceFile::SourceFile(FileId id, std::filesystem::path path, void* mapping, std::size_t size) noexcept
        : m_id{id}, m_path{std::move(path)}, m_mapping{mapping}, m_size{size}
    {
    }

    SourceFile::SourceFile(SourceFile&& o) noexcept
        : m_id{o.m_id}, m_path{std::move(o.m_path)}, m_mapping{o.m_mapping}, m_size{o.m_size}, m_line_start{std::move(o.m_line_start)},
          m_line_index_built{o.m_line_index_built}
    {
        o.m_mapping = nullptr;
        o.m_size = 0;
    }

    SourceFile& SourceFile::operator=(SourceFile&& o) noexcept
    {
        if (this != &o)
        {
            close_mapping();

            m_id = o.m_id;
            m_path = std::move(o.m_path);
            m_mapping = o.m_mapping;
            m_size = o.m_size;
            m_line_start = std::move(o.m_line_start);
            m_line_index_built = o.m_line_index_built;
            o.m_mapping = nullptr;
            o.m_size = 0;
        }

        return *this;
    }

    SourceFile::~SourceFile()
    {
        close_mapping();
    }

    void SourceFile::close_mapping() noexcept
    {
        if (m_mapping && m_mapping != MAP_FAILED && m_size > 0)
            ::munmap(m_mapping, m_size);

        m_mapping = nullptr;
        m_size = 0;
    }

    std::expected<std::string_view, Error> SourceFile::text(Offset begin, Offset end) const noexcept
    {
        if (begin > end || end > static_cast<Offset>(m_size))
            return std::unexpected{Error::OutOfRange};

        return std::string_view{static_cast<const char*>(m_mapping) + begin, static_cast<std::size_t>(end - begin)};
    }

    std::expected<std::string_view, Error> SourceFile::text(SourceRange range) const noexcept
    {
        if (range.begin.fileId != m_id)
            return std::unexpected{Error::OutOfRange};

        return text(range.begin.offset, range.end.offset);
    }

    void SourceFile::ensure_line_index() const
    {
        if (m_line_index_built)
            return;

        m_line_start.clear();
        m_line_start.push_back(0);

        const char* data = static_cast<const char*>(m_mapping);
        for (std::size_t i = 0; i < m_size; ++i)
            if (data[i] == '\n' && i + 1 <= m_size)
                m_line_start.push_back(static_cast<Offset>(i + 1));

        m_line_index_built = true;
    }

    uint32_t SourceFile::line_count() const
    {
        ensure_line_index();
        return static_cast<uint32_t>(m_line_start.size());
    }

    std::expected<LineCol, Error> SourceFile::line_col(Offset offset) const
    {
        if (offset > static_cast<Offset>(m_size))
            return std::unexpected{Error::OutOfRange};

        ensure_line_index();

        auto it = std::ranges::upper_bound(m_line_start, offset);
        --it;

        const uint32_t line_idx = static_cast<uint32_t>(it - m_line_start.begin());
        const Offset line_start = *it;
        const uint32_t line_number = line_idx + 1;

        const char* data = static_cast<const char*>(m_mapping);
        std::string_view line_sv{data + line_start, offset - line_start};

        int visual_col = 0;
        uint32_t byte_col = 0;

        for (auto result : utf8::codepoints(line_sv))
        {
            if (!result)
                return std::unexpected{Error::InvalidUtf8};

            visual_col += utf8::codepoint_width(result->codepoint);
            byte_col += static_cast<uint32_t>(result->bytes_consumed);
        }

        return LineCol{
            .line = line_number,
            .column = static_cast<uint32_t>(visual_col) + 1,
            .byte_col = byte_col + 1,
        };
    }

    std::expected<std::string_view, Error> SourceFile::line_text(uint32_t line_number) const
    {
        ensure_line_index();

        if (line_number == 0 || line_number > static_cast<uint32_t>(m_line_start.size()))
            return std::unexpected{Error::OutOfRange};

        const Offset start = m_line_start[line_number - 1];
        const Offset next_start = (line_number < m_line_start.size()) ? m_line_start[line_number] : static_cast<Offset>(m_size) + 1;

        Offset end = next_start - 1;
        if (end > static_cast<Offset>(m_size))
            end = static_cast<Offset>(m_size);

        const char* data = static_cast<const char*>(m_mapping);
        if (end > start && data[end - 1] == '\r')
            --end;

        return std::string_view{data + start, end - start};
    }

    utf8::CodepointRange SourceFile::codepoints(Offset begin, Offset end) const noexcept
    {
        const char* data = static_cast<const char*>(m_mapping);
        const std::size_t clamped_end = std::min<std::size_t>(end, m_size);
        const std::size_t clamped_begin = std::min<std::size_t>(begin, clamped_end);
        return utf8::codepoints(std::string_view{data + clamped_begin, clamped_end - clamped_begin});
    }

    namespace
    {
        [[nodiscard]] std::expected<std::pair<void*, std::size_t>, Error> mmap_file(const std::filesystem::path& path) noexcept
        {
            const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);

            if (fd == -1)
            {
                switch (errno)
                {
                    case ENOENT:
                        return std::unexpected{Error::FileNotFound};
                    case EACCES:
                        return std::unexpected{Error::PermissionDenied};
                    default:
                        return std::unexpected{Error::MmapFailed};
                }
            }

            struct ::stat st{};
            if (::fstat(fd, &st) == -1)
            {
                ::close(fd);
                return std::unexpected{Error::MmapFailed};
            }

            const auto file_size = static_cast<std::size_t>(st.st_size);

            constexpr std::size_t kMaxSize = std::size_t{1} << 32;
            if (file_size >= kMaxSize)
            {
                ::close(fd);
                return std::unexpected{Error::FileTooLarge};
            }

            if (file_size == 0)
            {
                ::close(fd);

                static const char empty = '\0';
                return std::pair{const_cast<char*>(&empty), std::size_t{}};
            }

            void* mapping = ::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
            ::close(fd);

            if (mapping == MAP_FAILED)
                return std::unexpected{Error::MmapFailed};

            ::madvise(mapping, file_size, MADV_SEQUENTIAL);

            return std::pair{mapping, file_size};
        }

    } // anonymous namespace

    std::expected<FileId, Error> SourceManager::load(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path, ec);
        if (ec)
        {
            switch (ec.value())
            {
                case ENOENT:
                    return std::unexpected{Error::FileNotFound};
                case EACCES:
                    return std::unexpected{Error::PermissionDenied};
                default:
                    return std::unexpected{Error::MmapFailed};
            }
        }

        for (const auto& f : m_files)
            if (f && f->path() == canonical)
                return f->id();

        auto result = mmap_file(canonical);
        if (!result)
            return std::unexpected{result.error()};

        auto [mapping, size] = *result;

        const FileId id = next_id();
        m_files.push_back(std::make_unique<SourceFile>(id, std::move(canonical), mapping, size));

        if (size > 0)
            ::madvise(mapping, size, MADV_RANDOM);

        return id;
    }

    FileId SourceManager::add_synthetic(std::string name, std::string content)
    {
        const FileId id = next_id();

        auto& sc = m_synthetic_content.emplace_back(SyntheticContent{
            .id = id,
            .content = std::move(content),
        });

        void* ptr = sc.content.empty() ? nullptr : static_cast<void*>(sc.content.data());
        m_files.push_back(std::make_unique<SourceFile>(id, std::filesystem::path{std::move(name)}, ptr, sc.content.size()));

        return id;
    }

    const SourceFile* SourceManager::get(FileId id) const noexcept
    {
        const auto idx = static_cast<std::size_t>(id);
        if (idx >= m_files.size())
            return nullptr;

        return m_files[idx].get();
    }

    std::expected<LineCol, Error> SourceManager::line_col(Location loc) const
    {
        const SourceFile* f = get(loc.fileId);
        if (!f)
            return std::unexpected{Error::OutOfRange};

        return f->line_col(loc.offset);
    }

    std::expected<std::string_view, Error> SourceManager::text(SourceRange range) const
    {
        const SourceFile* f = get(range.begin.fileId);
        if (!f)
            return std::unexpected{Error::OutOfRange};

        return f->text(range);
    }

} // namespace dcc::sm
