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

LLVMBool canMoveOutOfLoop(LLVMLoopRef L, LLVMValueRef I, worklist_t store_addr)
{

    LLVMValueRef addr = LLVMGetOperand(I, 0);
    if(LLVMGetVolatile(I)) return (LLVMBool)0;
    if(worklist_include(store_addr, addr)) return (LLVMBool)0;

    if(LLVMIsAGlobalVariable(addr)) return (LLVMBool)1;
    if(LLVMIsAAllocaInst(addr)) return (LLVMBool)1;
    return (LLVMBool)0;
}

void LICM(LLVMLoopInfoRef LIRef, LLVMLoopRef Loop, worklist_t loops_found)
{
    // use Loop
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
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMLoad) has_load = (LLVMBool)1;
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMStore)
            {
                worklist_insert(store_addr, LLVMGetOperand(inst_iter_, 1));
                has_store = (LLVMBool)1;
            }
            if(LLVMGetInstructionOpcode(inst_iter_) == LLVMCall) has_call = (LLVMBool)1;
            inst_iter_ = LLVMGetNextInstruction(inst_iter_);
        }
    }
    
    LLVMStatisticsInc(NumLoops);
    if(!has_store) LLVMStatisticsInc(NumLoopsNoStore);
    if(!has_load) LLVMStatisticsInc(NumLoopsNoLoad);
    if(!has_store && has_load) LLVMStatisticsInc(NumLoopsNoStoreWithLoad);
    if(has_call) LLVMStatisticsInc(NumLoopsWithCall);
    worklist_destroy(list);


    list = LLVMGetBlocksInLoop(Loop);
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

        LLVMLoopRef inner_Loop = LLVMGetLoopRef(LIRef, bb_iter);
        if(inner_Loop != Loop)
        {
            if(!worklist_include_BB(loops_found, bb_iter, LIRef))
            {
                worklist_insert(loops_found, LLVMBasicBlockAsValue(bb_iter));
                LICM(LIRef, inner_Loop, loops_found);
            }
            continue;
        }
        LLVMValueRef inst_iter = LLVMGetFirstInstruction(bb_iter);
        while(inst_iter != NULL) 
        {
            // LLVMStatisticsInc(NumInst);
            if(!LLVMIsALoadInst(inst_iter) && !LLVMIsAStoreInst(inst_iter)) // LLVMIsValueLoopInvariant(Loop, inst_iter) && LLVMMakeLoopInvariant(Loop, inst_iter)
            {
                LLVMMakeLoopInvariant(Loop, inst_iter);
                LLVMStatisticsInc(LICMBasic);
            }
            else if(LLVMIsALoadInst(inst_iter) && canMoveOutOfLoop(Loop, inst_iter, store_addr))
            {
                LLVMMakeLoopInvariant(Loop, inst_iter);
                LLVMStatisticsInc(LICMLoadHoist);
            }
            inst_iter = LLVMGetNextInstruction(inst_iter);
        }
    }
    worklist_destroy(list);
    worklist_destroy(store_addr);
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

    /* Implement here! */

    // worklist_t loops_found = 
    LLVMValueRef fn_iter; // iterator 
    worklist_t loops_found = worklist_create();
    for (fn_iter = LLVMGetFirstFunction(Module); fn_iter != NULL; 
        fn_iter = LLVMGetNextFunction(fn_iter))
    {
        // get loop info for Function F
        if(LLVMGetFirstBasicBlock(fn_iter) != NULL)
        {
            LLVMLoopInfoRef LI = LLVMCreateLoopInfoRef(fn_iter);
            LLVMLoopRef Loop;
            for(Loop = LLVMGetFirstLoop(LI); Loop != NULL;
                Loop = LLVMGetNextLoop(LI,Loop))
            {                
                LICM(LI, Loop, loops_found);
            }
            /* if instruction, insn, is invariant in Loop, move it out of the loop */
            // LLVMMakeLoopInvariant(Loop, insn); 

        }


    }
}
