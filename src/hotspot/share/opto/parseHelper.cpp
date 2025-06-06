/*
 * Copyright (c) 1998, 2025, Oracle and/or its affiliates. All rights reserved.
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
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "ci/ciInlineKlass.hpp"
#include "ci/ciSymbols.hpp"
#include "compiler/compileLog.hpp"
#include "oops/flatArrayKlass.hpp"
#include "oops/objArrayKlass.hpp"
#include "opto/addnode.hpp"
#include "opto/castnode.hpp"
#include "opto/inlinetypenode.hpp"
#include "opto/memnode.hpp"
#include "opto/mulnode.hpp"
#include "opto/parse.hpp"
#include "opto/rootnode.hpp"
#include "opto/runtime.hpp"
#include "runtime/sharedRuntime.hpp"

//------------------------------make_dtrace_method_entry_exit ----------------
// Dtrace -- record entry or exit of a method if compiled with dtrace support
void GraphKit::make_dtrace_method_entry_exit(ciMethod* method, bool is_entry) {
  const TypeFunc *call_type    = OptoRuntime::dtrace_method_entry_exit_Type();
  address         call_address = is_entry ? CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry) :
                                            CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit);
  const char     *call_name    = is_entry ? "dtrace_method_entry" : "dtrace_method_exit";

  // Get base of thread-local storage area
  Node* thread = _gvn.transform( new ThreadLocalNode() );

  // Get method
  const TypePtr* method_type = TypeMetadataPtr::make(method);
  Node *method_node = _gvn.transform(ConNode::make(method_type));

  kill_dead_locals();

  // For some reason, this call reads only raw memory.
  const TypePtr* raw_adr_type = TypeRawPtr::BOTTOM;
  make_runtime_call(RC_LEAF | RC_NARROW_MEM,
                    call_type, call_address,
                    call_name, raw_adr_type,
                    thread, method_node);
}


//=============================================================================
//------------------------------do_checkcast-----------------------------------
void Parse::do_checkcast() {
  bool will_link;
  ciKlass* klass = iter().get_klass(will_link);
  Node *obj = peek();

  // Throw uncommon trap if class is not loaded or the value we are casting
  // _from_ is not loaded, and value is not null.  If the value _is_ null,
  // then the checkcast does nothing.
  const TypeOopPtr *tp = _gvn.type(obj)->isa_oopptr();
  if (!will_link || (tp && !tp->is_loaded())) {
    if (C->log() != nullptr) {
      if (!will_link) {
        C->log()->elem("assert_null reason='checkcast' klass='%d'",
                       C->log()->identify(klass));
      }
      if (tp && !tp->is_loaded()) {
        // %%% Cannot happen?
        ciKlass* klass = tp->unloaded_klass();
        C->log()->elem("assert_null reason='checkcast source' klass='%d'",
                       C->log()->identify(klass));
      }
    }
    null_assert(obj);
    assert( stopped() || _gvn.type(peek())->higher_equal(TypePtr::NULL_PTR), "what's left behind is null" );
    return;
  }

  Node* res = gen_checkcast(obj, makecon(TypeKlassPtr::make(klass, Type::trust_interfaces)));
  if (stopped()) {
    return;
  }

  // Pop from stack AFTER gen_checkcast because it can uncommon trap and
  // the debug info has to be correct.
  pop();
  push(res);
}


//------------------------------do_instanceof----------------------------------
void Parse::do_instanceof() {
  if (stopped())  return;
  // We would like to return false if class is not loaded, emitting a
  // dependency, but Java requires instanceof to load its operand.

  // Throw uncommon trap if class is not loaded
  bool will_link;
  ciKlass* klass = iter().get_klass(will_link);

  if (!will_link) {
    if (C->log() != nullptr) {
      C->log()->elem("assert_null reason='instanceof' klass='%d'",
                     C->log()->identify(klass));
    }
    null_assert(peek());
    assert( stopped() || _gvn.type(peek())->higher_equal(TypePtr::NULL_PTR), "what's left behind is null" );
    if (!stopped()) {
      // The object is now known to be null.
      // Shortcut the effect of gen_instanceof and return "false" directly.
      pop();                   // pop the null
      push(_gvn.intcon(0));    // push false answer
    }
    return;
  }

  // Push the bool result back on stack
  Node* res = gen_instanceof(peek(), makecon(TypeKlassPtr::make(klass, Type::trust_interfaces)), true);

  // Pop from stack AFTER gen_instanceof because it can uncommon trap.
  pop();
  push(res);
}

//------------------------------array_store_check------------------------------
// pull array from stack and check that the store is valid
Node* Parse::array_store_check(Node*& adr, const Type*& elemtype) {
  // Shorthand access to array store elements without popping them.
  Node *obj = peek(0);
  Node *idx = peek(1);
  Node *ary = peek(2);

  if (_gvn.type(obj) == TypePtr::NULL_PTR) {
    // There's never a type check on null values.
    // This cutout lets us avoid the uncommon_trap(Reason_array_check)
    // below, which turns into a performance liability if the
    // gen_checkcast folds up completely.
    if (_gvn.type(ary)->is_aryptr()->is_null_free()) {
      null_check(obj);
    }
    return obj;
  }

  // Extract the array klass type
  Node* array_klass = load_object_klass(ary);
  // Get the array klass
  const TypeKlassPtr* tak = _gvn.type(array_klass)->is_klassptr();

  // The type of array_klass is usually INexact array-of-oop.  Heroically
  // cast array_klass to EXACT array and uncommon-trap if the cast fails.
  // Make constant out of the inexact array klass, but use it only if the cast
  // succeeds.
  if (MonomorphicArrayCheck && !tak->klass_is_exact()) {
    // Make a constant out of the inexact array klass
    const TypeAryKlassPtr* extak = nullptr;
    const TypeOopPtr* ary_t = _gvn.type(ary)->is_oopptr();
    ciKlass* ary_spec = ary_t->speculative_type();
    Deoptimization::DeoptReason reason = Deoptimization::Reason_none;
    // Try to cast the array to an exact type from profile data. First
    // check the speculative type.
    if (ary_spec != nullptr && !too_many_traps(Deoptimization::Reason_speculate_class_check)) {
      extak = TypeKlassPtr::make(ary_spec)->is_aryklassptr();
      reason = Deoptimization::Reason_speculate_class_check;
    } else if (UseArrayLoadStoreProfile) {
      // No speculative type: check profile data at this bci.
      reason = Deoptimization::Reason_class_check;
      if (!too_many_traps(reason)) {
        ciKlass* array_type = nullptr;
        ciKlass* element_type = nullptr;
        ProfilePtrKind element_ptr = ProfileMaybeNull;
        bool flat_array = true;
        bool null_free_array = true;
        method()->array_access_profiled_type(bci(), array_type, element_type, element_ptr, flat_array, null_free_array);
        if (array_type != nullptr) {
          extak = TypeKlassPtr::make(array_type)->is_aryklassptr();
        }
      }
    } else if (!too_many_traps(Deoptimization::Reason_array_check) && tak->isa_aryklassptr()) {
      // If the compiler has determined that the type of array 'ary' (represented
      // by 'array_klass') is java/lang/Object, the compiler must not assume that
      // the array 'ary' is monomorphic.
      //
      // If 'ary' were of type java/lang/Object, this arraystore would have to fail,
      // because it is not possible to perform a arraystore into an object that is not
      // a "proper" array.
      //
      // Therefore, let's obtain at runtime the type of 'ary' and check if we can still
      // successfully perform the store.
      //
      // The implementation reasons for the condition are the following:
      //
      // java/lang/Object is the superclass of all arrays, but it is represented by the VM
      // as an InstanceKlass. The checks generated by gen_checkcast() (see below) expect
      // 'array_klass' to be ObjArrayKlass, which can result in invalid memory accesses.
      //
      // See issue JDK-8057622 for details.
      extak = tak->cast_to_exactness(true)->is_aryklassptr();
      reason = Deoptimization::Reason_array_check;
    }
    if (extak != nullptr && extak->exact_klass(true) != nullptr) {
      Node* con = makecon(extak);
      Node* cmp = _gvn.transform(new CmpPNode(array_klass, con));
      Node* bol = _gvn.transform(new BoolNode(cmp, BoolTest::eq));
      // Only do it if the check does not always pass/fail
      if (!bol->is_Con()) {
        { BuildCutout unless(this, bol, PROB_MAX);
          uncommon_trap(reason,
                        Deoptimization::Action_maybe_recompile,
                        extak->exact_klass());
        }
        // Cast array klass to exactness
        replace_in_map(array_klass, con);
        array_klass = con;
        Node* cast = _gvn.transform(new CheckCastPPNode(control(), ary, extak->as_instance_type()));
        replace_in_map(ary, cast);
        ary = cast;

        // Recompute element type and address
        const TypeAryPtr* arytype = _gvn.type(ary)->is_aryptr();
        elemtype = arytype->elem();
        adr = array_element_address(ary, idx, T_OBJECT, arytype->size(), control());

        CompileLog* log = C->log();
        if (log != nullptr) {
          log->elem("cast_up reason='monomorphic_array' from='%d' to='(exact)'",
                    log->identify(extak->exact_klass()));
        }
      }
    }
  }

  // Come here for polymorphic array klasses

  // Extract the array element class
  int element_klass_offset = in_bytes(ArrayKlass::element_klass_offset());
  Node* p2 = basic_plus_adr(array_klass, array_klass, element_klass_offset);
  Node* a_e_klass = _gvn.transform(LoadKlassNode::make(_gvn, immutable_memory(), p2, tak));

  // If we statically know that this is an inline type array, use precise element klass for checkcast
  const TypeAryPtr* arytype = _gvn.type(ary)->is_aryptr();
  const TypePtr* elem_ptr = elemtype->make_ptr();
  bool null_free = arytype->is_null_free();
  if (elem_ptr->is_inlinetypeptr()) {
    // We statically know that this is an inline type array, use precise klass ptr
    a_e_klass = makecon(TypeKlassPtr::make(elemtype->inline_klass()));
  }
#ifdef ASSERT
  if (!StressReflectiveCode && array_klass->is_Con() != a_e_klass->is_Con()) {
    // When the element type is exact, the array type also needs to be exact. There is one exception, though:
    // Nullable arrays are not exact because the null-free array is a subtype while the element type being a
    // concrete value class (i.e. final) is always exact.
    assert(!array_klass->is_Con() && a_e_klass->is_Con() && elem_ptr->is_inlinetypeptr() && !null_free,
           "a constant element type either matches a constant array type or a non-constant nullable value class array");
  }

  // If the element type is exact, the array can be null-free (i.e. the element type is NotNull) if:
  //   - The elements are inline types
  //   - The array is from an autobox cache.
  // If the element type is inexact, it could represent multiple null-free arrays. Since autobox cache arrays
  // are local to very few cache classes and are only used in the valueOf() methods, they are always exact and are not
  // merged or hidden behind super types. Therefore, an inexact null-free array always represents some kind of
  // inline type array - either of an abstract value class or Object.
  if (null_free) {
    ciKlass* klass = elem_ptr->is_instptr()->instance_klass();
    if (klass->exact_klass()) {
      assert(elem_ptr->is_inlinetypeptr() || arytype->is_autobox_cache(), "elements must be inline type or autobox cache");
    } else {
      assert(!arytype->is_autobox_cache() && elem_ptr->can_be_inline_type() &&
             (klass->is_java_lang_Object() || klass->is_abstract()), "cannot have inexact non-inline type elements");
    }
  }
#endif // ASSERT

  // Check (the hard way) and throw if not a subklass.
  return gen_checkcast(obj, a_e_klass, nullptr, null_free);
}


//------------------------------do_new-----------------------------------------
void Parse::do_new() {
  kill_dead_locals();

  bool will_link;
  ciInstanceKlass* klass = iter().get_klass(will_link)->as_instance_klass();
  assert(will_link, "_new: typeflow responsibility");

  // Should throw an InstantiationError?
  if (klass->is_abstract() || klass->is_interface() ||
      klass->name() == ciSymbols::java_lang_Class() ||
      iter().is_unresolved_klass()) {
    uncommon_trap(Deoptimization::Reason_unhandled,
                  Deoptimization::Action_none,
                  klass);
    return;
  }

  if (C->needs_clinit_barrier(klass, method())) {
    clinit_barrier(klass, method());
    if (stopped())  return;
  }

  Node* kls = makecon(TypeKlassPtr::make(klass));
  Node* obj = new_instance(kls);

  // Push resultant oop onto stack
  push(obj);

  // Keep track of whether opportunities exist for StringBuilder
  // optimizations.
  if (OptimizeStringConcat &&
      (klass == C->env()->StringBuilder_klass() ||
       klass == C->env()->StringBuffer_klass())) {
    C->set_has_stringbuilder(true);
  }

  // Keep track of boxed values for EliminateAutoBox optimizations.
  if (C->eliminate_boxing() && klass->is_box_klass()) {
    C->set_has_boxed_value(true);
  }
}

#ifndef PRODUCT
//------------------------------dump_map_adr_mem-------------------------------
// Debug dump of the mapping from address types to MergeMemNode indices.
void Parse::dump_map_adr_mem() const {
  tty->print_cr("--- Mapping from address types to memory Nodes ---");
  MergeMemNode *mem = map() == nullptr ? nullptr : (map()->memory()->is_MergeMem() ?
                                      map()->memory()->as_MergeMem() : nullptr);
  for (uint i = 0; i < (uint)C->num_alias_types(); i++) {
    C->alias_type(i)->print_on(tty);
    tty->print("\t");
    // Node mapping, if any
    if (mem && i < mem->req() && mem->in(i) && mem->in(i) != mem->empty_memory()) {
      mem->in(i)->dump();
    } else {
      tty->cr();
    }
  }
}

#endif
