#pragma once

namespace GW {

    template <typename T>
    struct TLink {
        bool IsLinked() const { return prev_link != this; }

        void Unlink()
        {
            RemoveFromList();

            next_node = reinterpret_cast<T*>(reinterpret_cast<size_t>(this) + 1);
            prev_link = this;
        }

        T* Prev()
        {
            T* prevNode = prev_link->prev_link->next_node;
            if (reinterpret_cast<size_t>(prevNode) & 1)
                return nullptr;
            return prevNode;
        }

        T* Next()
        {
            if (reinterpret_cast<size_t>(next_node) & 1)
                return nullptr;
            return next_node;
        }

        TLink* NextLink()
        {
            const size_t offset = reinterpret_cast<size_t>(this) - (reinterpret_cast<size_t>(prev_link->next_node) & ~1);
            return reinterpret_cast<TLink*>((reinterpret_cast<size_t>(next_node) & ~1) + offset);
        }

        TLink* PrevLink() { return prev_link; }

    protected:
        TLink* prev_link; // +h0000
        T* next_node;     // +h0004

        void RemoveFromList()
        {
            NextLink()->prev_link = prev_link;
            prev_link->next_node = next_node;
        }
    };

    template <typename T>
    struct TList {
        class iterator {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = T;

            iterator()
                : current(nullptr)
                , first(nullptr)
            {
            }
            explicit iterator(TLink<T>* node, TLink<T>* first = nullptr)
                : current(node)
                , first(first)
            {
            }

            T& operator*() { return *current->Next(); }
            T* operator->() { return current->Next(); }
            T& operator*() const { return *current->Next(); }
            T* operator->() const { return current->Next(); }

            iterator& operator++()
            {
                if (current->NextLink() == first && first != nullptr)
                    iteration++;
                current = current->NextLink();
                return *this;
            }

            iterator operator++(int)
            {
                iterator it(current);
                ++*this;
                return it;
            }

            bool operator==(const iterator& other) const { return current == other.current && iteration == other.iteration; }

            bool operator!=(const iterator& other) const { return !(*this == other); }

        private:
            TLink<T>* current;
            TLink<T>* first;
            int iteration = 0;
        };

        iterator begin() { return iterator(&link, &link); }

        iterator end()
        {
            TLink<T>* last = &link;
            while (last->Next() != nullptr) {
                if (last->NextLink() == &link) {
                    return ++iterator(last, &link);
                }
                last = last->NextLink();
            }
            return iterator(last, &link);
        }

        [[nodiscard]] iterator begin() const { return iterator(&link); }
        [[nodiscard]] iterator end() const { return end(); }

        TLink<T>* Get() { return &link; }

    protected:
        size_t offset{};
        TLink<T> link;
    };

    static_assert(sizeof(TList<void*>) == 0xc);
}
