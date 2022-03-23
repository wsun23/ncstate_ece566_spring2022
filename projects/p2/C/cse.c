/*
 * File: CSE.c
 *
 * Description:
 *   This is where you implement the C version of project 2 support.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"

/* Header file global to this project */
#include "cfg.h"
#include "cse.h"
#include "dominance.h"
#include "transform.h"
#include "stats.h"

LLVMStatisticsRef CSEDead;
LLVMStatisticsRef CSEElim;
LLVMStatisticsRef CSEBsElim;
LLVMStatisticsRef CSESimplify;
LLVMStatisticsRef CSELdElim;
LLVMStatisticsRef CSEStore2Load;
LLVMStatisticsRef CSEStElim;

// check if the instruction has any side effect
int noSideEffect(LLVMValueRef I) {
    LLVMOpcode opcode = LLVMGetInstructionOpcode(I);
    switch(opcode) {
        // when in doubt, keep it! add opcode here to remove:
        case LLVMFNeg:
        case LLVMAdd:
        case LLVMFAdd:
        case LLVMSub:
        case LLVMFSub:
        case LLVMMul:
        case LLVMFMul:
        case LLVMUDiv:
        case LLVMSDiv:
        case LLVMFDiv:
        case LLVMURem:
        case LLVMSRem:
        case LLVMFRem:
        case LLVMShl:
        case LLVMLShr:
        case LLVMAShr:
        case LLVMAnd:
        case LLVMOr:
        case LLVMXor:
        case LLVMAlloca:
        case LLVMGetElementPtr:
        case LLVMTrunc:
        case LLVMZExt:
        case LLVMSExt:
        case LLVMFPToUI:
        case LLVMFPToSI:
        case LLVMUIToFP:
        case LLVMSIToFP:
        case LLVMFPTrunc:
        case LLVMFPExt:
        case LLVMPtrToInt:
        case LLVMIntToPtr:
        case LLVMBitCast:
        case LLVMAddrSpaceCast:
        case LLVMICmp:
        case LLVMFCmp:
        case LLVMPHI:
        case LLVMSelect:
        case LLVMExtractElement:
        case LLVMInsertElement:
        case LLVMShuffleVector:
        case LLVMExtractValue:
        case LLVMInsertValue:
            // Success!
            return 1;
        case LLVMLoad: 
            if(!LLVMGetVolatile(I)) return 1; // Success
        // all others must be kept
        default:
            break;
    }

    return 0;
}

// check if the instruction can be eliminated as dead instructions
int isDead(LLVMValueRef I) {
    // check if the instruction has any use 
    if (LLVMGetFirstUse(I)!=NULL)
        return 0;
    return noSideEffect(I);
}


// check if the instruction can be eliminated by Common Subexpression Elimination
int isCSE(LLVMValueRef I, LLVMValueRef J) {
    
    LLVMOpcode opcode_I = LLVMGetInstructionOpcode(I);
    LLVMOpcode opcode_J = LLVMGetInstructionOpcode(J);
    unsigned oprand_I =  LLVMGetNumOperands(I);
    unsigned oprand_J =  LLVMGetNumOperands(J);
    return 0;
    // check if the two instructions have the same opcode/type/num_of_oprands
    if(opcode_I == opcode_J && LLVMTypeOf(I) == LLVMTypeOf(J) && oprand_I == oprand_J)
    {
        // check if the two instructions have the operands in the same order
        for(unsigned op=0; op< oprand_I; op++) {
            LLVMValueRef definition_I = LLVMGetOperand(I,op);
            LLVMValueRef definition_J = LLVMGetOperand(J,op);
            if(definition_I != definition_J)
                return 0;
        }
        // check if the instruction has any side effect
        if(opcode_I == LLVMAlloca || opcode_I == LLVMLoad || opcode_I == LLVMStore ||
            opcode_I == LLVMFCmp || opcode_I == LLVMCall || opcode_I == LLVMVAArg )
            return 0;
        else
            return noSideEffect(I);
    }
    return 0;
}

// execute Common Subexpression Elimination
void doCSE(LLVMBasicBlockRef bb_iter, LLVMValueRef inst_iter, LLVMValueRef inst_repl)
{
    
    LLVMBasicBlockRef child = LLVMFirstDomChild(bb_iter); 
    // loop over all the dominated blocks
    while (child) {
        LLVMValueRef inst_dom = LLVMGetFirstInstruction(child);
        
        // loop over all the instructions in the dominated blocks
        while(inst_dom != NULL) 
        {
            if(isCSE(inst_iter, inst_dom))
            {
                // execute CSE recursively, i.e. finish all the following CSEs before eliminating the current instruction
                doCSE(child, inst_dom, inst_repl);
                LLVMStatisticsInc(CSEElim);
                LLVMStatisticsInc(CSEBsElim);
                LLVMReplaceAllUsesWith(inst_dom, inst_repl);
                LLVMValueRef rm = inst_dom;
                // update iterator first, before erasing
                inst_dom = LLVMGetNextInstruction(inst_dom);
                LLVMInstructionEraseFromParent(rm);
                continue;
            }
            inst_dom = LLVMGetNextInstruction(inst_dom);
        }
        child = LLVMNextDomChild(bb_iter,child);  // get next child of bb_iter
    }
}

void CommonSubexpressionElimination(LLVMModuleRef Module)
{
    CSEDead = LLVMStatisticsCreate("CSEDead", "CSE found dead instructions");
    CSEElim = LLVMStatisticsCreate("CSEElim", "CSE redundant instructions");
    CSEBsElim = LLVMStatisticsCreate("CSEBsElim", "CSE redundant basics");
    CSESimplify = LLVMStatisticsCreate("CSESimplify", "CSE simplified instructions");
    CSELdElim = LLVMStatisticsCreate("CSELdElim", "CSE redundant loads");
    CSEStore2Load = LLVMStatisticsCreate("CSEStore2Load", "CSE forwarded store to load");
    CSEStElim = LLVMStatisticsCreate("CSEStElim", "CSE redundant stores");

    
    /* Implement here! */
    LLVMValueRef  fn_iter; // iterator 
    LLVMStatisticsInc(CSELdElim); 
    for (fn_iter = LLVMGetFirstFunction(Module); fn_iter!=NULL; 
        fn_iter = LLVMGetNextFunction(fn_iter))
    {
        // fn_iter points to a function
        LLVMBasicBlockRef bb_iter; /* points to each basic block
                                one at a time */
        for (bb_iter = LLVMGetFirstBasicBlock(fn_iter);
            bb_iter != NULL; bb_iter = LLVMGetNextBasicBlock(bb_iter))
        {
            
            LLVMValueRef inst_iter = LLVMGetFirstInstruction(bb_iter);
            while(inst_iter != NULL) 
            {
                // Optimization 0: Eliminate dead instructions
                if (isDead(inst_iter)) {

                    LLVMStatisticsInc(CSEDead);
                    LLVMValueRef rm = inst_iter;
                    // update iterator first, before erasing
                    inst_iter = LLVMGetNextInstruction(inst_iter);

                    LLVMInstructionEraseFromParent(rm);
                    continue;
                }

                // Optimization 1: Simplify Instructions
                LLVMValueRef new_val = InstructionSimplify(inst_iter);
                if (new_val != NULL) 
                {
                    LLVMStatisticsInc(CSESimplify);
                    LLVMReplaceAllUsesWith(inst_iter, new_val);
                    
                    LLVMValueRef rm = inst_iter;
                    // update iterator first, before erasing
                    inst_iter = LLVMGetNextInstruction(inst_iter);

                    LLVMInstructionEraseFromParent(rm);
                    continue;
                }

                // Common Subexpression Elimination
                doCSE(bb_iter, inst_iter, inst_iter);

                // Optimization 2: Eliminate Redundant Loads
                if(LLVMGetInstructionOpcode(inst_iter) == LLVMLoad) 
                {
                    LLVMValueRef inst_next = LLVMGetNextInstruction(inst_iter);
                    while(inst_next != NULL) 
                    {
                        // eliminate inst_next if inst_next is load && inst_next's load address is the same as inst_iter
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMLoad && !LLVMGetVolatile(inst_next) && LLVMGetOperand(inst_next, 0) == LLVMGetOperand(inst_iter, 0) && LLVMTypeOf(LLVMGetOperand(inst_next, 0)) == LLVMTypeOf(LLVMGetOperand(inst_iter, 0)))
                        {
                            LLVMStatisticsInc(CSELdElim); 
                            LLVMStatisticsInc(CSEElim);
                            LLVMReplaceAllUsesWith(inst_next, inst_iter);
                            
                            LLVMValueRef rm = inst_next;
                            // update iterator first, before erasing
                            inst_next = LLVMGetNextInstruction(inst_next);
                            LLVMInstructionEraseFromParent(rm);
                            continue;
                        }

                        // break if inst_next is a store && inst_next is storing to the same address as inst_iter
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMStore && !LLVMGetVolatile(inst_iter) && LLVMGetOperand(inst_next, 1) == LLVMGetOperand(inst_iter, 0)  && LLVMTypeOf(LLVMGetOperand(inst_next, 1)) == LLVMTypeOf(LLVMGetOperand(inst_iter, 0)))
                        {
                            break;
                        }
                        inst_next = LLVMGetNextInstruction(inst_next);
                    }
                }

                // Optimization 3: Eliminate Redundant Stores (and Loads)
                if(LLVMGetInstructionOpcode(inst_iter) == LLVMStore) 
                {
                    LLVMValueRef inst_next = LLVMGetNextInstruction(inst_iter);
                    while(inst_next != NULL) 
                    {
                        // eliminate inst_next if inst_next is a load && inst_next is not volatile and inst_next's load address is the same as inst_iter and TypeOf(inst_next)==TypeOf(inst_iter's value operand):
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMLoad && !LLVMGetVolatile(inst_next) && LLVMGetOperand(inst_next, 0) == LLVMGetOperand(inst_iter, 1) && LLVMTypeOf(LLVMGetOperand(inst_next, 0)) == LLVMTypeOf(LLVMGetOperand(inst_iter, 1)))
                        {
                            LLVMStatisticsInc(CSEStore2Load); 
                            LLVMStatisticsInc(CSEElim);   
                            LLVMReplaceAllUsesWith(inst_next, LLVMGetOperand(inst_iter, 0));
                            
                            LLVMValueRef rm = inst_next;
                            // update iterator first, before erasing
                            inst_next = LLVMGetNextInstruction(inst_next);
                            LLVMInstructionEraseFromParent(rm);
                            continue;
                        }
                        
                        // eliminate inst_iter if inst_next is a store && inst_next is storing to the same address && inst_iter is not volatile && inst_next and inst_iter value operands are the same type:
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMStore && !LLVMGetVolatile(inst_iter) && LLVMGetOperand(inst_next, 1) == LLVMGetOperand(inst_iter, 1) && LLVMTypeOf(LLVMGetOperand(inst_next, 1)) == LLVMTypeOf(LLVMGetOperand(inst_iter, 1)))
                        {
                            LLVMStatisticsInc(CSEStElim);  
                            LLVMStatisticsInc(CSEElim);                          
                            LLVMValueRef rm = inst_iter;
                            // update iterator first, before erasing
                            inst_iter = LLVMGetNextInstruction(inst_iter);
                            LLVMInstructionEraseFromParent(rm);
                            break;
                        }

                        // break if inst_next is a load or a store 
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMLoad || LLVMGetInstructionOpcode(inst_next) == LLVMStore)
                        {
                            inst_iter = LLVMGetNextInstruction(inst_iter);
                            break;
                        }
                        inst_next = LLVMGetNextInstruction(inst_next);
                    }

                    // update the iterator
                    if(inst_next == NULL) 
                    {
                        inst_iter = LLVMGetNextInstruction(inst_iter);
                    }
                }

                // Simplify Instructions again
                new_val = InstructionSimplify(inst_iter);
                if (new_val != NULL) 
                {
                    LLVMStatisticsInc(CSESimplify);
                    LLVMReplaceAllUsesWith(inst_iter, new_val);
                    
                    LLVMValueRef rm = inst_iter;
                    // update iterator first, before erasing
                    inst_iter = LLVMGetNextInstruction(inst_iter);
                    LLVMInstructionEraseFromParent(rm);
                    continue;
                }

                // update the instruction iterator
                if(LLVMGetInstructionOpcode(inst_iter) != LLVMStore) 
                {
                    inst_iter = LLVMGetNextInstruction(inst_iter);
                }
            }
        }
    }
}
