/**CFile****************************************************************

  FileName    [aigPartReg.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [AIG package.]

  Synopsis    [Register partitioning algorithm.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - April 28, 2007.]

  Revision    [$Id: aigPartReg.c,v 1.00 2007/04/28 00:00:00 alanmi Exp $]

***********************************************************************/

#include "aig.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Aig_ManPre_t_         Aig_ManPre_t;

struct Aig_ManPre_t_
{
    // input data    
    Aig_Man_t *     pAig;            // seq AIG manager 
    Vec_Ptr_t *     vMatrix;         // register dependency
    int             nRegsMax;        // the max number of registers in the cluster
    // information about partitions
    Vec_Ptr_t *     vParts;          // the partitions
    char *          pfUsedRegs;      // the registers already included in the partitions
    // info about the current partition
    Vec_Int_t *     vRegs;           // registers of this partition
    Vec_Int_t *     vUniques;        // unique registers of this partition
    Vec_Int_t *     vFreeVars;       // free variables of this partition
    Vec_Flt_t *     vPartCost;       // costs of adding each variable
    char *          pfPartVars;      // input/output registers of the partition
};


////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_ManPre_t * Aig_ManRegManStart( Aig_Man_t * pAig )
{
    Aig_ManPre_t * p;
    p = ALLOC( Aig_ManPre_t, 1 );
    memset( p, 0, sizeof(Aig_ManPre_t) );
    p->pAig      = pAig;
    p->vMatrix   = Aig_ManSupportsRegisters( pAig );
    p->nRegsMax  = 500;
    p->vParts    = Vec_PtrAlloc(256);
    p->vRegs     = Vec_IntAlloc(256);
    p->vUniques  = Vec_IntAlloc(256);
    p->vFreeVars = Vec_IntAlloc(256);
    p->vPartCost = Vec_FltAlloc(256);
    p->pfUsedRegs = ALLOC( char, Aig_ManRegNum(p->pAig) );
    memset( p->pfUsedRegs, 0, sizeof(char) * Aig_ManRegNum(p->pAig) );
    p->pfPartVars  = ALLOC( char, Aig_ManRegNum(p->pAig) );
    return p;
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManRegManStop( Aig_ManPre_t * p )
{
    Vec_VecFree( (Vec_Vec_t *)p->vMatrix );
    if ( p->vParts )
        Vec_VecFree( (Vec_Vec_t *)p->vParts );
    Vec_IntFree( p->vRegs );
    Vec_IntFree( p->vUniques );
    Vec_IntFree( p->vFreeVars );
    Vec_FltFree( p->vPartCost );
    free( p->pfUsedRegs );
    free( p->pfPartVars );
    free( p );
}

/**Function*************************************************************

  Synopsis    [Computes the max-support register that is not taken yet.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ManRegFindSeed( Aig_ManPre_t * p )
{
    int i, iMax, nRegsCur, nRegsMax = -1;
    for ( i = 0; i < Aig_ManRegNum(p->pAig); i++ )
    {
        if ( p->pfUsedRegs[i] )
            continue;
        nRegsCur = Vec_IntSize( Vec_PtrEntry(p->vMatrix,i) );
        if ( nRegsMax < nRegsCur )
        {
            nRegsMax = nRegsCur;
            iMax = i;
        }
    }
    return iMax;
}

/**Function*************************************************************

  Synopsis    [Computes the next register to be added to the set.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Aig_ManRegFindBestVar( Aig_ManPre_t * p )
{
    Vec_Int_t * vSupp;
    int nNewVars, nNewVarsBest = AIG_INFINITY;
    int iVarFree, iVarSupp, iVarBest = -1, i, k;
    // go through the free variables
    Vec_IntForEachEntry( p->vFreeVars, iVarFree, i )
    {
//        if ( p->pfUsedRegs[iVarFree] )
//            continue;
        // get support of this variable
        vSupp = Vec_PtrEntry( p->vMatrix, iVarFree );
        // count the number of new vars
        nNewVars = 0;
        Vec_IntForEachEntry( vSupp, iVarSupp, k )
            nNewVars += !p->pfPartVars[iVarSupp];
        // quit if there is no new variables
        if ( nNewVars == 0 )
            return iVarFree;
        // compare the cost of this
        if ( nNewVarsBest > nNewVars )
        {
            nNewVarsBest = nNewVars;
            iVarBest = iVarFree;
        }
    }
    return iVarBest;
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManRegPartitionAdd( Aig_ManPre_t * p, int iReg )
{
    Vec_Int_t * vSupp;
    int RetValue, iVar, i;
    // make sure this is a new variable
//    assert( !p->pfUsedRegs[iReg] );
    if ( !p->pfUsedRegs[iReg] )
    {
        p->pfUsedRegs[iReg] = 1;
        Vec_IntPush( p->vUniques, iReg );
    }
    // remove it from the free variables
    if ( Vec_IntSize(p->vFreeVars) > 0 )
    {
        assert( p->pfPartVars[iReg] );
        RetValue = Vec_IntRemove( p->vFreeVars, iReg );
        assert( RetValue );
    }
    else
        assert( !p->pfPartVars[iReg] );
    // add it to the partition
    p->pfPartVars[iReg] = 1;
    Vec_IntPush( p->vRegs, iReg );
    // add new variables
    vSupp = Vec_PtrEntry( p->vMatrix, iReg );
    Vec_IntForEachEntry( vSupp, iVar, i )
    {
        if ( p->pfPartVars[iVar] )
            continue;
        p->pfPartVars[iVar] = 1;
        Vec_IntPush( p->vFreeVars, iVar );
    }
    // add it to the cost
    Vec_FltPush( p->vPartCost, 1.0*Vec_IntSize(p->vFreeVars)/Vec_IntSize(p->vRegs) );
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Man_t * Aig_ManRegCreatePart( Aig_Man_t * pAig, Vec_Int_t * vPart, int * pnCountPis, int * pnCountRegs )
{
    Aig_Man_t * pNew;
    Aig_Obj_t * pObj;
    Vec_Ptr_t * vNodes;
    Vec_Ptr_t * vRoots;
    int nOffset, iOut, i;
    int nCountPis, nCountRegs;
    // collect roots
    vRoots = Vec_PtrAlloc( Vec_IntSize(vPart) );
    nOffset = Aig_ManPoNum(pAig)-Aig_ManRegNum(pAig);
    Vec_IntForEachEntry( vPart, iOut, i )
    {
        pObj = Aig_ManPo(pAig, nOffset+iOut);
        Vec_PtrPush( vRoots, Aig_ObjFanin0(pObj) );
    }
    // collect/mark nodes/PIs in the DFS order
    vNodes = Aig_ManDfsNodes( pAig, (Aig_Obj_t **)Vec_PtrArray(vRoots), Vec_PtrSize(vRoots) );
    Vec_PtrFree( vRoots );
    // unmark register outputs
    nOffset = Aig_ManPiNum(pAig)-Aig_ManRegNum(pAig);
    Vec_IntForEachEntry( vPart, iOut, i )
    {
        pObj = Aig_ManPi(pAig, nOffset+iOut);
        Aig_ObjSetTravIdPrevious( pAig, pObj );
    }
    // count pure PIs
    nCountPis = nCountRegs = 0;
    Aig_ManForEachPiSeq( pAig, pObj, i )
        nCountPis += Aig_ObjIsTravIdCurrent(pAig, pObj);
    // count outputs of other registers
    Aig_ManForEachLoSeq( pAig, pObj, i )
        nCountRegs += Aig_ObjIsTravIdCurrent(pAig, pObj);
    if ( pnCountPis )
        *pnCountPis = nCountPis;
    if ( pnCountRegs )
        *pnCountRegs = nCountRegs;
    // create the new manager
    pNew = Aig_ManStart( Vec_PtrSize(vNodes) );
    Aig_ManConst1(pAig)->pData = Aig_ManConst1(pNew);
    // create the PIs
    Aig_ManForEachPi( pAig, pObj, i )
        if ( Aig_ObjIsTravIdCurrent(pAig, pObj) )
            pObj->pData = Aig_ObjCreatePi(pNew);
    // add variables for the register outputs
    // create fake POs to hold the register outputs
    Vec_IntForEachEntry( vPart, iOut, i )
    {
        pObj = Aig_ManPi(pAig, nOffset+iOut);
        pObj->pData = Aig_ObjCreatePi(pNew);
        Aig_ObjCreatePo( pNew, pObj->pData );
    }
    // create the nodes
    Vec_PtrForEachEntry( vNodes, pObj, i )
        if ( Aig_ObjIsNode(pObj) )
            pObj->pData = Aig_And(pNew, Aig_ObjChild0Copy(pObj), Aig_ObjChild1Copy(pObj) );
    Vec_PtrFree( vNodes );
    // add real POs for the registers
    nOffset = Aig_ManPoNum(pAig)-Aig_ManRegNum(pAig);
    Vec_IntForEachEntry( vPart, iOut, i )
    {
        pObj = Aig_ManPo( pAig, nOffset+iOut );
        Aig_ObjCreatePo( pNew, Aig_ObjChild0Copy(pObj) );
    }
    pNew->nRegs = Vec_IntSize(vPart);
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Aig_ManRegPartitionSmart( Aig_Man_t * pAig )
{
    extern void Ioa_WriteAiger( Aig_Man_t * pMan, char * pFileName, int fWriteSymbols, int fCompact );

    Aig_ManPre_t * p;
    Vec_Ptr_t * vResult;
    int iSeed, iNext, i, k;
    // create the manager
    p = Aig_ManRegManStart( pAig );
    // add partitions as long as registers remain
    for ( i = 0; (iSeed = Aig_ManRegFindSeed(p)) >= 0; i++ )
    {
printf( "Seed variable = %d.\n", iSeed );
        // clean the current partition information
        Vec_IntClear( p->vRegs );
        Vec_IntClear( p->vUniques );
        Vec_IntClear( p->vFreeVars );
        Vec_FltClear( p->vPartCost );
        memset( p->pfPartVars, 0, sizeof(char) * Aig_ManRegNum(p->pAig) );
        // add the register and its partition support
        Aig_ManRegPartitionAdd( p, iSeed );
        // select the best var to add
        for ( k = 0; Vec_IntSize(p->vRegs) < p->nRegsMax; k++ )
        {
            // get the next best variable
            iNext = Aig_ManRegFindBestVar( p );
            if ( iNext == -1 )
                break;
            // add the register to the support of the partition
            Aig_ManRegPartitionAdd( p, iNext );
            // report the result
printf( "Part %3d  Reg %3d : Free = %4d. Total = %4d. Ratio = %6.2f. Unique = %4d.\n", i, k, 
                Vec_IntSize(p->vFreeVars), Vec_IntSize(p->vRegs), 
                1.0*Vec_IntSize(p->vFreeVars)/Vec_IntSize(p->vRegs), Vec_IntSize(p->vUniques) );
            // quit if there are not free variables
            if ( Vec_IntSize(p->vFreeVars) == 0 )
                break;
        }
        // add this partition to the set
        Vec_PtrPush( p->vParts, Vec_IntDup(p->vRegs) );        
printf( "Part %3d  SUMMARY:  Free = %4d. Total = %4d. Ratio = %6.2f. Unique = %4d.\n", i,
                Vec_IntSize(p->vFreeVars), Vec_IntSize(p->vRegs), 
                1.0*Vec_IntSize(p->vFreeVars)/Vec_IntSize(p->vRegs), Vec_IntSize(p->vUniques) );
printf( "\n" ); 
    }
    vResult = p->vParts; p->vParts = NULL;
    Aig_ManRegManStop( p );
    return vResult;
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Aig_ManRegPartitionSimple( Aig_Man_t * pAig, int nPartSize )
{
    Vec_Ptr_t * vResult;
    Vec_Int_t * vPart;
    int i, k, nParts;
    nParts = (Aig_ManRegNum(pAig) / nPartSize) + (int)(Aig_ManRegNum(pAig) % nPartSize > 0);
    vResult = Vec_PtrAlloc( nParts );
    for ( i = 0; i < nParts; i++ )
    {
        vPart = Vec_IntAlloc( nPartSize );
        for ( k = 0; k < nPartSize; k++ )
            if ( i * nPartSize + k < Aig_ManRegNum(pAig) )
                Vec_IntPush( vPart, i * nPartSize + k );
        Vec_PtrPush( vResult, vPart );
    }
    return vResult;
}

/**Function*************************************************************

  Synopsis    [Computes partitioning of registers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Aig_ManRegPartitionRun( Aig_Man_t * pAig )
{
    int nPartSize = 1000;
    char Buffer[100];
    Aig_Man_t * pTemp;
    Vec_Ptr_t * vResult;
    Vec_Int_t * vPart;
    int i, nCountPis, nCountRegs;
    vResult = Aig_ManRegPartitionSimple( pAig, nPartSize );
    printf( "Simple partitioning: %d partitions are saved:\n", Vec_PtrSize(vResult) );
    Vec_PtrForEachEntry( vResult, vPart, i )
    {
        sprintf( Buffer, "part%03d.aig", i );
        pTemp = Aig_ManRegCreatePart( pAig, vPart, &nCountPis, &nCountRegs );
        Ioa_WriteAiger( pTemp, Buffer, 0, 0 );
        printf( "part%03d.aig : Regs = %4d. PIs = %4d. (True PIs = %4d. Other regs = %4d.)\n", 
            i, Vec_IntSize(vPart), Aig_ManPiNum(pTemp)-Vec_IntSize(vPart), nCountPis, nCountRegs );
        Aig_ManStop( pTemp );
    }
    Vec_VecFree( (Vec_Vec_t *)vResult );
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


