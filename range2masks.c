/*
 * Copyright (c) 2017, 2018, 2019, 2020, 2021 Yoichi Hariguchi
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <regex.h>
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


bool isNumber (char *s);
int  mask2plen (u32 mask);
int  ipv4a2h (char *s, u32 *addr);
void printEntry (u32 patt, u32 mask);
void printEntries (aclRule* p);
int  range2masks (u32 st, u32 end, aclRule* pRule);


/**
 * @name  isNumber
 *
 * @brief Checks if the parameter (string) is a number or not
 *
 * @param[in] s Pointer to a string that may represent a number
 *
 * @retval TRUE  's' is a number (decimal or hexadecimal)
 * @retval FALSE 's' is not a number
 */
bool
isNumber (char *s)
{
    char *pRstr = "^(0x|)[0-9A-Fa-f]+$";
    static regex_t re;
    static bool isCompiled = FALSE;

    if (!isCompiled) {
        if (regcomp(&re, pRstr, REG_EXTENDED | REG_NOSUB)) {
            return FALSE;
        }
        isCompiled = TRUE;
    }
    if (regexec(&re, s, 0, NULL, 0)) {
        return FALSE;
    }
    return TRUE;
}

/**
 * @name  mask2plen
 *
 * @brief Converts a netmask into a prefix length
 *
 * @param[in] mask Netmask
 *
 * @retval >= 0 prefix length representing 'mask'
 * @retval < 0  'mask' is not contiguous
 */
int
mask2plen (u32 mask)
{
    int plen = 32;
    int m = ~0;

    while (plen > 0) {
        if (m == mask) {
            return plen;
        }
        m <<= 1;
        --plen;
    }
    return plen;
}

/**
 * @name  ipv4a2h
 *
 * @brief Converts an IPv4 dotted decimal notation to the host order integer
 *
 * @param[in]  s    Sgring representing an IPv4 address (w/o prefix length)
 * @param[out] addr Pointer to the converted IPv4 address
 *
 * @retval FAILURE (-1) Wrong IPv4 address format, or 'addr' is NULL
 * @retval SUCCESS (0)  '*addr' has the IPv4 address in the host order.
 */
int
ipv4a2h (char *s, u32 *addr)
{
    struct sockaddr_in sa;

    if (addr == NULL) {
        return FAILURE;
    }
    if (inet_pton(AF_INET, s, &(sa.sin_addr.s_addr)) > 0) {
        *addr = ntohl(sa.sin_addr.s_addr);
        return SUCCESS;
    }
    return FAILURE;
}

void
printPrefix (u32 patt, u32 mask)
{
    printf("%d.%d.%d.%d/%d\n",
           (patt >> 24),
           (patt >> 16) & 0xff,
           (patt >>8) & 0xff,
           patt & 0xff,
           mask2plen(mask));
}

void
printEntry (u32 patt, u32 mask)
{
    u32 st;
    u32 end;

    st  = patt;
    end = patt | ((~patt) & (~mask));
    printf("patt:   %08x (%d - %d)\n", patt, st, end);
    printf("mask:   %08x\n", mask);
    printf("prefix: %d.%d.%d.%d/%d\n",
           (patt >> 24),
           (patt >> 16) & 0xff,
           (patt >>8) & 0xff,
           patt & 0xff,
           mask2plen(mask));
}

void
printEntries (aclRule* p)
{
    int i;


    if (!p) {
        return;
    }
    for (i = 0; i < p->nEnt; ++i) {
        printPrefix(p->ent[i].patt, p->ent[i].mask);
    }
    puts("\n");
    for (i = 0; i < p->nEnt; ++i) {
        printEntry(p->ent[i].patt, p->ent[i].mask);
    }
}

/**
 * @name  range2masks
 *
 * @brief Convert an arbitrary range into a set of TCAM entries 
 *        (st <= range <= end). 'end' must be < 0xffffffff.
 *
 * @param[in]  st    Number that the range starts
 * @param[in]  end   Number that the range ends
 * @param[out] pRule Converted result. An array of (pattern, mask)
 *
 * @retval SUCCESS The range is successfully converted
 * @retval FAILURE Otherwise
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
    if (isNumber(argv[1])) {
        start = strtoul(argv[1], NULL, 0);
    } else if (ipv4a2h(argv[1], &start) == FAILURE) {
        fprintf(stderr, "ERROR: failed to parse %s\n", argv[1]);
        exit(1);
    }
    if (isNumber(argv[2])) {
        end = strtoul(argv[2], NULL, 0);
    } else if (ipv4a2h(argv[2], &end) == FAILURE) {
        fprintf(stderr, "ERROR: failed to parse %s\n", argv[2]);
        exit(1);
    }
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
