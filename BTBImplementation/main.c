/*
 * main.c
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"

int main(int argc, char const *argv[])
{
    APEX_CPU *cpu;
    int cyclecount = -1;

    fprintf(stderr, "APEX CPU Pipeline Simulator v%0.1lf\n", VERSION);

    if (argc < 2 || argc > 4)
    {
        fprintf(stderr, "APEX_Help: Usage %s <input_file> [simulate <n>]\n", argv[0]);
        exit(1);
    }

    cpu = APEX_cpu_init(argv[1]);
    if (!cpu)
    {
        fprintf(stderr, "APEX_Error: Unable to initialize CPU\n");
        exit(1);
    }

    if (argc == 4 && strcmp(argv[2], "simulate") == 0)
    {
        cyclecount = atoi(argv[3]);
        if (cyclecount <= 0)
        {
            fprintf(stderr, "APEX_Help: Invalid number of cycles. Please specify a positive integer.\n");
            APEX_cpu_stop(cpu);
            exit(1);
        }
    }
    else if (argc == 4)
    {
        fprintf(stderr, "APEX_Help: Usage %s <input_file> simulate <n>\n", argv[0]);
        APEX_cpu_stop(cpu);
        exit(1);
    }

    APEX_cpu_run(cpu, cyclecount);
    APEX_cpu_stop(cpu);
    return 0;
}
