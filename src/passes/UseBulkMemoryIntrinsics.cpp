
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

  struct MemSetRange {
    int baseLocalIndex = -1;
    int offsetBegin = 0; // inclusive
    int offsetEnd = 0; // exclusive
    int storedValueLocalIndex = 0;

    bool isValid() { return baseLocalIndex >= 0 && offsetBegin <= offsetEnd; }
    bool isEmpty() { return isValid() && offsetBegin == offsetEnd; }

    static MemSetRange merge(MemSetRange a, MemSetRange b) {
      if (!a.isValid() || !b.isValid() || a.baseLocalIndex != b.baseLocalIndex || a.storedValueLocalIndex != b.storedValueLocalIndex)
        return MemSetRange();
      if (a.offsetEnd == b.offsetBegin || a.offsetBegin == b.offsetEnd)
        return MemSetRange{a.baseLocalIndex, std::min(a.offsetBegin, b.offsetBegin),
                                      std::max(a.offsetEnd, b.offsetEnd), a.storedValueLocalIndex};
      return MemSetRange();
    }
  };

  std::pair<int, int> isLocalBasePlusConstOffset(Expression *expr) {
    if (LocalGet *directGet = expr->dynCast<LocalGet>()) {
      return {directGet->index, 0};
    }

    if (Binary *basePlusConstOffset = expr->dynCast<Binary>()) {
      if (basePlusConstOffset->op == AddInt32) {
        LocalGet *leftLocalGet = basePlusConstOffset->left->dynCast<LocalGet>();
        LocalGet *rightLocalGet = basePlusConstOffset->right->dynCast<LocalGet>();
        Const *leftConst = basePlusConstOffset->left->dynCast<Const>();
        Const *rightConst = basePlusConstOffset->right->dynCast<Const>();

        LocalGet *localGet = nullptr;
        Const *constant = nullptr;

        if (leftLocalGet && rightConst) {
          localGet = leftLocalGet;
          constant = rightConst;
        } else if (rightLocalGet && leftConst) {
          localGet = rightLocalGet;
          constant = leftConst;
        }
        else {
          return {-1, 0};
        }

        if (constant->value.type == Type::i32) {
          return {localGet->index, constant->value.geti32()};
        }
      }
    }
    return {-1, 0};
  }

  MemSetRange isMemSetOperation(Expression *expr) {

    if (Store *store = expr->dynCast<Store>()) {
      if (LocalGet *valueLocalGet = store->value->dynCast<LocalGet>()) {
        std::pair<int, int> storeBasePlusOffset = isLocalBasePlusConstOffset(store->ptr);
        if (storeBasePlusOffset.first >= 0) {
          std::cout << "ismemset: " << (int)valueLocalGet->index << storeBasePlusOffset.first << storeBasePlusOffset.second << std::endl;
          return {storeBasePlusOffset.first, storeBasePlusOffset.second, storeBasePlusOffset.second+1, (int)valueLocalGet->index};
        }
      }
    }

    if (MemoryFill *fill = expr->dynCast<MemoryFill>()) {
      if (LocalGet *valueLocalGet = fill->value->dynCast<LocalGet>()) {
        std::pair<int, int> storeBasePlusOffset = isLocalBasePlusConstOffset(fill->dest);
        if (Const *sizeConst = fill->size->dynCast<Const>()) {
          if (sizeConst->value.type == Type::i32) {
            int size = sizeConst->value.geti32();
            if (storeBasePlusOffset.first >= 0) {
              std::cout << "ismemset: " << (int)valueLocalGet->index << storeBasePlusOffset.first << storeBasePlusOffset.second << std::endl;
              return {storeBasePlusOffset.first, storeBasePlusOffset.second, storeBasePlusOffset.second+size, (int)valueLocalGet->index};
            }
          }
        }
      }

    }

    return {-1, 0, 0};
  }

  MemSetRange tryMerge(Expression *a, Expression *b) {
    auto range_a = isMemSetOperation(a);
    auto range_b = isMemSetOperation(b);

    return MemSetRange::merge(range_a, range_b);
  }

  void visitExpression(Expression *curr) {

    Builder builder(*getModule());

    if (Block *block = curr->dynCast<Block>()) {
      //std::cout << "storeExpression.." << std::endl;

      for (int i = 0; i < (int)block->list.size() - 1; ++i) {
        Expression *a = block->list[i];
        Expression *b = block->list[i + 1];
        MemSetRange merged = tryMerge(a, b);
        if (merged.isValid()) {
          std::cout << "merging into " << merged.offsetEnd - merged.offsetBegin << " long" << std::endl;

          block->list[i] = builder.makeMemoryFill(
          builder.makeBinary(BinaryOp::AddInt32,
            builder.makeLocalGet(merged.baseLocalIndex, Type::i32),
            builder.makeConst(merged.offsetBegin)
          ),
          builder.makeLocalGet(merged.storedValueLocalIndex, Type::i32),
          builder.makeConst(merged.offsetEnd-merged.offsetBegin));
          block->list.erase(block->list.begin() + i + 1);
          i--;
        }
      }
    }

  }

  void doWalkFunction(Function* func) {
    PostWalker::doWalkFunction(func);
/*
    Builder builder(*getModule());
    Block *body = (Block *)func->body;


    if (func->name.hasSubstring("memcpy")) {
      func->body->dump();
      std::cout << "memcpy!" << std::endl;



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
    */
  }

};

Pass* createUseBulkMemoryIntrinsicsPass() { return new UseBulkMemoryIntrinsics(); }

} // namespace wasm
