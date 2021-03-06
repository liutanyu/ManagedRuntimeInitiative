/*
 * Copyright 1997-2007 Sun Microsystems, Inc.  All Rights Reserved.
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


#include "bytes_pd.hpp"
#include "bytecodes.hpp"
#include "bytecodeHistogram.hpp"
#include "bytecodeTracer.hpp"
#include "javaClasses.hpp"
#include "ostream.hpp"
#include "resourceArea.hpp"
#include "orderAccess_os_pd.inline.hpp"

// Standard closure for BytecodeTracer: prints the current bytecode
// and its attributes using bytecode-specific information.

class BytecodePrinter: public BytecodeClosure {
 private:
  // %%% This field is not GC-ed, and so can contain garbage
  // between critical sections.  Use only pointer-comparison
  // operations on the pointer, except within a critical section.
  // (Also, ensure that occasional false positives are benign.)
  const methodOopDesc* _current_method;
  bool      _is_wide;
  address   _next_pc;                // current decoding position

  void      align()                  { _next_pc = (address)round_to((intptr_t)_next_pc, sizeof(jint)); }
  int       get_byte()               { return *(jbyte*) _next_pc++; }  // signed
  short     get_short()              { short i=Bytes::get_Java_u2(_next_pc); _next_pc+=2; return i; }
  int       get_int()                { int i=Bytes::get_Java_u4(_next_pc); _next_pc+=4; return i; }

  int       get_index()              { return *(address)_next_pc++; }
  int       get_big_index()          { int i=Bytes::get_Java_u2(_next_pc); _next_pc+=2; return i; }
int get_giant_index(){int i=Bytes::get_native_u4(_next_pc);_next_pc+=4;return i;}
  int       get_index_special()      { return (is_wide()) ? get_big_index() : get_index(); }
const methodOopDesc*method()const{return _current_method;}
  bool      is_wide()                { return _is_wide; }


  bool      check_index(int i, bool in_cp_cache, int& cp_index, outputStream* st = tty);
  void      print_constant(int i, outputStream* st = tty);
void print_field_or_method(int i,outputStream*st=tty);
  void      print_attributes(Bytecodes::Code code, int bci, outputStream* st = tty);
  void      bytecode_epilog(int bci, outputStream* st = tty);

 public:
  BytecodePrinter() {
    _is_wide = false;
  }

  // This method is called while executing the raw bytecodes, so none of
  // the adjustments that BytecodeStream performs applies.
  void trace(methodHandle method, address bcp, uintptr_t tos, uintptr_t tos2, outputStream* st) {
    ResourceMark rm;
    if (_current_method != method()) {
      // Note 1: This code will not work as expected with true MT/MP.
      //         Need an explicit lock or a different solution.
      // It is possible for this block to be skipped, if a garbage
      // _current_method pointer happens to have the same bits as
      // the incoming method.  We could lose a line of trace output.
      // This is acceptable in a debug-only feature.
      st->cr();
      st->print("[%d] ", (int) Thread::current()->osthread()->thread_id());
      method->print_name(st);
      st->cr();
      _current_method = method();
    }
    Bytecodes::Code code;
    if (is_wide()) {
      // bcp wasn't advanced if previous bytecode was _wide.
      code = Bytecodes::code_at(bcp+1);
    } else {
      code = Bytecodes::code_at(bcp);
    }
    int bci = bcp - method->code_base();
    st->print("[%d] ", (int) Thread::current()->osthread()->thread_id());
    if (Verbose) {
st->print("%8lld  %4d  "INTPTR_FORMAT" "INTPTR_FORMAT" %s",
           BytecodeCounter::counter_value(), bci, tos, tos2, Bytecodes::name(code));
    } else {
st->print("%8lld  %4d  %s",
           BytecodeCounter::counter_value(), bci, Bytecodes::name(code));
    }
    _next_pc = is_wide() ? bcp+2 : bcp+1;
    print_attributes(code, bci);
    // Set is_wide for the next one, since the caller of this doesn't skip
    // the next bytecode.
    _is_wide = (code == Bytecodes::_wide);
  }

  // Used for methodOop::print_codes().  The input bcp comes from
  // BytecodeStream, which will skip wide bytecodes.
  void trace(methodHandle method, address bcp, outputStream* st) {
    _current_method = method();
    ResourceMark rm;
    Bytecodes::Code code = Bytecodes::code_at(bcp);
    // Set is_wide
    _is_wide = (code == Bytecodes::_wide);
    if (is_wide()) {
      code = Bytecodes::code_at(bcp+1);
    }
    int bci = bcp - method->code_base();
    // Print bytecode index and name
    if (is_wide()) {
      st->print("%d %s_w", bci, Bytecodes::name(code));
    } else {
      st->print("%d %s", bci, Bytecodes::name(code));
    }
    _next_pc = is_wide() ? bcp+2 : bcp+1;
    print_attributes(code, bci, st);
    bytecode_epilog(bci, st);
  }

  static void print_one_bytecode(const methodOopDesc *const moop, int bci, outputStream *out);
};


// Implementation of BytecodeTracer

// %%% This set_closure thing seems overly general, given that
// nobody uses it.  Also, if BytecodePrinter weren't hidden
// then methodOop could use instances of it directly and it
// would be easier to remove races on _current_method and bcp.
// Since this is not product functionality, we can defer cleanup.

BytecodeClosure* BytecodeTracer::_closure = NULL;

void BytecodePrinter::print_one_bytecode(const methodOopDesc *const moop, int bci, outputStream *out) {
  ResourceMark rm;
  BytecodePrinter BP;
  BP._current_method = moop;
address bcp=moop->bcp_from(bci);
  Bytecodes::Code code = (Bytecodes::Code)(bcp[0]);
  if( *bcp == Bytecodes::_wide ) {
    BP._is_wide = true;
    BP._next_pc = bcp+2;
    code = (Bytecodes::Code)(bcp[1]);
    out->print(Bytecodes::name(code));
out->print("_w");
  } else {
    BP._next_pc = bcp+1;
    out->print(Bytecodes::name(code));
  }
  BP.print_attributes(code,bci,out);
}

void BytecodeTracer ::print_one_bytecode(const methodOopDesc *const moop, int bci, outputStream *out) {
  BytecodePrinter::print_one_bytecode(moop,bci,out);
}

static BytecodePrinter std_closure;
BytecodeClosure* BytecodeTracer::std_closure() {
  return &::std_closure;
}

void BytecodeTracer::trace(methodHandle method, address bcp, uintptr_t tos, uintptr_t tos2, outputStream* st) {
  if (TraceBytecodes && BytecodeCounter::counter_value() >= TraceBytecodesAt) {
    ttyLocker ttyl;  // 5065316: keep the following output coherent
    // The ttyLocker also prevents races between two threads
    // trying to use the single instance of BytecodePrinter.
    // Using the ttyLocker prevents the system from coming to
    // a safepoint within this code, which is sensitive to methodOop
    // movement.
    //
    // There used to be a leaf mutex here, but the ttyLocker will
    // work just as well, as long as the printing operations never block.
    //
    // We put the locker on the static trace method, not the
    // virtual one, because the clients of this module go through
    // the static method.
    _closure->trace(method, bcp, tos, tos2, st);
  }
}

void BytecodeTracer::trace(methodHandle method, address bcp, outputStream* st) {
  ttyLocker ttyl;  // 5065316: keep the following output coherent
  _closure->trace(method, bcp, st);
}

void print_oop(oop value, outputStream* st) {
  if (value == NULL) {
    st->print_cr(" NULL");
  } else {
    EXCEPTION_MARK;
    Handle h_value (THREAD, value);
    symbolHandle sym = java_lang_String::as_symbol(h_value, CATCH);
    if (sym->utf8_length() > 32) {
      st->print_cr(" ....");
    } else {
      sym->print_on(st); st->cr();
    }
  }
}

bool BytecodePrinter::check_index(int i, bool in_cp_cache, int& cp_index, outputStream* st) {
  constantPoolOop constants = method()->constants();
  int ilimit = constants->length(), climit = 0;

  constantPoolCacheOop cache = NULL;
  if (in_cp_cache) {
    cache = constants->cache();
if(cache!=NULL){
      //climit = cache->length();  // %%% private!
size_t size=cache->size()*HeapWordSize;
size-=sizeof(constantPoolCacheOopDesc);
      size /= sizeof(ConstantPoolCacheEntry);
      climit = (int) size;
    }
  }

if(cache!=NULL){
i=Bytes::swap_u2(i);
    //if (WizardMode)  st->print(" (swap=%d)", i);
    goto check_cache_index;
  }

 check_cp_index:
  if (i >= 0 && i < ilimit) {
    //if (WizardMode)  st->print(" cp[%d]", i);
cp_index=i;
    return true;
  }

st->print_cr(" CP[%d] not in CP",i);
  return false;

 check_cache_index:
  if (i >= 0 && i < climit) {
i=cache->entry_at(i)->constant_pool_index();
    goto check_cp_index;
  }
st->print_cr("%d not in CP[*]?",i);
  return false;
}

void BytecodePrinter::print_constant(int i, outputStream* st) {
int orig_i=i;
  if (!check_index(orig_i, false, i, st))  return;

  constantPoolOop constants = method()->constants();
  constantTag tag = constants->tag_at(i);

  if (tag.is_int()) {
st->print_cr(" %d",constants->int_at(i));
  } else if (tag.is_long()) {
st->print_cr(" %lld",constants->long_at(i));
  } else if (tag.is_float()) {
    st->print_cr(" %f", constants->float_at(i));
  } else if (tag.is_double()) {
    st->print_cr(" %f", constants->double_at(i));
  } else if (tag.is_string()) {
    oop string = constants->resolved_string_at(i);
    print_oop(string, st);
  } else if (tag.is_unresolved_string()) {
    st->print_cr(" <unresolved string at %d>", i);
  } else if (tag.is_klass()) {
    st->print_cr(" %s", constants->resolved_klass_at(i)->klass_part()->external_name());
  } else if (tag.is_unresolved_klass()) {
    st->print_cr(" <unresolved klass at %d>", i);
  } else {
    st->print_cr(" bad tag=%d at %d", tag.value(), i);
  }
}

void BytecodePrinter::print_field_or_method(int i,outputStream*st){
int orig_i=i;
  if (!check_index(orig_i, true, i, st))  return;

  constantPoolOop constants = method()->constants();
  constantTag tag = constants->tag_at(i);
  jbyte tag_value = tag.value();

  switch (tag_value) {
  case JVM_CONSTANT_InterfaceMethodref:
  case JVM_CONSTANT_Methodref:
  case JVM_CONSTANT_Fieldref:
    break;
  default:
    st->print_cr(" bad tag=%d at %d", tag_value, i);
    return;
  }

  symbolOop klass = constants->klass_ref_at_noresolve(orig_i);
symbolOop name=constants->name_ref_at(orig_i);
symbolOop signature=constants->signature_ref_at(orig_i);
  st->print_cr( tag_value == JVM_CONSTANT_Fieldref ? " %d %s.%s <%s> " : " %d %s.%s%s ", 
                i, klass ? klass->as_C_string() : NULL, name->as_C_string(), signature->as_C_string());
}


void BytecodePrinter::print_attributes(Bytecodes::Code code, int bci, outputStream* st) {
  // Show attributes of pre-rewritten codes
  code = Bytecodes::java_code(code);
  // If the code doesn't have any fields there's nothing to print.
  // note this is ==1 because the tableswitch and lookupswitch are
  // zero size (for some reason) and we want to print stuff out for them.
  if (Bytecodes::length_for(code) == 1) {
    st->cr();
    return;
  }

  switch(code) {
    // Java specific bytecodes only matter.
    case Bytecodes::_bipush:
      st->print_cr(" " INT32_FORMAT, get_byte());
      break;
    case Bytecodes::_sipush:
      st->print_cr(" " INT32_FORMAT, get_short());
      break;
    case Bytecodes::_ldc:
      print_constant(get_index(), st);
      break;

    case Bytecodes::_ldc_w:
    case Bytecodes::_ldc2_w:
      print_constant(get_big_index(), st);
      break;

    case Bytecodes::_iload:
    case Bytecodes::_lload:
    case Bytecodes::_fload:
    case Bytecodes::_dload:
    case Bytecodes::_aload:
    case Bytecodes::_istore:
    case Bytecodes::_lstore:
    case Bytecodes::_fstore:
    case Bytecodes::_dstore:
    case Bytecodes::_astore:
      st->print_cr(" #%d", get_index_special());
      break;

    case Bytecodes::_iinc:
      { int index = get_index_special();
        jint offset = is_wide() ? get_short(): get_byte();
        st->print_cr(" #%d " INT32_FORMAT, index, offset);
      }
      break;

    case Bytecodes::_newarray: {
        BasicType atype = (BasicType)get_index();
        const char* str = type2name(atype);
        if (str == NULL || atype == T_OBJECT || atype == T_ARRAY) {
          assert(false, "Unidentified basic type");
        }
        st->print_cr(" %s", str);
      }
      break;
    case Bytecodes::_anewarray: {
        int klass_index = get_big_index();
        constantPoolOop constants = method()->constants();
        symbolOop name = constants->klass_name_at(klass_index);
        st->print_cr(" %s ", name->as_C_string());
      }
      break;
    case Bytecodes::_multianewarray: {
        int klass_index = get_big_index();
        int nof_dims = get_index();
        constantPoolOop constants = method()->constants();
        symbolOop name = constants->klass_name_at(klass_index);
        st->print_cr(" %s %d", name->as_C_string(), nof_dims);
      }
      break;

    case Bytecodes::_ifeq:
    case Bytecodes::_ifnull:
    case Bytecodes::_iflt:
    case Bytecodes::_ifle:
    case Bytecodes::_ifne:
    case Bytecodes::_ifnonnull:
    case Bytecodes::_ifgt:
    case Bytecodes::_ifge:
    case Bytecodes::_if_icmpeq:
    case Bytecodes::_if_icmpne:
    case Bytecodes::_if_icmplt:
    case Bytecodes::_if_icmpgt:
    case Bytecodes::_if_icmple:
    case Bytecodes::_if_icmpge:
    case Bytecodes::_if_acmpeq:
    case Bytecodes::_if_acmpne:
    case Bytecodes::_goto:
    case Bytecodes::_jsr:
      st->print_cr(" %d", bci + get_short());
      break;

    case Bytecodes::_goto_w:
    case Bytecodes::_jsr_w:
      st->print_cr(" %d", bci + get_int());
      break;

    case Bytecodes::_ret: st->print_cr(" %d", get_index_special()); break;

    case Bytecodes::_tableswitch:
      { align();
        int  default_dest = bci + get_int();
        int  lo           = get_int();
        int  hi           = get_int();
        int  len          = hi - lo + 1;
        jint* dest        = NEW_RESOURCE_ARRAY(jint, len);
        for (int i = 0; i < len; i++) {
          dest[i] = bci + get_int();
        }
        st->print(" %d " INT32_FORMAT " " INT32_FORMAT " ",
                      default_dest, lo, hi);
        int first = true;
        for (int ll = lo; ll <= hi; ll++, first = false)  {
          int idx = ll - lo;
          const char *format = first ? " %d:" INT32_FORMAT " (delta: %d)" :
                                       ", %d:" INT32_FORMAT " (delta: %d)";
          st->print(format, ll, dest[idx], dest[idx]-bci);
        }
        st->cr();
      }
      break;
    case Bytecodes::_lookupswitch:
      { align();
        int  default_dest = bci + get_int();
        int  len          = get_int();
        jint* key         = NEW_RESOURCE_ARRAY(jint, len);
        jint* dest        = NEW_RESOURCE_ARRAY(jint, len);
        for (int i = 0; i < len; i++) {
          key [i] = get_int();
          dest[i] = bci + get_int();
        };
        st->print(" %d %d ", default_dest, len);
        bool first = true;
        for (int ll = 0; ll < len; ll++, first = false)  {
          const char *format = first ? " " INT32_FORMAT ":" INT32_FORMAT :
                                       ", " INT32_FORMAT ":" INT32_FORMAT ;
          st->print(format, key[ll], dest[ll]);
        }
        st->cr();
      }
      break;

    case Bytecodes::_putstatic:
    case Bytecodes::_getstatic:
    case Bytecodes::_putfield:
case Bytecodes::_getfield:
print_field_or_method(get_big_index(),st);
      break;

    case Bytecodes::_invokevirtual:
    case Bytecodes::_invokespecial:
    case Bytecodes::_invokestatic:
print_field_or_method(get_big_index(),st);
      break;

    case Bytecodes::_invokeinterface:
      { int i = get_big_index();
        int n = get_index();
        get_index();            // ignore zero byte
        print_field_or_method(i, st);
      }
      break;

    case Bytecodes::_new:
    case Bytecodes::_checkcast:
    case Bytecodes::_instanceof:
      { int i = get_big_index();
        constantPoolOop constants = method()->constants();
        symbolOop name = constants->klass_name_at(i);
        st->print_cr(" %d <%s>", i, name->as_C_string());
      }
      break;

    case Bytecodes::_wide:
      // length is zero not one, but printed with no more info.
      break;

    default:
      ShouldNotReachHere();
      break;
  }
}


void BytecodePrinter::bytecode_epilog(int bci, outputStream* st) {
  // TODO: print profile data
}
