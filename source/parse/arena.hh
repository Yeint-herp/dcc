#ifndef DCC_PARSE_ARENA_HH
#define DCC_PARSE_ARENA_HH

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

namespace dcc::parse
{
    class AstArena
    {
    public:
        explicit AstArena(std::size_t page_size = 64 * 1024) : m_page_size{page_size} {}

        ~AstArena()
        {
            for (auto it = m_dtors.rbegin(); it != m_dtors.rend(); ++it)
                (*it)();
        }

        AstArena(const AstArena&) = delete;
        AstArena& operator=(const AstArena&) = delete;
        AstArena(AstArena&&) = default;
        AstArena& operator=(AstArena&&) = default;

        template <typename T, typename... Args> T* create(Args&&... args)
        {
            void* mem = allocate(sizeof(T), alignof(T));
            auto* obj = new (mem) T(std::forward<Args>(args)...);
            if constexpr (!std::is_trivially_destructible_v<T>)
                m_dtors.push_back([obj]() { obj->~T(); });

            return obj;
        }

        template <typename T> T* alloc_array(std::size_t n)
        {
            if (n == 0)
                return nullptr;

            void* mem = allocate(sizeof(T) * n, alignof(T));
            auto* arr = static_cast<T*>(mem);
            for (std::size_t i = 0; i < n; ++i)
                new (&arr[i]) T{};

            return arr;
        }

        template <typename T> std::span<T> to_span(const std::vector<T>& vec)
        {
            if (vec.empty())
                return {};

            void* mem = allocate(sizeof(T) * vec.size(), alignof(T));
            auto* arr = static_cast<T*>(mem);
            for (std::size_t i = 0; i < vec.size(); ++i)
                new (&arr[i]) T(vec[i]);

            return {arr, vec.size()};
        }

        template <typename T> std::span<const T> to_const_span(const std::vector<T>& vec)
        {
            auto s = to_span(vec);
            return {s.data(), s.size()};
        }

    private:
        struct Page
        {
            std::unique_ptr<std::byte[]> data;
            std::size_t capacity;
            std::size_t used{};
        };

        std::vector<Page> m_pages;
        std::size_t m_page_size;
        std::vector<std::function<void()>> m_dtors;

        void* allocate(std::size_t size, std::size_t align)
        {
            if (!m_pages.empty())
            {
                auto& cur = m_pages.back();
                std::size_t aligned = (cur.used + align - 1) & ~(align - 1);

                if (aligned + size <= cur.capacity)
                {
                    cur.used = aligned + size;
                    return cur.data.get() + aligned;
                }
            }

            std::size_t cap = (size + align > m_page_size) ? size + align : m_page_size;
            auto& page = m_pages.emplace_back(Page{std::make_unique<std::byte[]>(cap), cap, 0});

            std::size_t aligned = (page.used + align - 1) & ~(align - 1);
            page.used = aligned + size;

            return page.data.get() + aligned;
        }
    };

} // namespace dcc::parse

#endif /* DCC_PARSE_ARENA_HH */
