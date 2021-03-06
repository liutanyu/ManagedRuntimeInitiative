/*
 * Copyright 2000-2004 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *  
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The date of such changes is 2010.
// Copyright 2010 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, Inc., 1600 Plymouth Street, Mountain View, 
// CA 94043 USA, or visit www.azulsystems.com if you need additional information 
// or have any questions.
#ifndef MEMREGION_HPP
#define MEMREGION_HPP

#include "allocation.hpp"

// A very simple data structure representing a contigous region
// region of address space.

// Note that MemRegions are passed by value, not by reference.
// The intent is that they remain very small and contain no
// objects.

class MemRegion VALUE_OBJ_CLASS_SPEC {
private:
  HeapWord* _start;
  size_t    _word_size;

public:
  MemRegion() : _start(NULL), _word_size(0) {};
  MemRegion(HeapWord* start, size_t word_size) :
    _start(start), _word_size(word_size) {};
  MemRegion(HeapWord* start, HeapWord* end) :
    _start(start), _word_size(pointer_delta(end, start)) {
    assert(end >= start, "incorrect constructor arguments");
  }
  
  MemRegion(const MemRegion& mr): _start(mr._start), _word_size(mr._word_size) {}
    
  MemRegion intersection(const MemRegion mr2) const;
  // regions must overlap or be adjacent
  MemRegion _union(const MemRegion mr2) const;
  // minus will fail a guarantee if mr2 is interior to this, 
  // since there's no way to return 2 disjoint regions.
  MemRegion minus(const MemRegion mr2) const;

  HeapWord* start() const { return _start; }
  HeapWord* end() const   { return _start + _word_size; }
  HeapWord* last() const  { return _start + _word_size - 1; }

  void set_start(HeapWord* start) { _start = start; }
  void set_end(HeapWord* end)     { _word_size = pointer_delta(end, _start); }
  void set_word_size(size_t word_size) {
    _word_size = word_size;
  }

  bool contains(const MemRegion mr2) const {
    return _start <= mr2._start && end() >= mr2.end();
  }
  bool contains(const void* addr) const {
    return addr >= (void*)_start && addr < (void*)end();
  }
  bool equals(const MemRegion mr2) const {
    // first disjunct since we do not have a canonical empty set
    return ((is_empty() && mr2.is_empty()) ||
            (start() == mr2.start() && end() == mr2.end()));
  }

  size_t byte_size() const { return _word_size * sizeof(HeapWord); }
  size_t word_size() const { return _word_size; }

  bool is_empty() const { return word_size() == 0; }

  // Fill the space with parsable heap objects
  void fill();
};

// For iteration over MemRegion's.

class MemRegionClosure : public StackObj {
public:
  virtual void do_MemRegion(MemRegion mr) = 0;
};

// A ResourceObj version of MemRegionClosure

class MemRegionClosureRO: public MemRegionClosure {
public:
  void* operator new(size_t size, ResourceObj::allocation_type type) {
	return ResourceObj::operator new(size, type);
  }
  void* operator new(size_t size, Arena *arena) {
	return ResourceObj::operator new(size, arena);
  }
  void* operator new(size_t size) {
	return ResourceObj::operator new(size);
  }

  void  operator delete(void* p) {} // nothing to do
};
#endif // MEMREGION_HPP
