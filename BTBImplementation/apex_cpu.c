/*
 * apex_cpu.c
 * Contains APEX cpu pipeline implementation
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"
#include "apex_macros.h"


/* Converts the PC(4000 series) into array index for code memory
 *
 * Note: You are not supposed to edit this function
 */
static int
get_code_memory_index_from_pc(const int pc)
{
    return (pc - 4000) / 4;
}

static void initBTB(BTB *btb, unsigned int instructionAddress, int opcode, int predTarget)
{
    // Check if the BTB is full
    if (btb->size == BTB_SIZE)
    {
        // Eviction policy: Replace the oldest entry
        int index = btb->oldestEntryIndex;

        // Increment the oldest entry index in a circular fashion
        btb->oldestEntryIndex = (btb->oldestEntryIndex + 1) % BTB_SIZE;

        // Replace the oldest entry with the new one
        btb->entries[index].address = instructionAddress;

        // Initialize history bits based on opcode
        if (opcode == OPCODE_BNZ || opcode == OPCODE_BP)
        {
            btb->entries[index].history[0] = '1';
            btb->entries[index].history[1] = '1';
        }
        else if (opcode == OPCODE_BZ || opcode == OPCODE_BNP)
        {
            btb->entries[index].history[0] = '0';
            btb->entries[index].history[1] = '0';
        }

        btb->entries[index].targetAddress = predTarget;
        btb->entries[index].count = 0;
    }
    else
    {
        // Check if an entry with the same address already exists in the BTB
        for (int i = 0; i < btb->size; i++)
        {
            if (btb->entries[i].address == instructionAddress)
            {
                return; // Exit the function since the entry already exists
            }
        }

        // Add the new entry to the BTB
        int newIndex = btb->size;
        btb->entries[newIndex].address = instructionAddress;

        // Initialize history bits based on opcode
        if (opcode == OPCODE_BNZ || opcode == OPCODE_BP)
        {
            btb->entries[newIndex].history[0] = '1';
            btb->entries[newIndex].history[1] = '1';
        }
        else if (opcode == OPCODE_BZ || opcode == OPCODE_BNP)
        {
            btb->entries[newIndex].history[0] = '0';
            btb->entries[newIndex].history[1] = '0';
        }

        btb->entries[newIndex].targetAddress = predTarget;
        btb->entries[newIndex].count = 0;
        // Increment the size of the BTB
        btb->size++;
    }
}

static int predictBTB(BTB *btb, unsigned int instructionAddress, int opcode)
{
    for (int i = 0; i < BTB_SIZE; i++)
    {
        if (btb->entries[i].address == instructionAddress)
        {
            if (btb->entries[i].count < 1)
            {
                return 0;
            }
            // Check prediction policy based on opcode
            if ((opcode == OPCODE_BNZ || opcode == OPCODE_BP) && (btb->entries[i].history[0] == '1' || btb->entries[i].history[1] == '1'))
            {
                return 1; // Taken
            }
            else if ((opcode == OPCODE_BNZ || opcode == OPCODE_BP) && (btb->entries[i].history[0] == '0' && btb->entries[i].history[1] == '0'))
            {
                return 0; // Not taken
            }
            else if ((opcode == OPCODE_BZ || opcode == OPCODE_BNP) && (btb->entries[i].history[0] == '1' && btb->entries[i].history[1] == '1'))
            {
                return 1; // Not taken
            }
            else if ((opcode == OPCODE_BZ || opcode == OPCODE_BNP) && (btb->entries[i].history[0] == '0' || btb->entries[i].history[1] == '0'))
            {
                return 0; // Not taken
            }
        }
    }
    return -1; // Entry not found in BTB
}

// Function to update the BTB based on the actual outcome in the execute stage
void updateBTB(BTB *btb, unsigned int address, char opcode, char outcome, unsigned int targetAddress)
{
    // Find the BTB entry for the given address and opcode
    int index = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (btb->entries[i].address == address)
        {
            index = i;
            break;
        }
    }

    // If the BTB entry is found, update the history bits and target address
    if (index != -1)
    {
        // Update the history bits based on the actual outcome
        btb->entries[index].history[1] = btb->entries[index].history[0];
        btb->entries[index].history[0] = outcome;

        // Update the target address
        btb->entries[index].targetAddress = targetAddress;
    }
    // Handle the case where the BTB entry is not found (shouldn't happen in normal execution)
    else
    {
        // Implement replacement policy to choose a victim entry for replacement
        // ...
    }
    btb->entries[index].count++;
}

static void
print_instruction(const CPU_Stage *stage)
{
    switch (stage->opcode)
    {
        case OPCODE_NOP:
        {
            printf("%s", stage->opcode_str);
            break;
        }
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_DIV:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        {
            printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->rs2);
            break;
        }
        case OPCODE_ADDL:
        case OPCODE_SUBL:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->imm);
            break;
        }
        case OPCODE_MOVC:
        {
            printf("%s,R%d,#%d ", stage->opcode_str, stage->rd, stage->imm);
            break;
        }

        case OPCODE_JALR:
        case OPCODE_LOADP:
        case OPCODE_LOAD:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->imm);
            break;
        }

        case OPCODE_STOREP:
        case OPCODE_STORE:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
                   stage->imm);
            break;
        }

        case OPCODE_BP:
        case OPCODE_BNP:
        case OPCODE_BN:
        case OPCODE_BNN:
        case OPCODE_BZ:
        case OPCODE_BNZ:
        {
            printf("%s,#%d ", stage->opcode_str, stage->imm);
            break;
        }

        case OPCODE_CMP:
        {
            printf("%s,R%d,R%d", stage->opcode_str, stage->rs1, stage->rs2);
            break;
        }
        case OPCODE_JUMP:
        case OPCODE_CML:
        {
            printf("%s,R%d,#%d", stage->opcode_str, stage->rs1, stage->imm);
            break;
        }

        case OPCODE_HALT:
        {
            printf("%s", stage->opcode_str);
            break;
        }
    }
}

/* Debug function which prints the CPU stage content
 *
 * Note: You can edit this function to print in more detail
 */
static void
print_stage_content(const char *name, const CPU_Stage *stage)
{
    printf("%-15s: pc(%d) ", name, stage->pc);
    print_instruction(stage);
    printf("\n");
}

/* Debug function which prints the register file
 *
 * Note: You are not supposed to edit this function
 */
static void
print_reg_file(const APEX_CPU *cpu)
{
    int i;

    printf("----------\n%s\n----------\n", "Registers:");

    for (int i = 0; i < REG_FILE_SIZE / 2; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");

    for (i = (REG_FILE_SIZE / 2); i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");
    printf("P = %d\n", cpu->positive_flag);
    printf("N = %d\n", cpu->negative_flag);
    printf("Z = %d\n", cpu->zero_flag);
    printf("\n");

    // for (int i = 2000; i < 2010; ++i)
    // {
    //     printf("MEM[%-3d%s ", i, "]");
    //     printf("      DATA VALUE = %-4d", cpu->data_memory[i]);
    //     printf("\n");
    // }
    for (int i = 0; i < 4000; ++i)
    {
        if (cpu->data_memory[i] != 0)
        {
            printf("MEM[%-3d%s ", i, "]");
            printf("      DATA VALUE = %-4d", cpu->data_memory[i]);
            printf("\n");
        }
    }
}

/*
 * Fetch Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_fetch(APEX_CPU *cpu)
{
    APEX_Instruction *current_ins;

    if (cpu->fetch.has_insn)
    {
        /* This fetches new branch target instruction from next cycle */
        if (cpu->fetch_from_next_cycle == TRUE)
        {
            cpu->fetch_from_next_cycle = FALSE;

            /* Skip this cycle*/
            return;
        }
        if(cpu->stall == 1){
            cpu->fetch_from_next_cycle = FALSE;
            cpu->fetch.pc = cpu->pc;
            current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
            strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
            cpu->fetch.opcode = current_ins->opcode;
            cpu->fetch.rd = current_ins->rd;
            cpu->fetch.rs1 = current_ins->rs1;
            cpu->fetch.rs2 = current_ins->rs2;
            cpu->fetch.imm = current_ins->imm;
            if (ENABLE_DEBUG_MESSAGES)
            {
                print_stage_content("Fetch", &cpu->fetch);
            }
            return;
        }

        /* Store current PC in fetch latch */
        cpu->fetch.pc = cpu->pc;

        /* Index into code memory using this pc and copy all instruction fields
         * into fetch latch  */
        current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
        strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
        cpu->fetch.opcode = current_ins->opcode;
        cpu->fetch.rd = current_ins->rd;
        cpu->fetch.rs1 = current_ins->rs1;
        cpu->fetch.rs2 = current_ins->rs2;
        cpu->fetch.imm = current_ins->imm;

        /* Update PC for next instruction */
        cpu->pc += 4;

        /* Copy data from fetch latch to decode latch*/
        cpu->decode = cpu->fetch;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Fetch", &cpu->fetch);
        }

        /* Stop fetching new instructions if HALT is fetched */
        if (cpu->fetch.opcode == OPCODE_HALT)
        {
            cpu->fetch.has_insn = FALSE;
        }
    }
}

/*
 * Decode Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_decode(APEX_CPU *cpu)
{
    if (cpu->decode.has_insn)
    {
            /* Read operands from register file based on the instruction type */
            switch (cpu->decode.opcode)
            {
                case OPCODE_ADD:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2] )== BUSY){
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_ADDL:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    break;
                }

                case OPCODE_SUB:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_SUBL:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    break;
                }

                case OPCODE_MUL:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_AND:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_OR:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_XOR:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_LOAD:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY){
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->status[cpu->decode.rd] = BUSY;
                    break;
                }

                case OPCODE_LOADP:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->status[cpu->decode.rs1] = BUSY;
                    cpu->status[cpu->decode.rd] = BUSY;
                    break;
                }

                case OPCODE_STORE:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_STOREP:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    cpu->status[cpu->decode.rs2] = BUSY;
                    break;
                }

                case OPCODE_CMP:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY || (cpu->status[cpu->decode.rs2]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                    break;
                }

                case OPCODE_CML:
                {
                    if ((cpu->status[cpu->decode.rs1]) == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    break;
                }

                case OPCODE_MOVC:
                {
                    //cpu->status[cpu->decode.rd] = BUSY;
                    /* MOVC doesn't have register operands */
                    break;
                }
                case OPCODE_JUMP:
                {
                    if(cpu->status[cpu->decode.rs1] == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    break;
                    
                }

                case OPCODE_JALR:
                {
                    if (cpu->status[cpu->decode.rs1] == BUSY)
                    {
                        cpu->stall = 1;
                        if (ENABLE_DEBUG_MESSAGES)
                        {
                            print_stage_content("Decode/RF", &cpu->decode);
                        }
                        return;
                    }
                    else
                    {
                        cpu->stall = 0;
                    }
                    cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                    cpu->status[cpu->decode.rd] = BUSY;
                    break;
                }

                case OPCODE_BP:
                case OPCODE_BNP:
                case OPCODE_BZ:
                case OPCODE_BNZ:
                {
                    initBTB(&cpu->btb, cpu->decode.pc, cpu->decode.opcode, cpu->decode.pc + cpu->decode.imm);
                    int prediction = predictBTB(&cpu->btb, cpu->decode.pc, cpu->decode.opcode);
                    printf("pred%d", prediction);
                    if (prediction == 1)
                    {
                        cpu->pc = cpu->decode.pc + cpu->decode.imm;

                        // Enable fetch to start fetching from the new PC
                        cpu->fetch.has_insn = TRUE;
                        cpu->decode.has_insn = TRUE;

                        // Since we are using reverse callbacks for pipeline stages, prevent the new instruction from being fetched in the current cycle
                        // cpu->fetch_from_next_cycle = TRUE;
                    }
                    break;
                }

                case OPCODE_NOP:
                {
                    break;
                }
            }

        /* Copy data from decode latch to execute latch*/
        if(!cpu->stall){
            cpu->execute = cpu->decode;
            cpu->decode.has_insn = FALSE;
        }
        cpu->stall = 0;
        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Decode/RF", &cpu->decode);
        }
    }
}

/*
 * Execute Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_execute(APEX_CPU *cpu)
{
    if (cpu->execute.has_insn)
    {
        /* Execute logic based on instruction type */
        switch (cpu->execute.opcode)
        {
            case OPCODE_ADD:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;

                break;
            }

            case OPCODE_ADDL:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.imm;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;

                break;
            }
            case OPCODE_SUB:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;

                break;
            }

            case OPCODE_SUBL:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.imm;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;

                break;
            }

            case OPCODE_CMP:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                break;
            }

            case OPCODE_CML:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.imm;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                break;
            }

            case OPCODE_MUL:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value * cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;

                break;
            }

            case OPCODE_AND:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value & cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;
                break;
            }

            case OPCODE_OR:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value | cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;
                break;
            }

            case OPCODE_XOR:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value ^ cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                cpu->zero_flag = (cpu->execute.result_buffer == 0) ? TRUE : FALSE;
                /* Set the positive flag based on the result buffer */
                cpu->positive_flag = (cpu->execute.result_buffer > 0) ? TRUE : FALSE;
                /* Set the negative flag based on the result buffer */
                cpu->negative_flag = (cpu->execute.result_buffer < 0) ? TRUE : FALSE;

                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;
                break;
            }

            case OPCODE_LOAD:
            {
                cpu->execute.memory_address
                    = cpu->execute.rs1_value + cpu->execute.imm;
                //cpu->status[cpu->execute.rd] = BUSY;
                break;
            }

            case OPCODE_LOADP:
            {
                cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->execute.rs1_value = cpu->execute.rs1_value + 4;
                cpu->regs[cpu->execute.rs1] = cpu->execute.rs1_value;
                cpu->status[cpu->execute.rs1] = FREE;
                break;
            }

            case OPCODE_STORE:
            { 
                cpu->execute.memory_address = cpu->execute.rs2_value + cpu->execute.imm;
                break;
            }

            case OPCODE_STOREP:
            {
                cpu->execute.memory_address = cpu->execute.rs2_value + cpu->execute.imm;
                cpu->execute.rs2_value = cpu->execute.rs2_value + 4;
                cpu->regs[cpu->execute.rs2] = cpu->execute.rs2_value;
                cpu->status[cpu->execute.rs2] = FREE;
                break;
            }

            case OPCODE_JUMP:
            {
                cpu->pc = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->fetch_from_next_cycle = TRUE;
                cpu->fetch.has_insn = TRUE;
                cpu->decode.has_insn = FALSE;
                break;
            }

            case OPCODE_JALR:
            {
                cpu->pc = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->fetch_from_next_cycle = TRUE;
                cpu->decode.has_insn = FALSE;
                cpu->fetch.has_insn = TRUE;
                break;
            }

            case OPCODE_BZ:
            {
                char branch_taken = '0';
                if (cpu->zero_flag == TRUE)
                {
                    branch_taken = '1';
                }
                int prediction = predictBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode);
                updateBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode, branch_taken, cpu->execute.pc + cpu->execute.imm);
                int branch = branch_taken - '0';
                if (prediction == branch)
                {
                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    // cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                else if (prediction == 1 && branch == 0)
                {
                    cpu->pc = cpu->execute.pc + 4;
                    cpu->decode.has_insn = FALSE;
                    cpu->fetch_from_next_cycle = TRUE;
                    cpu->fetch.has_insn = TRUE;
                }
                else
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    /* Flush previous stages */

                    cpu->decode.has_insn = FALSE;

                    cpu->fetch_from_next_cycle = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }

                break;
            }

            case OPCODE_BNZ:
            {
                char branch_taken = '0';
                if (cpu->zero_flag == FALSE)
                {
                    branch_taken = '1';
                }
                int prediction = predictBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode);
                updateBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode, branch_taken, cpu->execute.pc + cpu->execute.imm);
                int branch = branch_taken - '0';
                if (prediction == branch)
                {
                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    // cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                else if (prediction == 1 && branch == 0)
                {
                    cpu->pc = cpu->execute.pc + 4;
                    cpu->decode.has_insn = FALSE;
                    cpu->fetch_from_next_cycle = TRUE;
                    cpu->fetch.has_insn = TRUE;
                }
                else
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    /* Flush previous stages */

                    cpu->decode.has_insn = FALSE;

                    cpu->fetch_from_next_cycle = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }

                break;
            }

            case OPCODE_BP:
            {
                char branch_taken = '0';
                if (cpu->positive_flag == TRUE)
                {
                    branch_taken = '1';
                }
                int prediction = predictBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode);
                updateBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode, branch_taken, cpu->execute.pc + cpu->execute.imm);
                int branch = branch_taken - '0';
                if (prediction == branch)
                {
                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    // cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                else if (prediction == 1 && branch == 0)
                {
                    cpu->pc = cpu->execute.pc + 4;
                    cpu->decode.has_insn = FALSE;
                    cpu->fetch_from_next_cycle = TRUE;
                    cpu->fetch.has_insn = TRUE;
                }
                else
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    /* Flush previous stages */

                    cpu->decode.has_insn = FALSE;

                    cpu->fetch_from_next_cycle = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }

                break;
            }

            case OPCODE_BNP:
            {
                char branch_taken = '0';
                if (cpu->positive_flag == FALSE)
                {
                    branch_taken = '1';
                }
                int prediction = predictBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode);
                updateBTB(&cpu->btb, cpu->execute.pc, cpu->execute.opcode, branch_taken, cpu->execute.pc + cpu->execute.imm);
                int branch = branch_taken - '0';
                if (prediction == branch)
                {
                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    // cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                else if (prediction == 1 && branch == 0)
                {
                    cpu->pc = cpu->execute.pc + 4;
                    cpu->decode.has_insn = FALSE;
                    cpu->fetch_from_next_cycle = TRUE;
                    cpu->fetch.has_insn = TRUE;
                }
                else
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    /* Flush previous stages */

                    cpu->decode.has_insn = FALSE;

                    cpu->fetch_from_next_cycle = TRUE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }

                break;
            }

            case OPCODE_BN:
            {
                if (cpu->negative_flag == TRUE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;

                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BNN:
            {
                if (cpu->negative_flag == FALSE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;

                    /* Since we are using reverse callbacks for pipeline stages,
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }
            case OPCODE_MOVC: 
            {
                cpu->execute.imm = cpu->execute.imm + 0;
                cpu->execute.result_buffer = cpu->execute.imm;
                cpu->regs[cpu->execute.rd] = cpu->execute.result_buffer;
                cpu->status[cpu->execute.rd] = FREE;
                break;
            }

            case OPCODE_NOP:
            {
                break;
            }
        }

        /* Copy data from execute latch to memory latch*/
        if(!(cpu->stall)){
                cpu->memory = cpu->execute;
                cpu->execute.has_insn = FALSE;
        }

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Execute", &cpu->execute);
        }
    }
}

/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_memory(APEX_CPU *cpu)
{
    if (cpu->memory.has_insn)
    {
        switch (cpu->memory.opcode)
        {
            case OPCODE_ADD:
            {
                /* No work for ADD */
                break;
            }

            case OPCODE_LOAD:
            {
                /* Read from data memory */
                cpu->memory.result_buffer
                    = cpu->data_memory[cpu->memory.memory_address];
                cpu->regs[cpu->memory.rd] = cpu->memory.result_buffer;
                cpu->status[cpu->memory.rd] = FREE;
                break;
            }

            case OPCODE_LOADP:
            {
                /* Read from data memory */
                cpu->memory.result_buffer = cpu->data_memory[cpu->memory.memory_address];
                cpu->regs[cpu->memory.rd] = cpu->memory.result_buffer;
                cpu->status[cpu->memory.rd] = FREE;
                break;
            }

            case OPCODE_STORE:
            {
                cpu->data_memory[cpu->memory.memory_address] = cpu->memory.rs1_value;
                break;
            }

            case OPCODE_STOREP:
            {
                cpu->data_memory[cpu->memory.memory_address] = cpu->memory.rs1_value;
                break;
            }

            case OPCODE_NOP:
            {
                break;
            }
        }

        /* Copy data from memory latch to writeback latch*/
        cpu->writeback = cpu->memory;
        cpu->memory.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Memory", &cpu->memory);
        }
    }
}

/*
 * Writeback Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static int
APEX_writeback(APEX_CPU *cpu)
{
    if (cpu->writeback.has_insn)
    {
        /* Write result to register file based on instruction type */
        switch (cpu->writeback.opcode)
        {
            case OPCODE_ADD:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_ADDL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_SUB:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_SUBL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_MUL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_AND:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_OR:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_XOR:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_LOAD:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_LOADP:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->regs[cpu->writeback.rs1] = cpu->writeback.rs1_value;
                cpu->status[cpu->writeback.rd] = FREE;
                cpu->status[cpu->writeback.rs1] = FREE;
                break;
            }

            case OPCODE_STOREP:
            {
                cpu->regs[cpu->writeback.rs2] = cpu->writeback.rs2_value;
                cpu->status[cpu->writeback.rs2] = FREE;
                break;
            }

            case OPCODE_MOVC: 
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                cpu->status[cpu->writeback.rd] = FREE;
                break;
            }

            case OPCODE_JALR:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.pc +4;
                cpu->status[cpu->writeback.rd] = FREE;
            }
            case OPCODE_NOP:
            {
                break;
            }
        }

        cpu->insn_completed++;
        cpu->writeback.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Writeback", &cpu->writeback);
        }

        if (cpu->writeback.opcode == OPCODE_HALT)
        {
            /* Stop the APEX simulator */
            return TRUE;
        }

        
    }

    /* Default */
    return 0;
}

/*
 * This function creates and initializes APEX cpu.
 *
 * Note: You are free to edit this function according to your implementation
 */
APEX_CPU *
APEX_cpu_init(const char *filename)
{
    int i;
    APEX_CPU *cpu;

    if (!filename)
    {
        return NULL;
    }

    cpu = calloc(1, sizeof(APEX_CPU));

    if (!cpu)
    {
        return NULL;
    }

    /* Initialize PC, Registers and all pipeline stages */
    cpu->pc = 4000;
    memset(cpu->regs, 0, sizeof(int) * REG_FILE_SIZE);
    memset(cpu->data_memory, 0, sizeof(int) * DATA_MEMORY_SIZE);
    cpu->single_step = ENABLE_SINGLE_STEP;


    for (i = 0; i < REG_FILE_SIZE; i++)
    {
        cpu->status[i] = FREE;
    }

    /* Parse input file and create code memory */
    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
    if (!cpu->code_memory)
    {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES)
    {
        fprintf(stderr,
                "APEX_CPU: Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU: PC initialized to %d\n", cpu->pc);
        fprintf(stderr, "APEX_CPU: Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode_str", "rd", "rs1", "rs2",
               "imm");

        for (i = 0; i < cpu->code_memory_size; ++i)
        {
            printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode_str,
                   cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
        }
    }

    /* To start fetch stage */
    cpu->fetch.has_insn = TRUE;
    return cpu;
}

/*
 * APEX CPU simulation loop
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_run(APEX_CPU *cpu, int cyclecount)
{
    char user_prompt_val;

    if (cyclecount != -1)
    {
        cpu->single_step = FALSE;
    }

    while (TRUE)
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("--------------------------------------------\n");
            printf("Clock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------------------\n");
        }

        if (APEX_writeback(cpu))
        {
            /* Halt in writeback stage */
            printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
            break;
        }

        APEX_memory(cpu);
        APEX_execute(cpu);
        APEX_decode(cpu);
        APEX_fetch(cpu);

        print_reg_file(cpu);

        if (cpu->single_step)
        {
            printf("Press any key to advance CPU Clock or <q> to quit:\n");
            scanf("%c", &user_prompt_val);

            if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
            {
                printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
                break;
            }
        }

        cpu->clock++;
        if (cyclecount == cpu->clock)
        {
            break;
        }
    }
}

/*
 * This function deallocates APEX CPU.
 *
 * Note: You are free to edit this function according to your implementation
 */
void
APEX_cpu_stop(APEX_CPU *cpu)
{
    free(cpu->code_memory);
    free(cpu);
}