// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include "validation.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, bool fProofOfStake, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL ) { 
        LogPrint("mine", "init Limit %d ,%d\n" ,pindexLast->nHeight ,BLOCK_HEIGHT_INIT);
        return nProofOfWorkLimit;
    }
    
    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if(pindexPrev==NULL||pindexPrev->pprev==NULL){
        return nProofOfWorkLimit;
    }
    
    if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*5){//1min *5 no block, will reset difficulty
        if(fDebug){ 
            DbgMsg("targetspace :%d curTime:%d prevTime:%d " , 
                params.nPowTargetSpacing ,
                pblock->GetBlockTime() ,
                pindexLast->GetBlockTime() );
            LogPrint("mine", "%d height too old net... gap:%s sec, new:%08x , prev:%08x \n" ,
                pindexLast->nHeight,
                pblock->GetBlockTime() - pindexLast->GetBlockTime() , 
                nProofOfWorkLimit,pindexLast->nBits );
        }
        return nProofOfWorkLimit;
    }
    //too fast
    if (pblock->GetBlockTime() <  ( pindexLast->GetBlockTime() +  params.nPowTargetSpacing/3)&& pindexLast->nHeight >10000){
        unsigned int ret =pindexLast->nBits / 2;
        if(fDebug)
            LogPrint("mine", "prevhieght:%d too fast block %08x\n ",pindexLast->nHeight, ret);
        return ret;
    }
    

    // Go back by what we want to be 14 days worth of blocks
    // JBCoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    //
    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval()) // nPowTargetTimespan / nPowTargetSpacing;
        blockstogoback = params.DifficultyAdjustmentInterval(); 

    // Go back by what we want to be 1 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    
    for (int i = 0; pindexFirst && i < blockstogoback; i++){ 
        if(pindexFirst->nHeight<=1)//root
            break;
        pindexFirst = pindexFirst->pprev;
    }
    
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params,fProofOfStake);
}

static arith_uint256 GetTargetLimit(int64_t nTime, bool fProofOfStake, const Consensus::Params& params)
{
    uint256 nLimit;

    if (fProofOfStake) {//같은결과가 나온다. 향후 특정버전에서의 결과를 다르게 하기 위해서...
        nLimit = params.posLimit;
    } else {
        nLimit = params.powLimit;
    }

    return UintToArith256(nLimit);
}
/**
지정된 시간만큼으로 다음 난이도를 결정한다.
**/
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params,bool fProofOfStake)
{
    // 난이도조절을 하지 않게 했다면...
    if (params.fPowNoRetargeting){//fPowNoRetargeting = false
        LogPrint("mine", "noRetargeting\n");
        return pindexLast->nBits;
    }
    
    // Limit adjustment step , 진짜로 이용된 시간.
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;//1 day time span 
    // 86400 / 4 = 6 hour , min 4hour
    // 이용된 시간이 /4 작으면 /4 로 최소값과 최대값을 1/4 *4 로 제한한다.
    if (nActualTimespan < params.nPowTargetTimespan/4) //nPowTargetTimespan = 1day 86400
        nActualTimespan = params.nPowTargetTimespan/4;
    // max 4day
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;
    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    
    bnOld = bnNew;

    // JBCoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = GetTargetLimit(pindexLast->GetBlockTime(),fProofOfStake, params);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift){
        bnNew >>= 1;
    }
    bnNew *= nActualTimespan;//실제걸린시간.
    bnNew /= params.nPowTargetTimespan;//예상한 시간.
    if (fShift){ 
        bnNew <<= 1;
    }

    if (bnNew <= 0 ||bnNew > bnPowLimit){ 
        bnNew = bnPowLimit;
    }
    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit)){
        DbgMsg("fail1 target:%s limit:%s" , bnTarget.ToString(),params.powLimit.ToString());
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget){ 
        return false;
    }

    return true;
}
