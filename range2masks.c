/*
 * Copyright (c) 2017 Yoichi Hariguchi
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so,
 * subject to the following conditions: 
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 */

#include <assert.h>
#include "local_types.h"


enum {
    MAXENT  = 32,
};

typedef struct tcamEnt_ {
    u32 patt;
    u32 mask;
} tcamEnt;

typedef struct aclRule_ {
    u32     nEnt;               /* number of TCAM entries */
    tcamEnt ent[MAXENT];        /* pattern/mask */
} aclRule;



void
printEntry (u32 patt, u32 mask)
{
    u32 st;
    u32 end;

    st  = patt;
    end = patt | ((~patt) & (~mask));
    printf("patt: %08x  (%d - %d)\n", patt, st, end);
    printf("mask: %08x\n", mask);
}

void
printEntries (aclRule* p)
{
    int i;


    if (!p) {
        return;
    }
    for (i = 0; i < p->nEnt; ++i) {
        printEntry(p->ent[i].patt, p->ent[i].mask);
    }
}


/* range2masks:
     Prints out a set of TCAM entries that represents the range
     (st <= range <= end). 'end' must be < 0xffffffff.
   Input:
     st  - number that the range starts
     end - number that the range ends
   Output:
   Return:
     SUCCESS - if there is no error
     FAILURE - otherwise.
 */
int
range2masks (u32 st, u32 end, aclRule* pRule)
{
    u32 patt;
    u32 mask;
    u32 i;


    if (!pRule) {
        return -1;
    }
    if (end == ~0) {
        if (st != 0) {
            fprintf(stderr,
                    "end too big: must be < %u (0x%x)\n", end, end);
            return FAILURE;
        }
    }

    pRule->nEnt = 0;
    for (patt = end; patt >= st; --patt) {

        if (pRule->nEnt >= elementsOf(pRule->ent)) {
            fprintf(stderr,
                   "not enough memory (%d:%d)\n", st, end);
            return FAILURE;
        }

        /* First, find the first 0 from LSB in 'patt'
           while clearing 1s.
         */
        mask = ~0;
        i    = 1;
        while (i & patt) {
            patt ^=  i;         /* clear log(i)-th bit */
            i    <<= 1;
            mask <<= 1;         /* mask = (~0) << log(i) */
        }

        /* Second, if 'patt' becomes too small,
           retract 'patt' until 'patt' >= 'st'.
         */
        while (patt < st) {
            i    >>= 1;
            patt |= i;          /* set log(i)-th bit */
            mask |= i;          /* mask = (~0) << log(i) */
        }
        pRule->ent[pRule->nEnt].patt = patt;
        pRule->ent[pRule->nEnt].mask = mask;
        ++pRule->nEnt;
        if (patt == 0) {        /* prevent infinite loop */
            return SUCCESS;
        }
    }
    return SUCCESS;
}


int
main (int argc, char *argv[])
{
    u32 start, end;
    aclRule rules[3]; /* [0]: orig, [1]: 0 - (start-1), [2]: 0 - end */
    int st;


    if (argc < 3) {
        fprintf(stderr,
                "Usage: range2ent <start> <end> [-optimize]\n");
        exit(1);
    }
    start = strtoul(argv[1], NULL, 0);
    end   = strtoul(argv[2], NULL, 0);
    rules[0].nEnt = 0;
    rules[1].nEnt = 0;
    rules[2].nEnt = 0;

    /*
     * Assume action is accept.
     * Compare the number of TCAM entries between the following rules:
     *  1. start:end (accept)
     *  2. Combination of:
     *      [0 ... start-1] (reject)
     *      [0 ... end]     (accept)
     * then choose the better one.
     */
    st = range2masks(start, end, rules);
    assert(st == SUCCESS);
    if (argc <= 3) {
        /*
         * No optimization
         */
        printEntries(rules);
        exit(0);
    }
    if (start == 0) {
        /*
         * No optimization: 'start - 1' is negative.
         */
        printEntries(rules);
        exit(0);
    }
    /*
     * Make two sets of TCAM entries and choose the better one
     */
    st = range2masks(0, start - 1, rules + 1);
    assert(st == SUCCESS);
    st = range2masks(0, end, rules + 2);
    assert(st == SUCCESS);
    if ((rules[1].nEnt + rules[2].nEnt) < rules[0].nEnt) {
        printf("Reject: 0 - %d\n", start - 1);
        printEntries(rules + 1);
        printf("Accept: 0 - %d\n", end);
        printEntries(rules + 2);
    } else {
        printEntries(rules);
    }
    exit(0);

    return 0;                   /* make compiler happy */
}