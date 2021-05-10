#ifndef __ACDB_DELTA_PARSER_H__
#define __ACDB_DELTA_PARSER_H__
/**
*=============================================================================
* \file acdb_delta_parser.h
*
* \brief
*		The interface of the Acdb Delta Parser/Generator that handles parsing
*		and/or generating the ACDB Delta file.
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
#include "ar_osal_types.h"
#include "ar_osal_file_io.h"

#include "acdb.h"
#include "acdb_utility.h"
/* ---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *--------------------------------------------------------------------------- */

#define ACDB_DELTA_FILE_VERSION_MAJOR	0x00000001
#define ACDB_DELTA_FILE_VERSION_MINOR	0x00000000
#define ACDB_DELTA_FILE_REVISION        0x00000000

/* ---------------------------------------------------------------------------
 * Type Declarations
 *--------------------------------------------------------------------------- */

typedef struct _acdb_delta_file_info_t AcdbDeltaFileInfo;
#include "acdb_begin_pack.h"
struct _acdb_delta_file_info_t
{
	int32_t fileType;
	uint32_t major;
	uint32_t minor;
	uint32_t revision;
	uint32_t cplInfo;
}
#include "acdb_end_pack.h"
;

typedef struct _AcdbCmdDeltaFileInfo AcdbCmdDeltaFileInfo;
#include "acdb_begin_pack.h"
struct _AcdbCmdDeltaFileInfo {
	uint32_t file_index;
	uint32_t is_updated;
	uint32_t exists;
	uint32_t file_size;
	ar_fhandle file_handle;
	AcdbFile delta_file_path;
	AcdbDeltaFileInfo file_info;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_module_cal_data_t AcdbDeltaModuleCalData;
#include "acdb_begin_pack.h"
struct _acdb_delta_module_cal_data_t {
	uint32_t module_iid;
	uint32_t param_id;
	uint32_t param_size;
	void *param_payload;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_persistence_payload_t AcdbDeltaPersistanceData;
#include "acdb_begin_pack.h"
struct _acdb_delta_persistence_payload_t {
	uint32_t data_size;
	LinkedList cal_data_list;//List of AcdbDeltaModuleCalData
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_subgraph_data_t AcdbDeltaSubgraphData;
#include "acdb_begin_pack.h"
struct _acdb_delta_subgraph_data_t {
	uint32_t subgraph_id;
	uint32_t subgraph_data_size;
	AcdbDeltaPersistanceData non_global_data;
	AcdbDeltaPersistanceData global_data;
}
#include "acdb_end_pack.h"
;

/**< Maps a Key Vector (CKV or TKV) to a list of subgraphs*/
typedef struct _acdb_delta_kv_to_subgraph_data_map_t AcdbDeltaKVToSubgraphDataMap;
#include "acdb_begin_pack.h"
struct _acdb_delta_kv_to_subgraph_data_map_t {
    /**< Key vector type that describes the data in key_vector_data. 
    If type=TAG_KEY_VECTOR key_vector_data=AcdbModuleTag. 
    If type=CAL_KEY_VECTOR key_vector_data=AcdbGraphKeyVector */
    KeyVectorType key_vector_type;
    /**< Pointer to AcdbGraphKeyVector Calibration key Vector or AcdbModuleTag*/
    void *key_vector_data;
    /**< Size of the Map*/
    uint32_t map_size;
    /**< Number of subgraphs in the map*/
    uint32_t num_subgraphs;
    /**< List of AcdbDeltaSubgraphData*/
    LinkedList subgraph_data_list;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_file_version_t AcdbDeltaFileVersion;
#include "acdb_begin_pack.h"
struct _acdb_delta_file_version_t {
	uint32_t major;
	uint32_t minor;
	uint32_t revision;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_file_header_t AcdbDeltaFileHeader;
#include "acdb_begin_pack.h"
struct _acdb_delta_file_header_t {
	AcdbDeltaFileVersion file_version;
	uint32_t acdb_major;
	uint32_t acdb_minor;
	uint32_t acdb_revision;
	uint32_t acdb_cpl_info;
	uint32_t file_data_size;
	uint32_t map_count;
}
#include "acdb_end_pack.h"
;

typedef struct _delta_chunk_header_t AcdbDeltaChunkHeader;
#include "acdb_begin_pack.h"
struct _delta_chunk_header_t {
	AcdbGraphKeyVector cal_key_vector;
	uint32_t map_size;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_delta_map_chunk_info AcdbDeltaMapChunkInfo;
#include "acdb_begin_pack.h"
struct _acdb_delta_map_chunk_info {
	uint32_t map_offset;
	uint32_t map_size;
}
#include "acdb_end_pack.h"
;

/* ---------------------------------------------------------------------------
 * Function Declarations and Documentation
 *--------------------------------------------------------------------------- */

int32_t IsAcdbDeltaFileValid(ar_fhandle fhandle, uint32_t file_size);

int32_t AcdbDeltaParserGetSwVersion(ar_fhandle fhandle, uint32_t file_size, AcdbDeltaFileInfo* delta_finfo);

//int32_t AcdbDeltaParserGetCalData(ar_fhandle fp);

//int32_t AcdbDeltaParserGetMapChunk(ar_fhandle fhandle, const uint32_t file_size, const AcdbGraphKeyVector ckv, AcdbDeltaMapChunkInfo *map_info);

int32_t AcdbDeltaParserGetMapChunkInfo(ar_fhandle fhandle, uint32_t *offset, AcdbDeltaMapChunkInfo *map_info);

int32_t ReadMapChunk(ar_fhandle fhandle, uint32_t *offset, AcdbDeltaKVToSubgraphDataMap *map);

int32_t WriteDeltaFileHeader(ar_fhandle fhandle, AcdbDeltaFileInfo* delta_finfo, uint32_t fdata_size, uint32_t map_count);

int32_t WriteMapChunk(ar_fhandle fhandle, AcdbDeltaKVToSubgraphDataMap *map);

#endif /* __ACDB_DELTA_PARSER_H__ */
