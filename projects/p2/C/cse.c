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
LLVMStatisticsRef CSESimplify;
LLVMStatisticsRef CSELdElim;
LLVMStatisticsRef CSEStore2Load;
LLVMStatisticsRef CSEStElim;

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


int isDead(LLVMValueRef I) {
    if (LLVMGetFirstUse(I)!=NULL)
        return 0;
    return noSideEffect(I);
}


int isCSE(LLVMValueRef I, LLVMValueRef J) {
    
    LLVMOpcode opcode_I = LLVMGetInstructionOpcode(I);
    LLVMOpcode opcode_J = LLVMGetInstructionOpcode(J);
    unsigned oprand_I =  LLVMGetNumOperands(I);
    unsigned oprand_J =  LLVMGetNumOperands(J);
    return 0;


    if(opcode_I == opcode_J && LLVMTypeOf(I) == LLVMTypeOf(J) && oprand_I == oprand_J)
    {
        for(unsigned op=0; op< oprand_I; op++) {
            LLVMValueRef definition_I = LLVMGetOperand(I,op);
            LLVMValueRef definition_J = LLVMGetOperand(J,op);
            if(definition_I != definition_J)
                return 0;
        }

        switch(opcode_I) {
            // when in doubt, keep it! add opcode here to remove:
            // case LLVMRet:
            // case LLVMBr:
            // case LLVMSwitch:
            // case LLVMIndirectBr:
            // case LLVMInvoke:
            // case LLVMUnreachable:
            // case LLVMCallBr:
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
            // case LLVMAlloca:
            // case LLVMLoad:
            // case LLVMStore:
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
            // case LLVMFCmp:
            case LLVMPHI:
            // case LLVMCall:
            case LLVMSelect:
            case LLVMUserOp1:
            case LLVMUserOp2:
            // case LLVMVAArg:
            case LLVMExtractElement:
            case LLVMInsertElement:
            case LLVMShuffleVector:
            case LLVMExtractValue:
            case LLVMInsertValue:
            // case LLVMFreeze:
            // case LLVMFence:
            // case LLVMAtomicCmpXchg:
            // case LLVMAtomicRMW:
            // case LLVMResume:
            // case LLVMLandingPad:
            // case LLVMCleanupRet:
            // case LLVMCatchRet:
            // case LLVMCatchPad:
            // case LLVMCleanupPad:
            // case LLVMCatchSwitch:
                // Success!
                return 1;
            // all others must be kept
            default:
                break;
        }
    }
    return 0;
}

void doCSE(LLVMBasicBlockRef bb_iter, LLVMValueRef inst_iter, LLVMValueRef inst_repl)
{
    LLVMBasicBlockRef child = LLVMFirstDomChild(bb_iter); 
    while (child) {
        LLVMValueRef inst_dom = LLVMGetFirstInstruction(child);
        while(inst_dom != NULL) 
        {
            if(isCSE(inst_iter, inst_dom))
            {
                doCSE(child, inst_dom, inst_repl);
                LLVMStatisticsInc(CSEElim);
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
    CSESimplify = LLVMStatisticsCreate("CSESimplify", "CSE simplified instructions");
    CSELdElim = LLVMStatisticsCreate("CSELdElim", "CSE redundant loads");
    CSEStore2Load = LLVMStatisticsCreate("CSEStore2Load", "CSE forwarded store to load");
    CSEStElim = LLVMStatisticsCreate("CSEStElim", "CSE redundant stores");

    /* Implement here! */
    LLVMValueRef  fn_iter; // iterator 
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

                // if(LLVMGetInstructionOpcode(inst_iter) ==  LLVMLoad) {
                //     fprintf(stderr,"\nLLVMLoad!");
                // }
                // if(LLVMGetInstructionOpcode(inst_iter) ==  LLVMStore) {
                //     fprintf(stderr,"\nLLVMStore!");
                // }
                // fprintf(stderr,"\nNumber of def is: %d", LLVMGetNumOperands(inst_iter));
                // for(unsigned op=0; op< LLVMGetNumOperands(inst_iter); op++) {
                //     LLVMValueRef definition = LLVMGetOperand(inst_iter,op);
                //     fprintf(stderr,"\nDefinition of op=%d is:\n",op);
                //     LLVMDumpValue(definition);
                // }

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
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMLoad && !LLVMGetVolatile(inst_next) && LLVMGetOperand(inst_next, 0) == LLVMGetOperand(inst_iter, 0) && LLVMTypeOf(inst_next) == LLVMTypeOf(inst_iter))
                        {
                            LLVMStatisticsInc(CSELdElim);
                            LLVMReplaceAllUsesWith(inst_next, inst_iter);
                            
                            LLVMValueRef rm = inst_next;
                            // update iterator first, before erasing
                            inst_next = LLVMGetNextInstruction(inst_next);

                            LLVMInstructionEraseFromParent(rm);
                            continue;
                        }
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMStore)
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
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMLoad && !LLVMGetVolatile(inst_next) && LLVMGetOperand(inst_next, 0) == LLVMGetOperand(inst_iter, 1) && LLVMTypeOf(inst_next) == LLVMTypeOf(inst_iter))
                        {
                            LLVMStatisticsInc(CSEStore2Load);
                            LLVMReplaceAllUsesWith(inst_next, inst_iter);
                            
                            LLVMValueRef rm = inst_next;
                            // update iterator first, before erasing
                            inst_next = LLVMGetNextInstruction(inst_next);

                            LLVMInstructionEraseFromParent(rm);
                            continue;
                        }
                        
                        if(LLVMGetInstructionOpcode(inst_next) == LLVMStore && LLVMGetOperand(inst_next, 1) == LLVMGetOperand(inst_iter, 1) && !LLVMGetVolatile(inst_iter) && LLVMTypeOf(inst_next) == LLVMTypeOf(inst_iter))
                        {
                            LLVMStatisticsInc(CSEStElim);                            
                            LLVMValueRef rm = inst_iter;
                            // update iterator first, before erasing
                            inst_iter = LLVMGetNextInstruction(inst_iter);

                            LLVMInstructionEraseFromParent(rm);
                            break;
                        }

                        if(!noSideEffect(inst_next))
                        {
                            inst_iter = LLVMGetNextInstruction(inst_iter);
                            break;
                        }
                        inst_next = LLVMGetNextInstruction(inst_next);
                    }
                    if(inst_next == NULL) 
                    {
                        inst_iter = LLVMGetNextInstruction(inst_iter);
                    }
                }

                if(LLVMGetInstructionOpcode(inst_iter) != LLVMStore) 
                {
                    inst_iter = LLVMGetNextInstruction(inst_iter);
                }
            }
        }
    }

    LLVMStatisticsInc(CSEElim);
    LLVMStatisticsInc(CSELdElim);
    LLVMStatisticsInc(CSEStore2Load);
    LLVMStatisticsInc(CSEStElim); 
}
