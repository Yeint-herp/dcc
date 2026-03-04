#include <algorithm>
#include <cstring>
#include <util/si.hh>

namespace dcc::si
{
    StringInterner::StringInterner(std::size_t page_size) : m_page_size{std::max(page_size, std::size_t{256})}, m_slots(64) {}

    StringInterner::StringInterner(StringInterner&&) noexcept = default;
    StringInterner& StringInterner::operator=(StringInterner&&) noexcept = default;

    char* StringInterner::arena_alloc(std::size_t bytes)
    {
        if (!m_pages.empty())
        {
            auto& back = m_pages.back();
            if (back.used + bytes <= back.capacity)
            {
                char* ptr = back.data.get() + back.used;
                back.used += bytes;
                return ptr;
            }
        }

        std::size_t cap = std::max(m_page_size, bytes);
        auto buf = std::make_unique<char[]>(cap);
        char* ptr = buf.get();

        m_pages.push_back(Page{
            .data = std::move(buf),
            .capacity = cap,
            .used = bytes,
        });

        return ptr;
    }

    std::size_t StringInterner::arena_bytes() const noexcept
    {
        std::size_t total = 0;
        for (const auto& p : m_pages)
            total += p.capacity;

        return total;
    }

    std::size_t StringInterner::hash(std::string_view sv) noexcept
    {
        if constexpr (sizeof(std::size_t) == 8)
        {
            std::size_t h = 14695981039346656037ULL;
            for (char c : sv)
            {
                h ^= static_cast<unsigned char>(c);
                h *= 1099511628211ULL;
            }
            return h;
        }
        else
        {
            std::size_t h = 2166136261U;
            for (char c : sv)
            {
                h ^= static_cast<unsigned char>(c);
                h *= 16777619U;
            }
            return h;
        }
    }

    const StringInterner::Slot* StringInterner::find_slot(std::string_view text) const noexcept
    {
        if (m_slots.empty())
            return nullptr;

        std::size_t mask = m_slots.size() - 1;
        std::size_t idx = hash(text) & mask;

        for (;;)
        {
            const auto& slot = m_slots[idx];
            if (!slot.occupied)
                return nullptr;

            if (slot.key.size() == text.size() && slot.key == text)
                return &slot;

            idx = (idx + 1) & mask;
        }
    }

    void StringInterner::grow()
    {
        std::size_t new_cap = m_slots.empty() ? 64 : m_slots.size() * 2;
        std::vector<Slot> new_slots(new_cap);
        std::size_t mask = new_cap - 1;

        for (const auto& old : m_slots)
        {
            if (!old.occupied)
                continue;

            std::size_t idx = hash(old.key) & mask;
            while (new_slots[idx].occupied)
                idx = (idx + 1) & mask;

            new_slots[idx] = old;
        }

        m_slots = std::move(new_slots);
    }

    InternedString StringInterner::intern(std::string_view text)
    {
        if (auto* existing = find_slot(text))
            return InternedString{existing->key};

        if (static_cast<double>(m_size + 1) / static_cast<double>(m_slots.size()) > max_load)
            grow();

        char* storage = arena_alloc(text.size());
        std::memcpy(storage, text.data(), text.size());
        std::string_view canonical{storage, text.size()};

        std::size_t mask = m_slots.size() - 1;
        std::size_t idx = hash(text) & mask;
        while (m_slots[idx].occupied)
            idx = (idx + 1) & mask;

        m_slots[idx] = Slot{.key = canonical, .occupied = true};
        ++m_size;

        return InternedString{canonical};
    }

    InternedString StringInterner::lookup(std::string_view text) const noexcept
    {
        if (auto* slot = find_slot(text))
            return InternedString{slot->key};

        return {};
    }

} // namespace dcc::si
