#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <unordered_map>
#include <vector>
using namespace llvm;

static cl::opt<bool>
    Triangular("tri", cl::desc("Enable Printing Triangular Loops Count"));

static cl::opt<bool>
    ArrRef("arr-ref",
           cl::desc("Enable Counting Array References and Dimensionality"));

static cl::opt<bool> Scalars(
    "scalars",
    cl::desc("Enable Counting All Scalar References inside loop nests"));

static cl::opt<bool>
    ArrIdx("arr-idx", cl::desc("Enable Printing Array Index Expressions"));

static cl::opt<bool>
    BinOps("bin-ops", cl::desc("Enable Printing Binary Operations Frequency"));

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

  void printOpMap(const std::unordered_map<std::string, int> &opMap) {
    errs() << "Operation: Frequency in Loop nest\n";
    for (auto const &pair : opMap) {
      errs() << pair.first << " : " << pair.second << "\n";
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

  // A function that recursively visits binary operation instructions.
  // Starts with one binop instr, and inspects its operands. If any of them
  // is the result of another binop inst, it visits that other instruction and
  // so on, until the level where the operands of the binops instruction are
  // either constants, induction variables, or parametric vars. During these
  // visits, it collects `localStats` as follows:
  //        localStats[0] ==> Number of induction variables visited
  //        localStats[1] ==> Number of constants
  //        localStats[2] ==> Number of parametric vars

  void visitBinOpInstr(Instruction *BinOpInstr, int (&localStats)[3]) {

    for (int i = 0; i < BinOpInstr->getNumOperands(); ++i) {

      Value *currentOperand = BinOpInstr->getOperand(i);

      if (isa<BinaryOperator>(currentOperand)) {

        Instruction *currentOperandAsInstr = cast<Instruction>(currentOperand);
        visitBinOpInstr(currentOperandAsInstr, localStats);

      } else {

        if (isa<ConstantData>(currentOperand)) {

          // Increment number of constants in the expression
          localStats[1]++;
        } else {

          if (isa<PHINode>(currentOperand)) {

            // Increment PHINodes count
            localStats[0]++;
          } else {

            // Increment Parametric
            localStats[2]++;
          }
        }
      }
    }
  }

  int findArrayRefs(
      Loop *L,
      std::unordered_map<std::string, std::pair<int, std::string>> &refMap) {

    // A map to store binary operations names and frequency
    std::unordered_map<std::string, int> binOps;

    // A counter to store the number of conditionals
    int conditionals = 0;

    // An array that stores 4 values for different array access types:
    //      [0] ==> Linear Expressions (e.g. array[i])
    //      [1] ==> Constant Shift (e.g. array[i+1])
    //      [2] ==> Parametric Shift (e.g. array[i+M])
    //      [3] ==> Skewed (e.g. array[i+j]):
    int idxExpressionCounter[4] = {0};

    int refCount = 0;

    // Loop latch compare instructions (used while counting conditionals)
    ICmpInst *latchCmp = L->getLatchCmpInst();

    auto LoopBlocks = L->getBlocks();
    for (BasicBlock *BB : LoopBlocks) {
      for (Instruction &I : *BB) {
        bool usesArray = false;
        Instruction *Ip = &I;

        // Count binary operators (add, sub, div, etc)
        if (isa<BinaryOperator>(Ip)) {
          std::string code = Ip->getOpcodeName();
          if (binOps.find(code) == binOps.end()) {
            binOps.emplace(std::make_pair(code, 1));
          } else {
            ++binOps[code];
          }
          // errs() << "Instruction opCode is : " << Ip->getOpcodeName() <<
          // "\n";
        }

        // Count conditionals
        // The idea is to find basic block terminators and check the first
        // operand if the first operand is a compare instruction (icmp, cmp,
        // etc.), then it is a conditional. Exclude the loop latch compare
        // instruction
        //
        // TODO: exclude nested loops compare instruction as well
        if (Ip->isTerminator()) {
          if (isa<CmpInst>(Ip->getOperand(0))) {
            Instruction *conditional = dyn_cast<Instruction>(Ip->getOperand(0));
            if (conditional != latchCmp) {
              conditionals++;
            }
          }
        }
        if (isa<GetElementPtrInst>(Ip)) {

          // This variable is used to know what should be the increment for
          // the index expression
          // For example: if we have a[i+1] += 5;
          // then we will count the index expression [i+1] twice as a constant
          // shift expression
          int idxExpressionCountStep = 0;

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
              ++idxExpressionCountStep;
            }
          }
          if (ArrIdx) {
            int gepNumOperands = Ip->getNumOperands();
            // errs() << "GEP instruction is " << *Ip << "\n";

            // TODO Analyze GEP Instruction to find the index type
            //
            // Get the GEP index expression (operand)
            Value *gepOperand = (Ip->getOperand(gepNumOperands - 1));

            // Check if the GEP expression is an instruction
            if (isa<Instruction>(gepOperand)) {
              // errs() << "GEP operand is an instruction \n";

              // If it is, cast it to Instruction
              Instruction *gepOperandI = cast<Instruction>(gepOperand);

              // Now, analyze this instruction
              // First, get the number of operands of that instruction
              // (it is noticed that this instruction is usually a sext
              // instr which has only one operand)
              int idxInstrNumOperands = gepOperandI->getNumOperands();
              for (int i = 0; i < idxInstrNumOperands; ++i) {
                // Get the operand of the instruction (sext instr.)
                Value *gepOperandIOperand = gepOperandI->getOperand(i);

                // If this operand is a Phi Node, most likely it is
                // the induction variable of the loop (i or j, etc.)
                if (isa<PHINode>(gepOperandIOperand)) {
                  // errs() << "Opernad Is a PHI node\n";
                  //  Increment Linear Expressions Counter
                  idxExpressionCounter[0] += idxExpressionCountStep;
                } else if (isa<BinaryOperator>(gepOperandIOperand)) {
                  Instruction *binaryIdxInstr =
                      cast<Instruction>(gepOperandIOperand);

                  // This is an array which will collect local stats inside
                  // recursive calls of visitBinOpInstr The indices store values
                  // as follows:
                  //      localStats[0] ==> The number of induction variables
                  //      localStats[1] ==> The number of constants
                  //      localStats[2] ==> The number of parametric vars
                  int localStats[] = {0, 0, 0};

                  // This is the function that visit the binary operation
                  // instruction of the index expression and recursively visit
                  // its operands if they're binary operations as well to
                  // identify the index access expression
                  //
                  // For example, if GEP operand was %idxprom
                  // and
                  // %idxprom = add nsw i32 %add4, %add3 --> (1)
                  //
                  // This function starts by visiting instr (1), then looks at
                  // the operands (%add4 , %add3). If either (or both) are also
                  // binary operations, it visit each of them recursively, and
                  // so on until we reach the last level, where the operand are
                  // no longer values produced by other binary operators
                  // (constants, induction variables, or parametric variables)
                  visitBinOpInstr(binaryIdxInstr, localStats);

                  // Now, parse local stats collected during the recursive calls
                  // and reflect them into the global stats

                  if (localStats[0] > 1) {
                    idxExpressionCounter[3] += idxExpressionCountStep;
                  }

                  if (localStats[1] > 0) {
                    idxExpressionCounter[1] += idxExpressionCountStep;
                  }
                  if (localStats[2] > 0) {
                    idxExpressionCounter[2] += idxExpressionCountStep;
                  }
                }

                errs() << "Operand " << i << " : "
                       << *(gepOperandI->getOperand(i)) << "\n";
              }
            }

            errs() << "array: " << arrayName
                   << " , index: " << *(Ip->getOperand(gepNumOperands - 1))
                   << "\n";
          }
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
          if (Scalars) {
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
    }

    if (ArrIdx)
      printIdxExpSummary(idxExpressionCounter);

    if (BinOps)
      printOpMap(binOps);
    return refCount;
  }

  void printIdxExpSummary(int (&idxExpressionCounter)[4]) {

    errs() << "\nLoop Nest Array Access Pattern Summary\n=================\n";

    errs() << "Linear Expressions: " << idxExpressionCounter[0] << "\n";
    errs() << "Constant Shift Expressions: " << idxExpressionCounter[1] << "\n";
    errs() << "Parametric Shift Expressions: " << idxExpressionCounter[2]
           << "\n";
    errs() << "Skewed Shift Expressions: " << idxExpressionCounter[3] << "\n\n";
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
      if (ArrRef) {
        errs() << "Number of Array References: " << loopArrRefs << "\n";
        printMap(refMap);
      }
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

    if (Triangular)
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
