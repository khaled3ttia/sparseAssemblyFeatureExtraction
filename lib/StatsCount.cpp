#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <unordered_map>
#include <vector>
using namespace llvm;
namespace {

struct StatsCount : public FunctionPass {
  static char ID;
  StatsCount() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.setPreservesAll();
  }

  void printMap(
      std::unordered_map<std::string, std::pair<int, std::string>> refMap) {

    errs() << "Name : Number of Refs : Size and Type\n";
    for (auto const &pair : refMap) {

      errs() << pair.first << " : " << pair.second.first << " : "
             << pair.second.second << "\n";
    }
  }

  bool instInLoop(Loop *L, Instruction *I) {

    if (L->contains(I))
      return true;
    for (auto subLoop : L->getSubLoops())
      if (subLoop->contains(I))
        return true;

    return false;
  }

  int findArrayRefs(
      Loop *L,
      std::unordered_map<std::string, std::pair<int, std::string>> &refMap) {

    int refCount = 0;
    auto LoopBlocks = L->getBlocks();
    for (BasicBlock *BB : LoopBlocks) {
      for (Instruction &I : *BB) {
        bool usesArray = false;
        Instruction *Ip = &I;
        if (isa<GetElementPtrInst>(Ip)) {

          // Get the name of the array
          std::string arrayName = (*(Ip->getOperand(0))).getName().str();

          Type *T = cast<PointerType>(
                        cast<GetElementPtrInst>(Ip)->getPointerOperandType())
                        ->getElementType();

          if (isa<ArrayType>(T)) {

            // errs() << "Operand 0: " << (*(Ip->getOperand(0))).getName() <<
            // "\n"; errs() << "T is " << *T << "\n";

            // convert type to string
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            T->print(rso);
            // std::cout << rso.str();

            refMap.emplace(
                std::make_pair(arrayName, std::make_pair(0, type_str)));
            for (User *U : Ip->users()) {
              // errs() << "User " << *U << "\n";
              if (instInLoop(L, cast<Instruction>(U)))
                refCount++;
              ++(refMap[arrayName].first);
            }
          }

          int gepNumOperands = Ip->getNumOperands();
          errs() << "array: " << arrayName << " , index: " << *(Ip->getOperand(gepNumOperands-1)) << "\n";

          /*
          // GEP operands
          for (int k = 0; k < Ip->getNumOperands(); ++k) {
            errs() << "Operand " << k << ":" << *(Ip->getOperand(k)) << "\n";
          }
          */

          /*
          Type *T =
          cast<PointerType>(cast<GetElementPtrInst>(Ip)->getPointerOperandType())->getElementType();
          int numElements = cast<ArrayType>(T)->getNumElements();
          errs() << "T is: " << *T << " and numElements is: " << numElements<<
          "\n";
          */
        } else {
          int numOperands = Ip->getNumOperands();
          for (int i = 0; i < numOperands; ++i) {
            Value *op = Ip->getOperand(i);
            Type *opTy = op->getType();

            if (!(opTy->isLabelTy() || opTy->isArrayTy() ||
                  opTy->isPointerTy() || opTy->isFunctionTy() ||
                  opTy->isMetadataTy())) {
              // if (!isa<label>(op){

              errs() << "Scalar " << i << (*(Ip->getOperand(i))).getName()
                     << " : ";
              errs() << *(op->getType()) << "\n";
            }
            //  }
          }
        }
      }
    }
    return refCount;
  }

  void countBlocksInLoop(Loop *L, unsigned nesting) {

    /*
    unsigned numBlocks = 0;
    Loop::block_iterator bb;
    for (bb = L->block_begin(); bb != L->block_end(); ++bb){

        numBlocks++;

    }
    errs() << "Loop level " << nesting << " has " << numBlocks << " blocks\n";
    */

    std::vector<Loop *> subLoops = L->getSubLoops();
    Loop::iterator j, f;
    for (j = subLoops.begin(), f = subLoops.end(); j != f; ++j) {
      countBlocksInLoop(*j, nesting + 1);
    }
  }

  bool containsUnsafeInstructions(BasicBlock *BB) {
    return any_of(*BB, [](const Instruction &I) {
      return I.mayHaveSideEffects() || I.mayReadFromMemory();
    });
  }

  // Check if two loops are tightly nested
  bool tightlyNested(Loop *OuterLoop, Loop *InnerLoop) {

    BasicBlock *OuterLoopHeader = OuterLoop->getHeader();
    BasicBlock *InnerLoopPreHeader = InnerLoop->getLoopPreheader();
    BasicBlock *OuterLoopLatch = OuterLoop->getLoopLatch();

    BranchInst *OuterLoopHeaderBI =
        dyn_cast<BranchInst>(OuterLoopHeader->getTerminator());

    if (!OuterLoopHeaderBI)
      return false;
    for (BasicBlock *Succ : successors(OuterLoopHeaderBI))
      if (Succ != InnerLoopPreHeader && Succ != InnerLoop->getHeader() &&
          Succ != OuterLoopLatch)
        return false;

    if (containsUnsafeInstructions(OuterLoopHeader) ||
        containsUnsafeInstructions(OuterLoopLatch))
      return false;

    if (InnerLoopPreHeader != OuterLoopHeader &&
        containsUnsafeInstructions(InnerLoopPreHeader))
      return false;

    BasicBlock *InnerLoopExit = InnerLoop->getExitBlock();
    const BasicBlock &SuccInner =
        LoopNest::skipEmptyBlockUntil(InnerLoopExit, OuterLoopLatch);
    if (&SuccInner != OuterLoopLatch) {
      return false;
    }

    if (containsUnsafeInstructions(InnerLoopExit))
      return false;

    return true;
  }

  bool IsPathToIndVar(Value *V, PHINode *InnerInduction) {
    if (V == InnerInduction)
      return true;
    if (isa<Constant>(V))
      return true;
    Instruction *I = dyn_cast<Instruction>(V);
    if (!I)
      return false;
    if (isa<CastInst>(I))
      return IsPathToIndVar(I->getOperand(0), InnerInduction);
    if (isa<BinaryOperator>(I))
      return IsPathToIndVar(I->getOperand(0), InnerInduction) &&
             IsPathToIndVar(I->getOperand(1), InnerInduction);
    return false;
  }

  bool isTriangular(Loop *OuterLoop, Loop *InnerLoop, PHINode *InnerInduction,
                    ScalarEvolution *SE) {

    unsigned Num = InnerInduction->getNumOperands();
    BasicBlock *InnerLoopPreHeader = InnerLoop->getLoopPreheader();
    for (unsigned i = 0; i < Num; ++i) {
      Value *Val = InnerInduction->getOperand(i);
      if (isa<Constant>(Val))
        continue;
      Instruction *I = dyn_cast<Instruction>(Val);
      if (!I)
        // TODO re-check
        return false;

      unsigned IncomBlockIndx = PHINode::getIncomingValueNumForOperand(i);
      if (InnerInduction->getIncomingBlock(IncomBlockIndx) ==
              InnerLoopPreHeader &&
          !OuterLoop->isLoopInvariant(I)) {
        return true;
      }
    }

    BasicBlock *InnerLoopLatch = InnerLoop->getLoopLatch();
    BranchInst *InnerLoopLatchBI =
        dyn_cast<BranchInst>(InnerLoopLatch->getTerminator());

    if (!InnerLoopLatchBI->isConditional())
      return true;
    if (CmpInst *InnerLoopCmp =
            dyn_cast<CmpInst>(InnerLoopLatchBI->getCondition())) {

      Value *Op0 = InnerLoopCmp->getOperand(0);
      Value *Op1 = InnerLoopCmp->getOperand(1);

      Value *Left = nullptr;
      Value *Right = nullptr;

      if (IsPathToIndVar(Op0, InnerInduction) && !isa<Constant>(Op0)) {
        Left = Op0;
        Right = Op1;
      } else if (IsPathToIndVar(Op1, InnerInduction) && !isa<Constant>(Op1)) {
        Left = Op1;
        Right = Op0;
      }

      if (Left == nullptr)
        return true;

      const SCEV *S = SE->getSCEV(Right);
      if (!SE->isLoopInvariant(S, OuterLoop))
        return true;
    }
  }

  void analyzeLoopBounds(Loop *i, ScalarEvolution *se, int &triangularLoops) {
    // bool isTriangular = false;

    Optional<Loop::LoopBounds> bounds = i->getBounds(*se);
    if (!bounds.hasValue()) {
      errs() << "Could not get the bounds\n";
    } else {
      errs() << "Loop Direction: ";
      // Loop::LoopBounds fetchedBounds = bounds.getValue();
      // Loop::LoopBounds::Direction dir = fetchedBounds.getDirection();
      auto dir = bounds->getDirection();
      switch (dir) {
      case Loop::LoopBounds::Direction::Increasing:
        errs() << "Increasing"
               << "\n";
        break;
      case Loop::LoopBounds::Direction::Decreasing:
        errs() << "Decreasing"
               << "\n";
        break;
      case Loop::LoopBounds::Direction::Unknown:
        errs() << "Unknown"
               << "\n";
        break;
      default:
        errs() << "Cannot find direction \n";
      }

      // Value &initialValue = fetchedBounds.getInitialIVValue();
      Value &initialValue = bounds->getInitialIVValue();

      errs() << "Initial value is: " << initialValue << "\n";
      // errs() << "has name: " << initialValue.hasName() << "\n";

      // Value* stepValue = fetchedBounds.getStepValue();
      Value *stepValue = bounds->getStepValue();
      if (stepValue != nullptr) {
        errs() << "Step value is: " << *stepValue << "\n";

        Instruction &stepInstruction = bounds->getStepInst();

        errs() << "Step Instr is: " << stepInstruction << "\n";
      }

      // Induction Variable
      PHINode *IndVar = i->getInductionVariable(*se);

      Value &finalValue = bounds->getFinalIVValue();

      errs() << "Final value is: " << finalValue << "\n";
      /*
      if (initialValue.hasName() || finalValue.hasName()){
           isTriangular = true;
      }

      if (isTriangular){
           errs() << "This is a triangular loop\n";
           triangularLoops++;
      }else {
           errs() << "This is a rectangular loop\n";
      }
      */
    }
    errs() << "=============================\n";
  }

  virtual bool runOnFunction(Function &F) {

    // Get the containing module
    Module *mod = F.getParent();

    std::string dataLayout = mod->getDataLayoutStr();

    // errs() << "Data layout: " << dataLayout << "\n";

    errs() << "Function " << F.getName() << '\n';
    errs() << "-----------------\n";
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    ScalarEvolution *se = &getAnalysis<ScalarEvolutionWrapperPass>().getSE();

    int loopCounter = 0;
    int totalLoops = 0, nestedLoops = 0, disjointLoops = 0;
    double avgDepth = 0;
    int triangularLoops = 0;

    for (LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i) {
      loopCounter++;
      totalLoops++;
      disjointLoops++;
      avgDepth++;

      int loopDepth = 1;
      errs() << "Analyzing loop " << loopCounter << "\n";
      // countBlocksInLoop(*i, 0);
      std::vector<Loop *> subLoops = (*i)->getSubLoops();

      if (!subLoops.empty()) {
        totalLoops += subLoops.size();
        nestedLoops++;
        avgDepth += subLoops.size();
      }

      errs() << "Loop Depth: " << subLoops.size() + 1 << "\n";

      std::unordered_map<std::string, std::pair<int, std::string>> refMap;

      int loopArrRefs = findArrayRefs(*i, refMap);
      errs() << "Number of Array References: " << loopArrRefs << "\n";
      printMap(refMap);
      analyzeLoopBounds(*i, se, triangularLoops);

      Loop::iterator j, f;
      int nest = 0;
      for (j = subLoops.begin(), f = subLoops.end(); j != f; ++j) {
        nest++;
        errs() << "Analyzing loop nest " << nest << "\n";

        PHINode *indVar = (*j)->getInductionVariable(*se);
        if (indVar == nullptr) {
          BasicBlock *LoopHeader = (*j)->getHeader();
          for (auto &I : *LoopHeader) {
            if (PHINode *PN = dyn_cast<PHINode>(&I))
              indVar = PN;
          }
        }

        bool tr = isTriangular(*i, *j, indVar, se);
        if (tr) {
          triangularLoops++;
          errs() << "Triangular Loop\n";
        }

        analyzeLoopBounds(*j, se, triangularLoops);
        // bool x = tightlyNested(*i, *j);
        // errs() << (x?"Tightly nested":"Not tightly nested") << "\n";
        // errs() << "Induction Variable: " << (*indVar) << "\n";
      }
      // errs() << "Loop Depth: " << loopDepth << "\n";
      /*
      Optional<Loop::LoopBounds> bounds = (*i)->getBounds(*se);
      if (!bounds.hasValue()){
          errs() << "Could not get the bounds\n";
      }else{
          errs() << "Loop Direction: ";
          //Loop::LoopBounds fetchedBounds = bounds.getValue();
          //Loop::LoopBounds::Direction dir = fetchedBounds.getDirection();
          auto dir = bounds->getDirection();
          switch (dir){
              case Loop::LoopBounds::Direction::Increasing:
                  errs() << "Increasing" << "\n";
                  break;
              case Loop::LoopBounds::Direction::Decreasing:
                  errs() << "Decreasing" << "\n";
                  break;
              case Loop::LoopBounds::Direction::Unknown:
                  errs() << "Unknown" << "\n";
                  break;
              default:
                  errs() << "Cannot find direction \n";
          }

         //Value &initialValue = fetchedBounds.getInitialIVValue();
         Value &initialValue = bounds->getInitialIVValue();

         errs() << "Initial value is: " << initialValue << "\n";
         errs() << "has name: " << initialValue.hasName() << "\n";

         //Value* stepValue = fetchedBounds.getStepValue();
         Value* stepValue = bounds->getStepValue();
         if (stepValue != nullptr){
              errs() << "Step value is: " << *stepValue << "\n";

              Instruction &stepInstruction = bounds->getStepInst();

              errs() << "Step Instr is: " << stepInstruction << "\n";
         }

         // Induction Variable
         PHINode *IndVar = (*i)->getInductionVariable(*se);


         Value &finalValue = bounds->getFinalIVValue();

         errs() << "Final value is: " << finalValue << "\n";

      }
      */
    }
    errs() << "==============================================\n";
    errs() << "==============================================\n";
    errs() << "Total Loops: " << totalLoops << "\n";
    errs() << "Disjoint Loops Found: " << loopCounter << "\n";
    errs() << "Nested Loops: " << nestedLoops << "\n";
    errs() << "Triangular Loops: " << triangularLoops << "\n";
    errs() << "Rectangular Loops: " << nestedLoops - triangularLoops << "\n";
    errs() << "Average Loop Depth: " << avgDepth / loopCounter << "\n";
    errs() << "==============================================\n";
    errs() << "==============================================\n";

    return false;
  }
};
} // namespace
char StatsCount::ID = 0;
static RegisterPass<StatsCount> X("stCounter", "Khaled: Capture loop stats");
