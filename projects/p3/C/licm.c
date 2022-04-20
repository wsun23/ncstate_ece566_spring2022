/*
 * File: licm.c
 *
 * Description:
 *   This is where you implement the C version of project 3 support.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* LLVM Header Files */
#include "llvm-c/Core.h"

/* Header file global to this project */
#include "cfg.h"
#include "licm.h"
#include "dominance.h"
#include "transform.h"
#include "stats.h"
#include "loop.h"

LLVMStatisticsRef LICMBasic;
LLVMStatisticsRef LICMLoadHoist;
LLVMStatisticsRef LICMNoPreheader;
LLVMStatisticsRef NumLoops;
LLVMStatisticsRef NumInst;
LLVMStatisticsRef NumBlock;
LLVMStatisticsRef NumLoopsNoStore;
LLVMStatisticsRef NumLoopsNoLoad;
LLVMStatisticsRef NumLoopsNoStoreWithLoad;
LLVMStatisticsRef NumLoopsWithCall;

LLVMStatisticsRef CSEDead;
LLVMStatisticsRef CSESimplify;

// check if the load instruction in the loop can be moved out of the loop
LLVMBool canMoveOutOfLoop(LLVMLoopRef L, LLVMValueRef I, worklist_t store_addr)
{
    LLVMValueRef addr = LLVMGetOperand(I, 0);
    if(LLVMGetVolatile(I)) return (LLVMBool)0;
    // check if there is no possible store to addr in the loop
    if(worklist_include(store_addr, addr)) return (LLVMBool)0;
    if(LLVMIsAGlobalVariable(addr)) return (LLVMBool)1;
    if(LLVMIsAAllocaInst(addr)) return (LLVMBool)1;
    return (LLVMBool)0;
}

void LICM(LLVMLoopInfoRef LIRef, LLVMLoopRef Loop, worklist_t loops_found)
{
    // collect the statistical data and the store addresses
    LLVMStatisticsInc(NumLoops);
    worklist_t list = LLVMGetBlocksInLoop(Loop);
    LLVMBool has_store = (LLVMBool)0;
    LLVMBool has_load = (LLVMBool)0;
    LLVMBool has_call = (LLVMBool)0;
    worklist_t store_addr = worklist_create();

    LLVMValueRef I_;
    LLVMBasicBlockRef bb_iter_;
    LLVMValueRef inst_iter_;
    while(!worklist_empty(list)) {
        I_ = worklist_pop(list);
        bb_iter_ = LLVMValueAsBasicBlock(I_);
        inst_iter_ = LLVMGetFirstInstruction(bb_iter_);
        while(inst_iter_ != NULL) 
        {
            //check is the instruction is load, store, or call
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMLoad) has_load = (LLVMBool)1;
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMStore)
            {
                // save the address of the store instructions in the loop
                worklist_insert(store_addr, LLVMGetOperand(inst_iter_, 1));
                has_store = (LLVMBool)1;
            }
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMCall) has_call = (LLVMBool)1;
            inst_iter_ = LLVMGetNextInstruction(inst_iter_);
        }
    }
    
    if(!has_store) LLVMStatisticsInc(NumLoopsNoStore);
    if(!has_load) LLVMStatisticsInc(NumLoopsNoLoad);
    if(!has_store && has_load) LLVMStatisticsInc(NumLoopsNoStoreWithLoad);
    if(has_call) LLVMStatisticsInc(NumLoopsWithCall);
    worklist_destroy(list);

    list = LLVMGetBlocksInLoop(Loop);
    // check if the loop has preheader
    LLVMBasicBlockRef PH = LLVMGetPreheader(Loop);
    if(PH == NULL)
    {
        LLVMStatisticsInc(LICMNoPreheader);
        return;
    } 

    while(!worklist_empty(list)) {
        // LLVMStatisticsInc(NumBlock);
        LLVMValueRef I = worklist_pop(list);
        LLVMBasicBlockRef bb_iter = LLVMValueAsBasicBlock(I);

        // check if the innermost_loop of block is same as the current loop 
        LLVMLoopRef inner_Loop = LLVMGetLoopRef(LIRef, bb_iter);
        if(inner_Loop != Loop)
        {
            // check if the innermost loop has already been optimized
            if(!worklist_include_BB(loops_found, bb_iter, LIRef))
            {
                worklist_insert(loops_found, LLVMBasicBlockAsValue(bb_iter));
                LICM(LIRef, inner_Loop, loops_found);
            }
            continue;
        }

        // optimize the blocks whose inner most loop is same as the current loop
        LLVMValueRef inst_iter = LLVMGetFirstInstruction(bb_iter);
        while(inst_iter != NULL) 
        {
            // move the instruction if it is not load or store and is loop invariant 
            if(!LLVMIsALoadInst(inst_iter) && !LLVMIsAStoreInst(inst_iter) && LLVMMakeLoopInvariant(Loop, inst_iter)) // LLVMIsValueLoopInvariant(Loop, inst_iter) && LLVMMakeLoopInvariant(Loop, inst_iter)
            {
                LLVMStatisticsInc(LICMBasic);
            }
            // move the instruction if it is load and can move out of the loop
            else if(LLVMIsALoadInst(inst_iter) && canMoveOutOfLoop(Loop, inst_iter, store_addr) && LLVMMakeLoopInvariant(Loop, LLVMGetOperand(inst_iter, 0)))
            {
                LLVMStatisticsInc(LICMLoadHoist);
            }
            inst_iter = LLVMGetNextInstruction(inst_iter);
        }
    }
    worklist_destroy(list);
    worklist_destroy(store_addr);
}

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

void LoopInvariantCodeMotion(LLVMModuleRef Module)
{
    LICMBasic = LLVMStatisticsCreate("LICMBasic", "basic loop invariant instructions");
    LICMLoadHoist = LLVMStatisticsCreate("LICMLoadHoist", "loop invariant load instructions");
    LICMNoPreheader = LLVMStatisticsCreate("LICMNoPreheader", "absence of preheader prevents optimization");
    NumLoops = LLVMStatisticsCreate("NumLoops", "number of loops");
    NumInst = LLVMStatisticsCreate("NumInst", "NumInst");
    NumBlock = LLVMStatisticsCreate("NumBlock", "NumBlock");
    NumLoopsNoStore = LLVMStatisticsCreate("NumLoopsNoStore", "NumLoopsNoStore");
    NumLoopsNoLoad = LLVMStatisticsCreate("NumLoopsNoLoad", "NumLoopsNoLoad");
    NumLoopsNoStoreWithLoad = LLVMStatisticsCreate("NumLoopsNoStoreWithLoad", "NumLoopsNoStoreWithLoad");
    NumLoopsWithCall = LLVMStatisticsCreate("NumLoopsWithCall", "NumLoopsWithCall");

    CSEDead = LLVMStatisticsCreate("CSEDead", "CSE found dead instructions");
    CSESimplify = LLVMStatisticsCreate("CSESimplify", "CSE simplified instructions");
    /* Implement here! */

    // worklist_t loops_found = 
    LLVMValueRef fn_iter; // iterator 
    worklist_t loops_found = worklist_create();
    for (fn_iter = LLVMGetFirstFunction(Module); fn_iter != NULL; 
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
                // Additional Optimization 0: Eliminate dead instructions
                if (isDead(inst_iter)) {

                    LLVMStatisticsInc(CSEDead);
                    LLVMValueRef rm = inst_iter;
                    // update iterator first, before erasing
                    inst_iter = LLVMGetNextInstruction(inst_iter);

                    LLVMInstructionEraseFromParent(rm);
                    continue;
                }

                // Additional Optimization 1: Simplify Instructions
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

                inst_iter = LLVMGetNextInstruction(inst_iter);
            }
        }
        // get loop info for Function F
        if(LLVMGetFirstBasicBlock(fn_iter) != NULL)
        {
            LLVMLoopInfoRef LI = LLVMCreateLoopInfoRef(fn_iter);
            LLVMLoopRef Loop;
            for(Loop = LLVMGetFirstLoop(LI); Loop != NULL;
                Loop = LLVMGetNextLoop(LI,Loop))
            {   
                // conduct LICM for each loop based on loop info
                LICM(LI, Loop, loops_found);
            }
        }

    }
    worklist_destroy(loops_found);
}
