/**************************************************************************/
/*  Copyright 2003 by SEED.                                               */
/*  All rights reserved. Property of SEED.                                */
/**************************************************************************/
 
/*
 *  ======== seeddm642_cpld.c ========
 *  CPLD module for the SEEDDM642
 */
#include "seeddm642.h"

/* Read an 8-bit value from a CPLD register */
Uint8 SEEDDM642_rget(Int16 regnum)
{
    Uint8 *pdata;
    
    /* Return lower 8 bits of register */
    pdata = (Uint8 *)(SEEDDM642_CPLD_BASE + regnum);
    return (*pdata & 0xff);
}

/* Write an 8-bit value to a CPLD register */
void SEEDDM642_rset(Int16 regnum, Uint8 regval)
{
    Uint8 *pdata;
    
    /* Write lower 8 bits of register */
    pdata = (Uint8 *)(SEEDDM642_CPLD_BASE + regnum);
    *pdata = (regval & 0xff);
}

/* Spin in a delay loop for delay iterations */
void SEEDDM642_wait(Uint32 delay)
{
    volatile Uint32 i, n;
    
    n = 0;
    for (i = 0; i < delay; i++)
    {
        n = n + 1;
    }
}

/* Spin in a delay loop for delay microseconds */
void SEEDDM642_waitusec(Uint32 delay)
{
    SEEDDM642_wait(delay * 21);
}



