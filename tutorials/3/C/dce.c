#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "llvm-c/Core.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Analysis.h"
#include "worklist.h"
#include "stats.h"

LLVMStatisticsRef Dead;
LLVMStatisticsRef WorkList;
int DCE_count=0;
void NoOptimization(LLVMModuleRef Module) {
    printf("NoOptimization is running!\n");
    worklist_t worklist = worklist_create();
    for (LLVMValueRef F = LLVMGetFirstFunction(Module);
    F!=NULL;
    F=LLVMGetNextFunction(F))
    {
        // Use each function, F
        for(LLVMBasicBlockRef BB=LLVMGetFirstBasicBlock(F);
        BB!=NULL;
        BB = LLVMGetNextBasicBlock(BB))
        {
            // get each basic block in F
            for (LLVMValueRef I = LLVMGetFirstInstruction(BB);
            I != NULL;
            I = LLVMGetNextInstruction(I))
            {
                if (isDead(I))
                    worklist_insert(worklist,I);

                // // loop over all instructions
                // LLVMUseRef use;
                // fprintf(stderr,"All users of the instruction: ");
                // LLVMDumpValue(I);
                // for(use = LLVMGetFirstUse(I);
                // use!=NULL;
                // use = LLVMGetNextUse(use))
                // {
                //     LLVMValueRef user = LLVMGetUser(use);
                //     LLVMDumpValue(user);
                // }

                // // for(unsigned op=0; op< LLVMGetNumOperands(I); op++) {
                // //     LLVMValueRef definition = LLVMGetOperand(I,op);
                // //     fprintf(stderr,"Definition of op=%d is:",op);
                // //     LLVMDumpValue(definition);
                // // }

                // for(unsigned op=0; op< LLVMGetNumOperands(I); op++) {
                //     LLVMValueRef definition = LLVMGetOperand(I,op);
                //     fprintf(stderr,"Definition of op=%d is:\n",op);
                //     LLVMDumpValue(definition);
                //     if(LLVMIsAInstruction(definition)) {
                //         fprintf(stderr,"Is an instruction!\n");
                //     }
                // }
            }
        }
        while(!worklist_empty(worklist)) {
            LLVMValueRef I = worklist_pop(worklist);
            if (isDead(I))
            {
                fprintf(stderr,"Delete: ");
                LLVMDumpValue(I);
                for(unsigned i=0; i<LLVMGetNumOperands(I); i++)
                {
                    LLVMValueRef J = LLVMGetOperand(I,i);
                    // Add to worklist only if J is an instruction
                    // Note, J still has one use (in I) so the isDead
                    // would return false, so we’d better not check that.
                    // This forces us to check in the if statement above.
                    if (LLVMIsAInstruction(J))
                        worklist_insert(worklist,J);
                }
                DCE_count++;
                LLVMInstructionEraseFromParent(I);
            }
        }

    }
    worklist_destroy(worklist);
}

int isDead(LLVMValueRef I) {
    if (LLVMGetFirstUse(I)!=NULL)
        return 0;
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

void RunDeadCodeElimination(LLVMModuleRef Module) {

}

int main(int argc, char **argv) {
/* some basic error handling */
    if (argc < 3) {
        fprintf(stderr, "Not enough files specified.\n");
        return 0;
    }

    LLVMMemoryBufferRef Buff = NULL;
    char *outMessage = NULL;

    /* 1. Read contents of object file from command line argv[1] */
    LLVMCreateMemoryBufferWithContentsOfFile(argv[1], &Buff, &outMessage);

    LLVMModuleRef Module = NULL;
    /* 2. Try to parse buffer into a legal Module */
    if (!Buff || LLVMParseBitcode(Buff, &Module, &outMessage)) {
        /* handle error */
        printf("Error opening file: %s\n", outMessage);
        return 1;
    }

    LLVMEnableStatistics();

    LLVMPassManagerRef PM = LLVMCreatePassManager();
    LLVMAddScalarReplAggregatesPass(PM);
    LLVMRunPassManager(PM, Module);

    Dead = LLVMStatisticsCreate("Dead", "Dead instructions");
    WorkList = LLVMStatisticsCreate("WorkList", "Instructions added to worklist");

    /* 3. Do optimization on Module */
    NoOptimization(Module);
    //RunDeadCodeElimination(Module);

    // LLVMDumpModule(Module);

    /* 4. Save result to a new file (argv[2]) to preserve original */
    char *msg;
    LLVMBool res = LLVMVerifyModule(Module, LLVMPrintMessageAction, &msg);
    if (!res)
        LLVMWriteBitcodeToFile(Module, argv[2]);
    else
        fprintf(stderr, "Error: %s not created.\n", argv[2]);

    LLVMPrintStatistics();

    return 0;
}
