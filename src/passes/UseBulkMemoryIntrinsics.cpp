
//
// Merges locals when it is beneficial to do so.
//
// An obvious case is when locals are copied. In that case, two locals have the
// same value in a range, and we can pick which of the two to use. For
// example, in
//
//  (if (result i32)
//   (local.tee $x
//    (local.get $y)
//   )
//   (i32.const 100)
//   (local.get $x)
//  )
//
// If that assignment of $y is never used again, everything is fine. But if
// if is, then the live range of $y does not end in that get, and will
// necessarily overlap with that of $x - making them appear to interfere
// with each other in coalesce-locals, even though the value is identical.
//
// To fix that, we replace uses of $y with uses of $x. This extends $x's
// live range and shrinks $y's live range. This tradeoff is not always good,
// but $x and $y definitely overlap already, so trying to shrink the overlap
// makes sense - if we remove the overlap entirely, we may be able to let
// $x and $y be coalesced later.
//
// If we can remove only some of $y's uses, then we are definitely not
// removing the overlap, and they do conflict. In that case, it's not clear
// if this is beneficial or not, and we don't do it for now
// TODO: investigate more
//

#include <iostream>
#include <ir/local-graph.h>
#include <pass.h>
#include <wasm-builder.h>
#include <wasm.h>

namespace wasm {

struct UseBulkMemoryIntrinsics
  : public WalkerPass<
      PostWalker<UseBulkMemoryIntrinsics, UnifiedExpressionVisitor<UseBulkMemoryIntrinsics>>> {
  bool isFunctionParallel() override { return true; }

  Pass* create() override { return new UseBulkMemoryIntrinsics(); }

  void doWalkFunction(Function* func) {
    if (func->name.hasSubstring("memcpy")) {
      func->body->dump();
      std::cout << "memcpy!" << std::endl;

    Builder builder(*getModule());
    Block *body = (Block *)func->body;

    body->list.clear();
    body->list.push_back(builder.makeMemoryCopy(builder.makeLocalGet(0, Type::i32),
                                          builder.makeLocalGet(1, Type::i32),
                                          builder.makeLocalGet(2, Type::i32)));
    body->list.push_back(builder.makeReturn(builder.makeLocalGet(0, Type::i32)));

    //replaceCurrent(builder.makeFunction(func->name, func->type, body));
      //void *memcpy(void *dest, const void * src, size_t n);
    }

    if (func->name.hasSubstring("memset")) {
      std::cout << "memset!" << std::endl;
    func->body->dump();
    Builder builder(*getModule());
    Block *body = (Block *)func->body;

    body->list.clear();
    body->list.push_back(builder.makeMemoryFill(builder.makeLocalGet(0, Type::i32),
                                          builder.makeLocalGet(1, Type::i32),
                                          builder.makeLocalGet(2, Type::i32)));
    body->list.push_back(builder.makeReturn(builder.makeLocalGet(0, Type::i32)));

    }
  }

};

Pass* createUseBulkMemoryIntrinsicsPass() { return new UseBulkMemoryIntrinsics(); }

} // namespace wasm
