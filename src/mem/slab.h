#pragma once

#include "superslab.h"

namespace snmalloc
{
  class Slab
  {
  private:
    uint16_t pointer_to_index(void* p)
    {
      // Get the offset from the slab for a memory location.
      return static_cast<uint16_t>(address_cast(p) - address_cast(this));
    }

  public:
    static Slab* get(void* p)
    {
      return pointer_cast<Slab>(address_cast(p) & SLAB_MASK);
    }

    Metaslab& get_meta()
    {
      Superslab* super = Superslab::get(this);
      return super->get_meta(this);
    }

    uint8_t get_sizeclass()
    {
      Superslab* super = Superslab::get(this);
      return super->get_sizeclass(this);
    }

    SlabLink* get_link()
    {
      return get_meta().get_link(this);
    }

    template<ZeroMem zero_mem, typename MemoryProvider>
    void* alloc(SlabList* sc, size_t rsize, MemoryProvider& memory_provider)
    {
      // Read the head from the metadata stored in the superslab.
      Metaslab& meta = get_meta();
      uint16_t head = meta.head;

      assert(rsize == sizeclass_to_size(get_sizeclass()));
      meta.debug_slab_invariant(is_short(), this, get_sizeclass());
      assert(sc->get_head() == (SlabLink*)((size_t)this + meta.link));
      assert(!meta.is_full());

      meta.add_use();

      void* p;

      if ((head & 1) == 0)
      {
        p = pointer_offset(this, head);

        // Read the next slot from the memory that's about to be allocated.
        uint16_t next = *static_cast<uint16_t*>(p);
        meta.head = next;
      }
      else
      {
        if (meta.head == 1)
        {
          p = pointer_offset(this, meta.link);
          sc->pop();
          meta.set_full();
        }
        else
        {
          // This slab is being bump allocated.
          p = pointer_offset(this, head - 1);
          meta.head = (head + static_cast<uint16_t>(rsize)) & (SLAB_SIZE - 1);
        }
      }

      meta.debug_slab_invariant(is_short(), this, get_sizeclass());

      if constexpr (zero_mem == YesZero)
      {
        if (rsize < PAGE_ALIGNED_SIZE)
          memory_provider.zero(p, rsize);
        else
          memory_provider.template zero<true>(p, rsize);
      }

      return p;
    }

    bool is_start_of_object(Superslab* super, void* p)
    {
      return is_multiple_of_sizeclass(
        sizeclass_to_size(super->get_sizeclass(this)),
        address_cast(this) + SLAB_SIZE - address_cast(p));
    }

    // Returns true, if it alters get_status.
    template<typename MemoryProvider>
    inline typename Superslab::Action dealloc(
      ModArray<NUM_SMALL_CLASSES, SlabList>& scs,
      Superslab* super,
      void* p,
      MemoryProvider& memory_provider)
    {
      Metaslab& meta = super->get_meta(this);

      bool was_full = meta.is_full();
      meta.debug_slab_invariant(is_short(), this, get_sizeclass());
      meta.sub_use();

      if (was_full)
      {
        // We are not on the sizeclass list.
        if (!meta.is_unused())
        {
          // Update the head and the sizeclass link.
          uint16_t index = pointer_to_index(p);
          assert(meta.head == 1);
          meta.link = index;

          // Push on the list of slabs for this sizeclass.
          scs[get_sizeclass()].insert(meta.get_link(this));
          meta.debug_slab_invariant(is_short(), this, get_sizeclass());
        }
        else
        {
          // Dealloc on the superslab.
          if (is_short())
            return super->dealloc_short_slab(memory_provider);

          return super->dealloc_slab(this, memory_provider);
        }
      }
      else if (meta.is_unused())
      {
        // Remove from the sizeclass list and dealloc on the superslab.
        scs[get_sizeclass()].remove(meta.get_link(this));

        if (is_short())
          return super->dealloc_short_slab(memory_provider);

        return super->dealloc_slab(this, memory_provider);
      }
      else
      {
#ifndef NDEBUG
        scs[get_sizeclass()].debug_check_contains(meta.get_link(this));
#endif

        // Update the head and the next pointer in the free list.
        uint16_t head = meta.head;
        uint16_t current = pointer_to_index(p);

        // Set the head to the memory being deallocated.
        meta.head = current;
        assert(meta.valid_head(is_short(), super->get_sizeclass(this)));

        // Set the next pointer to the previous head.
        *static_cast<uint16_t*>(p) = head;
        meta.debug_slab_invariant(is_short(), this, get_sizeclass());
      }
      return Superslab::NoSlabReturn;
    }

    bool is_short()
    {
      return (address_cast(this) & SUPERSLAB_MASK) == address_cast(this);
    }
  };
} // namespace snmalloc
