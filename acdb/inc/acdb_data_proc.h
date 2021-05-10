#ifndef __ACDB_DATA_PROC_H__
#define __ACDB_DATA_PROC_H__
/**
*=============================================================================
* \file acdb_data_proc.h
*
* \brief
*      Processes data for the ACDB SW commands.
*
* \copyright
*  Copyright (c) 2019-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*=============================================================================
*/

#include "acdb_delta_file_mgr.h"

typedef enum _acdb_data_persistance_type_t {
    ACDB_DATA_NON_PERSISTENT = 0x0,
    ACDB_DATA_PERSISTENT,
	ACDB_DATA_GLOBALY_PERSISTENT,
	ACDB_DATA_UNKNOWN
} AcdbDataPersistanceType;

/**< Specifies the clients that make requests to ACDB SW*/
typedef enum _acdb_data_client_t {
    ACDB_DATA_ACDB_CLIENT = 0x0,  /**< A client of ACDB SW (e.g GSL)*/
    ACDB_DATA_ATS_CLIENT          /**< A client of ATS (e.g ARCT)*/
}AcdbDataClient;

typedef struct _processor_module_iid_t ProcIIDMap;
#include "acdb_begin_pack.h"
struct _processor_module_iid_t
{
	uint32_t procID;
	uint32_t iid_mid_count;
	AcdbModuleInstance iid_mid_list[0];
}
#include "acdb_end_pack.h"
;

typedef struct _subgraph_module_iid_t SubgraphModuleIIDMap;
#include "acdb_begin_pack.h"
struct _subgraph_module_iid_t
{
	uint32_t subgraph_id;
	uint32_t proc_count;
	uint32_t size;
	ProcIIDMap *proc_info;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_blob_module_info_t AcdbBlobModuleInfo;
#include "acdb_begin_pack.h"
struct _acdb_blob_module_info_t {
	//Offset of the module data within the AcdbBlob
	uint32_t blob_offset;

	//Subgraph that the module belongs to
	uint32_t subgraph_id;
}
#include "acdb_end_pack.h"
;

/**
* \brief
*		Determines the persistence type of a Parameter. Persistence types are
*       Persist, Non-persist, and shared(aka global)
* \param[in] pid: Parameter ID to check the persistence type for.
* \param[in/out] persistence_type: Pointer to persistence type
* \return 0 on success, non-zero on failure
*/
int32_t DataProcGetPersistenceType(uint32_t pid, AcdbDataPersistanceType *persistence_type);

/**
* \brief
*		Retrieves calibration from the heap for a subgrap
* \param[in] persist_type: Get subgraph data for non-persistent, persistent, or globally persistent calibration
* \param[in] client: Indicates where the request originates from (GSL, ARCT, etc...).
* This is used by the heap to determine what type of nodes can be saved to the delta file
* \param[in] key_vector: key vector used to retrieve data from the heap. Can be CKV/TKV.
* \param[in] subgraph_id: The subgraph to retrieve data for
* \param[in] module_iid: The module to retrieve data for
* \param[in] param_id: The parameter to retrieve data for
* \param[in] blob_offset: Offset of where to start writing data within the blob
* \param[in] blob: Calibration blob in the format MID, PID, ParamSize, Error Code, Payload
* \return 0 on success, non-zero on failure
*/
int32_t GetSubgraphCalData(
    AcdbDataPersistanceType persist_type, AcdbDataClient client,
	AcdbGraphKeyVector *key_vector,
	uint32_t subgraph_id, uint32_t module_iid, uint32_t param_id,
	uint32_t *blob_offset, AcdbBlob *blob);

//TODO: Are these two functions needed? Remove if not
//int32_t DataProcGetCalData();
//int32_t DataProcGetTagData();

/**
* \brief
*		Sets calibration data to the heap based on the Key Vector and Subgraphs
* \param[in] req: Key Vector and Subgraph data to set to the heap.
* \sa AcdbSetCalibrationDataReq
* \return 0 on success, non-zero on failure
*/
int32_t DataProcSetCalData(AcdbSetCalibrationDataReq *req);

/**
* \brief
*		Sets tag data to the heap based on the Key Vector and Subgraphs
* \param[in] req: Key Vector and Subgraph data to set to the heap.
* \sa AcdbSetTagDataReq
* \return 0 on success, non-zero on failure
*/
int32_t DataProcSetTagData(AcdbSetTagDataReq *req);

/**
* \brief
* Retrieves the mapping between a subgraph and its module instance IDs
*
* \param[in] subgraph_id: subgraph to retrieve the mapping for
* \param[in/out] sg_iid_map: subgraph to instance ID map. This is a double pointer.
* Ex. pass in address of pointer(&*) or double pointer(**)
* \return 0 on success, non-zero on failure
*/
int32_t GetSubgraphIIDMap(uint32_t subgraph_id, SubgraphModuleIIDMap **sg_iid_map);

/**
* \brief
* Free the subgraph module instance id map
*
* \param[in/out] sg_iid_map: subgraph instance id map to free
*/
void FreeSubgraphIIDMap(SubgraphModuleIIDMap *sg_iid_map);

/**
* \brief
* Retrieves information about the nodes in the ACDB Heap
* This includes:
* 1. Key Vector
* 2. Map Size
* 3. Key Vector Type (CKV or TKV)
*
* \param[in] rsp: response data containing key vector and map size information for each heap node
*
* \return 0 on success, non-zero on failure
*/
int32_t DataProcGetHeapInfo(AcdbBufferContext *rsp);

/**
* \brief
* Retrieves heap data for one node in the heap. Each heap node is associated
* with one key vector
*
* \param[in] key_vector: the key vector to retrieve heap data for
* \param[in] rsp: response containing subgraph calibration data retrieved from the heap
*
* \sa AcdbDeltaSubgraphData
* \return 0 on success, non-zero on failure
*/
int32_t DataProcGetHeapData(AcdbGraphKeyVector *key_vector, AcdbBufferContext* rsp, uint32_t *blob_offset);

/**
* \brief
*		Verifys whether a pid is globaly persistent.
* \param[in] pid: Parameter ID to verify for global persistance
* \return 0 on success, non-zero on failure
*/
int32_t IsPidGlobalyPersistent(uint32_t pid);

/**
* \brief
*		Verifys whether a pid is persistent.
* \param[in] pid: Parameter ID to verify for global persistance
* \return 0 on success, non-zero on failure
*/
int32_t IsPidPersistent(uint32_t pid);

/**
* \brief
* Retrieve tag data from the ACDB heap
*
* \param[in] aaaaaa:
*
* \return 0 on success, non-zero on failure
*/
//int32_t DataProcGetTagData(AcdbGraphKeyVector *tag_key_vector, uint32_t *blob_offset, AcdbBlob *blob);

#endif //__ACDB_DATA_PROC_H__
