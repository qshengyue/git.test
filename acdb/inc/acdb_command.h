#ifndef __ACDB_COMMAND_H__
#define __ACDB_COMMAND_H__
/**
*=============================================================================
* \file acdb_command.h
*
* \brief
*		Contains the definition of the commands exposed by the
*		ACDB public interface.
*
* \copyright
*  Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*=============================================================================
*/

/* ---------------------------------------------------------------------------
 * Include Files
 *--------------------------------------------------------------------------- */
#include "acdb.h"
#include "acdb_file_mgr.h"
#include "acdb_delta_file_mgr.h"
#include "acdb_utility.h"

/* ---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *--------------------------------------------------------------------------- */
/**< The number of elements in Global Buffer 1 */
#define GLB_BUF_1_LENGTH 2500
/**< The number of elements in Global Buffer 2 */
#define GLB_BUF_2_LENGTH 1000
/**< The number of elements in Global Buffer 3 */
#define GLB_BUF_3_LENGTH 500
/* ---------------------------------------------------------------------------
 * Type Declarations
 *--------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------
* Class Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Function Declarations and Documentation
 *--------------------------------------------------------------------------- */

int32_t AcdbCmdGetGraph(AcdbGraphKeyVector *gkv, AcdbGetGraphRsp *getGraphRsp,
    uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphData(AcdbSgIdGraphKeyVector *pInput,
    AcdbGetSubgraphDataRsp *pOutput, uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphConn(AcdbSubGraphList *pInput, AcdbBlob *pOutput,
    uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphCalDataNonPersist(AcdbSgIdCalKeyVector *pInput,
    AcdbBlob *pOutput, uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphCalDataPersist(AcdbSgIdCalKeyVector *pInput,
    AcdbSgIdPersistCalData *pOutput, uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphGlbPersistIds(AcdbSgIdCalKeyVector *pInput,
    AcdbGlbPsistIdentifierList *pOutput, uint32_t rsp_struct_size);

int32_t AcdbCmdGetSubgraphGlbPersistCalData(
    AcdbGlbPersistCalDataCmdType *pInput, AcdbBlob *pOutput,
    uint32_t rsp_struct_size);

int32_t AcdbCmdGetModuleTagData(AcdbSgIdModuleTag* sg_mod_tag, AcdbBlob* rsp,
    uint32_t rsp_size);

int32_t AcdbCmdGetTaggedModules(AcdbGetTaggedModulesReq* req,
    AcdbGetTaggedModulesRsp* rsp, uint32_t rsp_size);

int32_t AcdbCmdGetDriverData(AcdbDriverData *driver_data, AcdbBlob* rsp,
    uint32_t rsp_size );

int32_t AcdbCmdSetSubgraphCalData(AcdbSetCalibrationDataReq *req);

int32_t AcdbCmdGetAmdbRegData(AcdbAmdbProcID *req, AcdbBlob* rsp,
    uint32_t rsp_size);

int32_t AcdbCmdGetAmdbDeRegData(AcdbAmdbProcID *req, AcdbBlob* rsp,
    uint32_t rsp_size);

int32_t AcdbCmdGetSubgraphProcIds(AcdbCmdGetSubgraphProcIdsReq *req,
    AcdbCmdGetSubgraphProcIdsRsp* rsp, uint32_t rsp_size);

int32_t AcdbCmdGetAmdbBootupLoadModules(AcdbAmdbProcID *req,
    AcdbBlob* rsp, uint32_t rsp_size);

int32_t AcdbCmdGetTagsFromGkv(AcdbCmdGetTagsFromGkvReq *req,
    AcdbCmdGetTagsFromGkvRsp* rsp, uint32_t rsp_size);

int32_t AcdbCmdGetGraphCalKeyVectors(
    AcdbGraphKeyVector *gkv, AcdbKeyVectorList *rsp, uint32_t rsp_size);

int32_t AcdbCmdGetSupportedGraphKeyVectors(AcdbUintList *key_id_list,
    AcdbKeyVectorList *rsp, uint32_t rsp_size);

int32_t AcdbCmdGetDriverModuleKeyVectors(
    uint32_t module_id, AcdbKeyVectorList *rsp, uint32_t rsp_size);

int32_t AcdbCmdGetGraphTagKeyVectors(
    AcdbGraphKeyVector *gkv, AcdbTagKeyVectorList *rsp, uint32_t rsp_size);

int32_t AcdbCmdGetCalData(AcdbGetCalDataReq *req, AcdbBlob *rsp);

int32_t AcdbCmdGetTagData(AcdbGetTagDataReq *req, AcdbBlob *rsp);

int32_t AcdbCmdSetTagData(AcdbSetTagDataReq *req);

/******************************************************************************
* Functions Only Used by ATS
*******************************************************************************
*/

int32_t AcdbGetParameterCalData(AcdbParameterCalData *req, AcdbBlob* rsp);

int32_t AcdbGetParameterTagData(AcdbParameterTagData *req, AcdbBlob* rsp);
#endif /* __ACDB_COMMAND_H__ */
