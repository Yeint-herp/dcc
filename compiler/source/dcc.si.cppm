export module dcc.si;

import std;

export namespace dcc::si
{
    class string_interner
    {
    public:
        string_interner() : string_interner(initial_capacity) {}

        explicit string_interner(std::uint32_t initial_cap)
        {
            auto cap = std::bit_ceil(std::max(initial_cap, std::uint32_t{16}));
            m_resource = std::make_unique<std::pmr::monotonic_buffer_resource>(chunk_size);
            grow(cap);
        }

        ~string_interner() {}

        string_interner(string_interner&& other) noexcept
            : m_slots(std::move(other.m_slots)), m_resource(std::move(other.m_resource)), m_capacity(other.m_capacity), m_size(other.m_size),
              m_occupied_plus_tombstones(other.m_occupied_plus_tombstones), m_arena_bytes(other.m_arena_bytes)
        {
            other.m_capacity = 0;
            other.m_size = 0;
            other.m_occupied_plus_tombstones = 0;
            other.m_arena_bytes = 0;
        }

        string_interner& operator=(string_interner&& other) noexcept
        {
            if (this != &other)
            {
                m_slots = std::move(other.m_slots);
                m_resource = std::move(other.m_resource);
                m_capacity = other.m_capacity;
                m_size = other.m_size;
                m_occupied_plus_tombstones = other.m_occupied_plus_tombstones;
                m_arena_bytes = other.m_arena_bytes;

                other.m_capacity = 0;
                other.m_size = 0;
                other.m_occupied_plus_tombstones = 0;
                other.m_arena_bytes = 0;
            }

            return *this;
        }

        string_interner(string_interner const&) = delete;
        string_interner& operator=(string_interner const&) = delete;

        [[nodiscard]] auto intern(std::string_view str) -> std::string_view
        {
            if (str.empty()) [[unlikely]]
                return {};

            auto const hash = fnv1a(str);
            auto const mask = m_capacity - 1;
            auto idx = static_cast<std::uint32_t>(hash & mask);

            std::uint32_t first_tombstone = tombstone_sentinel;

            for (;;)
            {
                auto const& s = m_slots[idx];

                if (s.state == slot_state::empty)
                {
                    auto insert_idx = (first_tombstone != tombstone_sentinel) ? first_tombstone : idx;
                    return insert_at(insert_idx, hash, str);
                }

                if (s.state == slot_state::tombstone)
                {
                    if (first_tombstone == tombstone_sentinel)
                        first_tombstone = idx;
                }
                else if (s.hash == hash && content_eq(s, str))
                    return resolve(s);

                idx = (idx + 1) & mask;
            }
        }

        [[nodiscard]] auto find(std::string_view str) const -> std::string_view
        {
            if (str.empty())
                return {};

            auto const hash = fnv1a(str);
            auto const mask = m_capacity - 1;
            auto idx = static_cast<std::uint32_t>(hash & mask);

            for (;;)
            {
                auto const& s = m_slots[idx];

                if (s.state == slot_state::empty)
                    return {};

                if (s.state == slot_state::occupied && s.hash == hash && content_eq(s, str))
                    return resolve(s);

                idx = (idx + 1) & mask;
            }
        }

        [[nodiscard]] auto contains(std::string_view str) const -> bool
        {
            if (str.empty())
                return false;

            auto const hash = fnv1a(str);
            auto const mask = m_capacity - 1;
            auto idx = static_cast<std::uint32_t>(hash & mask);

            for (;;)
            {
                auto const& s = m_slots[idx];

                if (s.state == slot_state::empty)
                    return false;

                if (s.state == slot_state::occupied && s.hash == hash && content_eq(s, str))
                    return true;

                idx = (idx + 1) & mask;
            }
        }

        [[nodiscard]] auto size() const noexcept -> std::uint32_t { return m_size; }
        [[nodiscard]] auto capacity() const noexcept -> std::uint32_t { return m_capacity; }
        [[nodiscard]] auto arena_bytes() const noexcept -> std::size_t { return m_arena_bytes; }
        [[nodiscard]] auto empty() const noexcept -> bool { return m_size == 0; }

    private:
        enum class slot_state : std::uint8_t
        {
            empty = 0,
            occupied = 1,
            tombstone = 2,
        };

        struct alignas(16) slot
        {
            std::uint64_t hash{};
            char const* data{};
            std::uint32_t length{};
            slot_state state{slot_state::empty};
        };

        static constexpr std::uint32_t initial_capacity = 256;
        static constexpr std::uint32_t chunk_size = 4096;
        static constexpr std::uint32_t tombstone_sentinel = std::numeric_limits<std::uint32_t>::max();
        static constexpr double max_load_factor = 0.75;

        static constexpr std::uint64_t fnv_offset_basis = 0xcbf29ce484222325ULL;
        static constexpr std::uint64_t fnv_prime = 0x00000100000001b3ULL;

        std::vector<slot> m_slots;
        std::unique_ptr<std::pmr::monotonic_buffer_resource> m_resource;
        std::uint32_t m_capacity = 0;
        std::uint32_t m_size = 0;
        std::uint32_t m_occupied_plus_tombstones = 0;
        std::size_t m_arena_bytes = 0;

        [[nodiscard]] static constexpr auto fnv1a(std::string_view s) noexcept -> std::uint64_t
        {
            auto hash = fnv_offset_basis;
            for (auto c : s)
            {
                hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
                hash *= fnv_prime;
            }

            return hash;
        }

        [[nodiscard]] static auto resolve(slot const& s) -> std::string_view { return {s.data, s.length}; }

        [[nodiscard]] static auto content_eq(slot const& s, std::string_view str) -> bool
        {
            if (s.length != str.size())
                return false;

            return std::memcmp(s.data, str.data(), str.size()) == 0;
        }

        [[nodiscard]] auto arena_store(std::string_view str) -> char const*
        {
            auto const len = str.size();
            auto* p = static_cast<char*>(m_resource->allocate(len, alignof(char)));
            std::memcpy(p, str.data(), len);
            m_arena_bytes += len;
            return p;
        }

        auto insert_at(std::uint32_t idx, std::uint64_t hash, std::string_view str) -> std::string_view
        {
            const auto *const data = arena_store(str);
            auto const length = static_cast<std::uint32_t>(str.size());

            bool was_tombstone = (m_slots[idx].state == slot_state::tombstone);

            m_slots[idx] = slot{
                .hash = hash,
                .data = data,
                .length = length,
                .state = slot_state::occupied,
            };

            ++m_size;
            if (!was_tombstone)
                ++m_occupied_plus_tombstones;

            if (should_grow())
                grow(m_capacity * 2);

            return {data, length};
        }

        [[nodiscard]] auto should_grow() const -> bool
        {
            return static_cast<double>(m_occupied_plus_tombstones) > static_cast<double>(m_capacity) * max_load_factor;
        }

        void grow(std::uint32_t new_capacity)
        {
            auto old_slots = std::move(m_slots);

            m_capacity = new_capacity;
            m_slots.clear();
            m_slots.resize(new_capacity);
            m_occupied_plus_tombstones = m_size;

            auto const mask = new_capacity - 1;

            for (auto const& old : old_slots)
            {
                if (old.state != slot_state::occupied)
                    continue;

                auto idx = static_cast<std::uint32_t>(old.hash & mask);
                while (m_slots[idx].state == slot_state::occupied)
                    idx = (idx + 1) & mask;

                m_slots[idx] = slot{
                    .hash = old.hash,
                    .data = old.data,
                    .length = old.length,
                    .state = slot_state::occupied,
                };
            }
        }
    };

    struct InternedStringHash
    {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept
        {
            if (sv.empty()) [[unlikely]]
                return 0;
            return std::hash<void const*>{}(sv.data());
        }
    };

    struct InternedStringEqual
    {
        using is_transparent = void;

        [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const noexcept
        {
            if (a.empty() || b.empty()) [[unlikely]]
                return a.size() == 0 && b.size() == 0;
            return a.data() == b.data();
        }
    };

    template <typename T> using InternedHashMap = std::unordered_map<std::string_view, T, InternedStringHash, InternedStringEqual>;

    template <typename T> using InternedPmrHashMap = std::pmr::unordered_map<std::string_view, T, InternedStringHash, InternedStringEqual>;

    using InternedHashSet = std::unordered_set<std::string_view, InternedStringHash, InternedStringEqual>;

    using InternedPmrHashSet = std::pmr::unordered_set<std::string_view, InternedStringHash, InternedStringEqual>;

} // namespace dcc::si
