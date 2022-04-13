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
LLVMStatisticsRef fake;

LLVMBool canMoveOutOfLoop(LLVMLoopRef L, LLVMValueRef I)
{

    LLVMValueRef addr = LLVMGetOperand(I, 0);
    if(LLVMGetVolatile(I)) return false;
    if(LLVMIsAGlobalVariable(addr) && )
}

void LoopInvariantCodeMotion(LLVMModuleRef Module)
{
    LICMBasic = LLVMStatisticsCreate("LICMBasic", "basic loop invariant instructions");
    LICMLoadHoist = LLVMStatisticsCreate("LICMLoadHoist", "loop invariant load instructions");
    LICMNoPreheader = LLVMStatisticsCreate("LICMNoPreheader", "absence of preheader prevents optimization");
    NumLoops = LLVMStatisticsCreate("NumLoops", "number of loops");
    fake = LLVMStatisticsCreate("fake", "fake");

    /* Implement here! */

    LLVMValueRef fn_iter; // iterator 
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
                
                LLVMStatisticsInc(NumLoops);
                LLVMBasicBlockRef PH = LLVMGetPreheader(Loop);
                if(PH == NULL)
                {
                    LLVMStatisticsInc(LICMNoPreheader);
                    return;
                } 
                // use Loop
                worklist_t list = LLVMGetBlocksInLoop(Loop);
                while(!worklist_empty(list)) {
                    LLVMValueRef I = worklist_pop(list);
                    LLVMBasicBlockRef bb_iter = LLVMValueAsBasicBlock(I);
                    LLVMValueRef inst_iter = LLVMGetFirstInstruction(bb_iter);
                    while(inst_iter != NULL) 
                    {
                        LLVMStatisticsInc(fake);
                        if(LLVMMakeLoopInvariant(Loop, inst_iter) ) // LLVMIsValueLoopInvariant(Loop, inst_iter) && LLVMMakeLoopInvariant(Loop, inst_iter)
                        {
                            LLVMStatisticsInc(LICMBasic);
                        }
                        else if(LLVMIsALoadInst(inst_iter))
                        {
                            if(canMoveOutOfLoop(Loop, inst_iter) && LLVMMakeLoopInvariant(Loop, inst_iter))
                            {
                                
                            }
                        }
                        inst_iter = LLVMGetNextInstruction(inst_iter);
                    }
                }
                worklist_destroy(list);
            }
            /* if instruction, insn, is invariant in Loop, move it out of the loop */
            // LLVMMakeLoopInvariant(Loop, insn); 

        }


    }
}
