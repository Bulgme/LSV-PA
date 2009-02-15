/**CFile****************************************************************

  FileName    [csw.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Cut sweeping.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - July 11, 2007.]

  Revision    [$Id: csw.h,v 1.00 2007/07/11 00:00:00 alanmi Exp $]

***********************************************************************/
 
#ifndef __CSW_H__
#define __CSW_H__

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif 

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                           ITERATORS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== cnfCore.c ========================================================*/
extern Aig_Man_t *     Csw_Sweep( Aig_Man_t * pAig, int nCutsMax, int nLeafMax, int fVerbose );

#ifdef __cplusplus
}
#endif

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

