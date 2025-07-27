#pragma once

#include "az/common.hpp"

#define az_list_entry(ptr, type, field) ((type *)((char *)(ptr) - (unsigned long)(&(((type *)(0))->field))))

namespace az {

/**
 * `ListNode` is intended to be embedded in other structs.
 * NOTE: Copying a list node is ambiguous. Suppose a -> b and then copy b, what should a.next be?
 */
struct ListNode : Noncopyable {
    ListNode *prev = nullptr;
    ListNode *next = nullptr;

    ListNode() : prev(this), next(this) {}

    ~ListNode() {
        // If the node is destructed while on a list, we set prev and next pointers to nullptr
        // to prevent unnoticed illegal accesses
        auto next_copy = this->next;
        if (this->prev) {
            this->prev->next = nullptr;
        }
        if (next_copy) {
            next_copy->prev = nullptr;
        }

        this->prev = nullptr;
        this->next = nullptr;
    }

    ListNode(ListNode &&other) {
        if (other.empty()) {
            // We are cloning an empty list
            this->prev = this;
            this->next = this;
        } else {
            this->prev = other.prev;
            this->next = other.next;
            this->prev->next = this;
            this->next->prev = this;
        }

        other.prev = nullptr;
        other.next = nullptr;
    }

    ListNode &operator=(ListNode &&other) {
        if (other.empty()) {
            // We are cloning an empty list
            this->prev = this;
            this->next = this;
        } else {
            this->prev = other.prev;
            this->next = other.next;
            this->prev->next = this;
            this->next->prev = this;
        }

        other.prev = nullptr;
        other.next = nullptr;

        return *this;
    }

    template <typename T>
    T *get_owner_ptr(ListNode T::*member_ptr) const {
        // Calculate offset within parent struct
        const auto offset = reinterpret_cast<std::ptrdiff_t>(&(reinterpret_cast<T *>(0)->*member_ptr));

        // Convert to byte pointer, adjust by offset, cast to parent type
        return reinterpret_cast<T *>(const_cast<std::byte *>(reinterpret_cast<const std::byte *>(this) - offset));
    }

    bool empty() const {
        return next == this;
    }

    void link_to(ListNode &other) {
        this->next->prev = other.prev;
        other.prev->next = this->next;
        this->next = &other;
        other.prev = this;
    }

    void detach() {
        prev->next = this->next;
        next->prev = this->prev;
        this->next = this;
        this->prev = this;
    }

    /**
     * detach() + link_to(other)
     */
    void move_to(ListNode &other) {
        this->prev->next = this->next;
        this->next->prev = this->prev;
        this->prev = other.prev;
        other.prev->next = this;
        this->next = &other;
        other.prev = this;
    }
};

}  // namespace az
