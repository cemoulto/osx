/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mpi/f77/bindings.h"
#include "ompi/constants.h"
#include "ompi/communicator/communicator.h"
#include "ompi/mpi/f77/strings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_INFO_GET_VALUELEN = mpi_info_get_valuelen_f
#pragma weak pmpi_info_get_valuelen = mpi_info_get_valuelen_f
#pragma weak pmpi_info_get_valuelen_ = mpi_info_get_valuelen_f
#pragma weak pmpi_info_get_valuelen__ = mpi_info_get_valuelen_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_INFO_GET_VALUELEN,
                            pmpi_info_get_valuelen,
                            pmpi_info_get_valuelen_,
                            pmpi_info_get_valuelen__,
                            pmpi_info_get_valuelen_f,
                            (MPI_Fint *info, char *key, MPI_Fint *valuelen, MPI_Flogical *flag, MPI_Fint *ierr, int key_len),
                            (info, key, valuelen, flag, ierr, key_len) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_INFO_GET_VALUELEN = mpi_info_get_valuelen_f
#pragma weak mpi_info_get_valuelen = mpi_info_get_valuelen_f
#pragma weak mpi_info_get_valuelen_ = mpi_info_get_valuelen_f
#pragma weak mpi_info_get_valuelen__ = mpi_info_get_valuelen_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_INFO_GET_VALUELEN,
                            mpi_info_get_valuelen,
                            mpi_info_get_valuelen_,
                            mpi_info_get_valuelen__,
                            mpi_info_get_valuelen_f,
                            (MPI_Fint *info, char *key, MPI_Fint *valuelen, MPI_Flogical *flag, MPI_Fint *ierr, int key_len),
                            (info, key, valuelen, flag, ierr, key_len) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_INFO_GET_VALUELEN";

/* Note that the key_len parameter is silently added by the Fortran
   compiler, and will be filled in with the actual length of the
   character array from the caller.  Hence, it's the max length of the
   string that we can use. */

void mpi_info_get_valuelen_f(MPI_Fint *info, char *key,
                             MPI_Fint *valuelen, MPI_Flogical *flag,
                             MPI_Fint *ierr, int key_len)
{
    int c_err, ret;
    MPI_Info c_info;
    char *c_key;
    OMPI_SINGLE_NAME_DECL(valuelen);
    OMPI_LOGICAL_NAME_DECL(flag);

    if (OMPI_SUCCESS != (ret = ompi_fortran_string_f2c(key, key_len, &c_key))) {
        c_err = OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, ret, FUNC_NAME);
        *ierr = OMPI_INT_2_FINT(c_err);
        return;
    }
    c_info = MPI_Info_f2c(*info);
    *ierr = OMPI_INT_2_FINT(MPI_Info_get_valuelen(c_info, c_key,
                                                  OMPI_SINGLE_NAME_CONVERT(valuelen),
                                                  OMPI_LOGICAL_SINGLE_NAME_CONVERT(flag)));
    if (MPI_SUCCESS == OMPI_FINT_2_INT(*ierr)) {
        OMPI_SINGLE_INT_2_FINT(valuelen);
        OMPI_SINGLE_INT_2_LOGICAL(flag);
    }

    free(c_key);
}
