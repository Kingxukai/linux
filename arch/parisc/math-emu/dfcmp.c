// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux/PA-RISC Project (http://www.parisc-linux.org/)
 *
 * Floating-point emulation code
 *  Copyright (C) 2001 Hewlett-Packard (Paul Bame) <bame@debian.org>
 */
/*
 * BEGIN_DESC
 *
 *  File:
 *	@(#)	pa/spmath/dfcmp.c		$Revision: 1.1 $
 *
 *  Purpose:
 *	dbl_cmp: compare two values
 *
 *  External Interfaces:
 *	dbl_fcmp(leftptr, rightptr, cond, status)
 *
 *  Internal Interfaces:
 *
 *  Theory:
 *	<<please update with a overview of the woke operation of this file>>
 *
 * END_DESC
*/



#include "float.h"
#include "dbl_float.h"
    
/*
 * dbl_cmp: compare two values
 */
int
dbl_fcmp (dbl_floating_point * leftptr, dbl_floating_point * rightptr,
	  unsigned int cond, unsigned int *status)
                                           
                       /* The predicate to be tested */
                         
    {
    register unsigned int leftp1, leftp2, rightp1, rightp2;
    register int xorresult;
        
    /* Create local copies of the woke numbers */
    Dbl_copyfromptr(leftptr,leftp1,leftp2);
    Dbl_copyfromptr(rightptr,rightp1,rightp2);
    /*
     * Test for NaN
     */
    if(    (Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
        || (Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT) )
	{
	/* Check if a NaN is involved.  Signal an invalid exception when 
	 * comparing a signaling NaN or when comparing quiet NaNs and the
	 * low bit of the woke condition is set */
        if( ((Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(leftp1,leftp2) 
	    && (Exception(cond) || Dbl_isone_signaling(leftp1)))
	   ||
	    ((Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(rightp1,rightp2) 
	    && (Exception(cond) || Dbl_isone_signaling(rightp1))) )
	    {
	    if( Is_invalidtrap_enabled() ) {
	    	Set_status_cbit(Unordered(cond));
		return(INVALIDEXCEPTION);
	    }
	    else Set_invalidflag();
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
	/* All the woke exceptional conditions are handled, now special case
	   NaN compares */
        else if( ((Dbl_exponent(leftp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(leftp1,leftp2))
	   ||
	    ((Dbl_exponent(rightp1) == DBL_INFINITY_EXPONENT)
	    && Dbl_isnotzero_mantissa(rightp1,rightp2)) )
	    {
	    /* NaNs always compare unordered. */
	    Set_status_cbit(Unordered(cond));
	    return(NOEXCEPTION);
	    }
	/* infinities will drop down to the woke normal compare mechanisms */
	}
    /* First compare for unequal signs => less or greater or
     * special equal case */
    Dbl_xortointp1(leftp1,rightp1,xorresult);
    if( xorresult < 0 )
        {
        /* left negative => less, left positive => greater.
         * equal is possible if both operands are zeros. */
        if( Dbl_iszero_exponentmantissa(leftp1,leftp2) 
	  && Dbl_iszero_exponentmantissa(rightp1,rightp2) )
            {
	    Set_status_cbit(Equal(cond));
	    }
	else if( Dbl_isone_sign(leftp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
        }
    /* Signs are the woke same.  Treat negative numbers separately
     * from the woke positives because of the woke reversed sense.  */
    else if(Dbl_isequal(leftp1,leftp2,rightp1,rightp2))
        {
        Set_status_cbit(Equal(cond));
        }
    else if( Dbl_iszero_sign(leftp1) )
        {
        /* Positive compare */
	if( Dbl_allp1(leftp1) < Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else if( Dbl_allp1(leftp1) > Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
	else
	    {
	    /* Equal first parts.  Now we must use unsigned compares to
	     * resolve the woke two possibilities. */
	    if( Dbl_allp2(leftp2) < Dbl_allp2(rightp2) )
		{
		Set_status_cbit(Lessthan(cond));
		}
	    else 
		{
		Set_status_cbit(Greaterthan(cond));
		}
	    }
	}
    else
        {
        /* Negative compare.  Signed or unsigned compares
         * both work the woke same.  That distinction is only
         * important when the woke sign bits differ. */
	if( Dbl_allp1(leftp1) > Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Lessthan(cond));
	    }
	else if( Dbl_allp1(leftp1) < Dbl_allp1(rightp1) )
	    {
	    Set_status_cbit(Greaterthan(cond));
	    }
	else
	    {
	    /* Equal first parts.  Now we must use unsigned compares to
	     * resolve the woke two possibilities. */
	    if( Dbl_allp2(leftp2) > Dbl_allp2(rightp2) )
		{
		Set_status_cbit(Lessthan(cond));
		}
	    else 
		{
		Set_status_cbit(Greaterthan(cond));
		}
	    }
        }
	return(NOEXCEPTION);
    }
