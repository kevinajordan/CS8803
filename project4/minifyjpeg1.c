/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "minifyjpeg.h"

bool_t
xdr_in_pointer (XDR *xdrs, in_pointer *objp)
{
	register int32_t *buf;

	 if (!xdr_pointer (xdrs, (char **)&objp->arg1, sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->arg2, sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_minimize_image_1_argument (XDR *xdrs, minimize_image_1_argument *objp)
{
	 if (!xdr_in_pointer (xdrs, &objp->arg1))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->arg2))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->arg3, sizeof (int), (xdrproc_t) xdr_int))
		 return FALSE;
	return TRUE;
}
