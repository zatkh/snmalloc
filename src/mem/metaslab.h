#pragma once

#include "../ds/dllist.h"
#include "../ds/helpers.h"
#include "sizeclass.h"

namespace snmalloc
{
  class Slab;

  struct SlabLink
  {
    SlabLink* prev;
    SlabLink* next;

    Slab* get_slab()
    {
      return pointer_cast<Slab>(address_cast(this) & SLAB_MASK);
    }
  };

  using SlabList = DLList<SlabLink, InvalidPointer<UINTPTR_MAX>>;

  static_assert(
    sizeof(SlabLink) <= MIN_ALLOC_SIZE,
    "Need to be able to pack a SlabLink into any free small alloc");

  static constexpr uint16_t SLABLINK_INDEX =
    static_cast<uint16_t>(SLAB_SIZE - sizeof(SlabLink));

  // The Metaslab represent the status of a single slab.
  // This can be either a short or a standard slab.
  class Metaslab
  {
  private:
    // How many entries are used in this slab.
    uint16_t used = 0;

  public:
    // Bump free list of unused entries in this sizeclass.
    // If the bottom bit is 1, then this represents a bump_ptr
    // of where we have allocated up to in this slab. Otherwise,
    // it represents the location of the first block in the free
    // list.  The free list is chained through deallocated blocks.
    // It either terminates with a bump ptr, or if all the space is in
    // the free list, then the last block will be also referenced by
    // link.
    // Note that, the first entry in a slab is never bump allocated
    // but is used for the link. This means that 1 represents the fully
    // bump allocated slab.
    Mod<SLAB_SIZE, uint16_t> head;
    // When a slab has free space it will be on the has space list for
    // that size class.  We use an empty block in this slab to be the
    // doubly linked node into that size class's free list.
    // If a slab is currently unused, then link is used to connect it to other
    // free slabs in the superslab.
    Mod<SLAB_SIZE, uint16_t> link;

    void add_use()
    {
      used++;
    }

    void sub_use()
    {
      used--;
    }

    void set_unused()
    {
      used = 0;
    }

    bool is_unused()
    {
      return used == 0;
    }

    bool is_full()
    {
      return link == 1;
    }

    void set_full()
    {
      assert(head == 1);
      assert(link != 1);
      link = 1;
    }

    SlabLink* get_link(Slab* slab)
    {
      return reinterpret_cast<SlabLink*>(pointer_offset(slab, link));
    }

    bool valid_head(bool is_short, uint8_t sizeclass)
    {
      size_t size = sizeclass_to_size(sizeclass);
      size_t offset = get_slab_offset(sizeclass, is_short);
      size_t all_high_bits = ~static_cast<size_t>(1);

      size_t head_start = head & all_high_bits;
      size_t slab_start = offset & all_high_bits;

      return ((head_start - slab_start) % size) == 0;
    }

    void debug_slab_invariant(bool is_short, Slab* slab, uint8_t sizeclass)
    {
#if !defined(NDEBUG) && !defined(SNMALLOC_CHEAP_CHECKS)
      size_t size = sizeclass_to_size(sizeclass);
      size_t offset = get_slab_offset(sizeclass, is_short) - 1;

      size_t accounted_for = used * size + offset;

      if (is_full())
      {
        // All the blocks must be used.
        assert(SLAB_SIZE == accounted_for);
        // There is no free list to validate
        // 'link' value is not important if full.
        return;
      }
      // Block is not full
      assert(SLAB_SIZE > accounted_for);

      // Walk bump-free-list-segment accounting for unused space
      uint16_t curr = head;
      while ((curr & 1) != 1)
      {
        // Check we are looking at a correctly aligned block
        uint16_t start = curr;
        assert((start - offset) % size == 0);

        // Account for free elements in free list
        accounted_for += size;
        assert(SLAB_SIZE >= accounted_for);
        // We should never reach the link node in the free list.
        assert(curr != link);

        // Iterate bump/free list segment
        curr = *reinterpret_cast<uint16_t*>(pointer_offset(slab, curr));
      }

      if (curr != 1)
      {
        // Check we terminated traversal on a correctly aligned block
        uint16_t start = curr & ~1;
        assert((start - offset) % size == 0);

        // Account for to be bump allocated space
        accounted_for += SLAB_SIZE - (curr - 1);

        // The link should be the first allocation as we
        // haven't completely filled this block at any point.
        assert(link == (get_slab_offset(sizeclass, is_short) - 1));
      }

      assert(link != 1);
      // Add the link node.
      accounted_for += size;

      // All space accounted for
      assert(SLAB_SIZE == accounted_for);
#else
      UNUSED(slab);
      UNUSED(is_short);
      UNUSED(sizeclass);
#endif
    }
  };

  static_assert(sizeof(Metaslab) == 6, "Should be 6 bytes");
} // namespace snmalloc
