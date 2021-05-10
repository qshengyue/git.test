#ifndef __ACDB_API_H__
#define __ACDB_API_H__
/**
*=============================================================================
* \file acdb.h
*
* \brief
*      Contains the public interface definitions for the Audio Calibration
*      Database (ACDB) module.
*
* \copyright
*  Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*=============================================================================
*/
/*===========================================================================
NOTE: The description above does not appear in the PDF.

The acdb_mainpage.dox file contains all file/group descriptions that are
in the output PDF generated using Doxygen and Latex. To edit or update
any of the file/group text in the PDF, edit the acdb_mainpage.dox file
(contact Tech Pubs).
===========================================================================*/
/* $Header: //components/rel/audio.acdb.casa/1.0/api/acdb.h#17 $ */

/*===========================================================================

	EDIT HISTORY FOR FILE

	This section contains comments describing changes made to this file.
	Notice that changes are listed in reverse chronological order.


	when        who     what, where, why
	----------  ---     -----------------------------------------------------
    2018-03-03  kde     ACDB SW initial draft
    2019-12-03  kde     ACDB SW Updated to v1.1
    2019-12-16  kde     ACDB SW Updated to v1.2
    2020-01-16  kde     ACDB SW v1.3: Added support for new APIs:
                        ACDB_CMD_GET_GRAPH_CAL_KVS
                        ACDB_CMD_GET_SUPPORTED_GKVS
                        ACDB_CMD_GET_DRIVER_MODULE_KVS
	2020-01-17  Mahadev Fix for ARAPPS-89
	2020-02-25  kde     ACDB SW v1.4: Updates to acdb_command.c due to
                        Hypervisor Feature
    2020-03-23  kde     ACDB SW v1.5
                        Updated codebase to use AR OSAL Heap and String APIs
    2020-04-06  kde     ACDB SW v1.6
                        Fixes made in acdb_command.c for get non-persist cal
    2020-04-06  kde     ACDB SW v1.7
                        Fixes made for Code Analysis warnings
===========================================================================*/

/* ---------------------------------------------------------------------------
* Include Files
*--------------------------------------------------------------------------- */
#include <stdlib.h>
#include "ar_osal_types.h"
#include "ar_osal_file_io.h"

#ifdef __cplusplus
extern "C"
{
#endif /*__cplusplus*/

/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */

#define ACDB_SOFTWARE_VERSION_MAJOR 0x00000001
#define ACDB_SOFTWARE_VERSION_MINOR 0x0000000B
#define ACDB_SOFTWARE_VERSION_REVISION 0x00000000
#define ACDB_SOFTWARE_VERSION_CPLINFO 0x00000000

#define MAX_FILENAME_LENGTH 256
#define MAX_ACDB_FILE_COUNT 20
#define INF 4294967295U

#define ACDB_CMD_ID(x) ((0xACDBUL << 16) | (0xFFFFUL & x)) /*! Ex. ACDB_CMD_ID(1) is 0xACDB0001*/
#define ACDB_MEM_CPY(dst,src,src_size) memcpy((uint8_t *)dst, (uint8_t *)src, src_size)
#define ACDB_MEM_CMP(x, y, len) memcmp(x,y,len)
#define ACDB_CLEAR_BUFFER(x) memset(&x, 0, sizeof(x))

extern uint32_t glb_buf_1[];
extern uint32_t glb_buf_2[];
extern uint32_t glb_buf_3[];

/* ---------------------------------------------------------------------------
* Type Declarations
*-------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Generic Query Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup GENERIC_STRUCTURES
@{ */

/**< Represents one key value pair.*/
typedef struct _acdb_key_value_t AcdbKeyValuePair;
#include "acdb_begin_pack.h"
struct _acdb_key_value_t {
    /**< Key ID*/
    uint32_t key;
    /**< Key value*/
    uint32_t value;
}
#include "acdb_end_pack.h"
;

/**< Generic response structure. BLOB stands for binary large object.*/
typedef struct _acdb_blob_t AcdbBlob;
#include "acdb_begin_pack.h"
struct _acdb_blob_t {
	/**< Buffer size. */
	uint32_t buf_size;
	/**< Point to the query buffer. */
	void* buf;
}
#include "acdb_end_pack.h"
;

/**< Maintains buffer context information.*/
typedef struct _acdb_buffer_context_t AcdbBufferContext;
#include "acdb_begin_pack.h"
struct _acdb_buffer_context_t
{
    /*Total size of the buffer 'buf'*/
    uint32_t size;
    /*Number of bytes filled in the buffer 'buf'*/
    uint32_t bytes_filled;
    /*The buffer that contains a data blob*/
    uint8_t *buf;
}
#include "acdb_end_pack.h"
;

/**< Request structure used to query for AMDB bootup
modules, registration, and deregistration data*/
typedef struct _acdb_amdb_proc_id_t AcdbAmdbProcID;
#include "acdb_begin_pack.h"
struct _acdb_amdb_proc_id_t
{
	/**< Processor ID*/
	uint32_t proc_id;
}
#include "acdb_end_pack.h"
;

/**< A Key Vector with a zero sized array */
typedef struct _acdb_key_vector_t AcdbKeyVector;
#include "acdb_begin_pack.h"
struct _acdb_key_vector_t {
    /**< number of keys*/
    uint32_t num_keys;
    /**< Graph key vector*/
    AcdbKeyValuePair graph_key_vector[0];
}
#include "acdb_end_pack.h"
;

/**< A Tag Key Vector with a zero sized array */
typedef struct _acdb_tag_key_vector_t AcdbTagKeyVector;
#include "acdb_begin_pack.h"
struct _acdb_tag_key_vector_t {
    /**< The module tag identifier*/
    uint32_t tag_id;
    /**< number of keys*/
    uint32_t num_keys;
    /**< Graph key vector*/
    AcdbKeyValuePair graph_key_vector[0];
}
#include "acdb_end_pack.h"
;

/**< List of uint32_t */
typedef struct _acdb_uint_list_t AcdbUintList;
struct _acdb_uint_list_t
{
    /**< number of uints in the list */
    uint32_t count;
    /**< List of uints */
    uint32_t *list;
};
/** @} */ /* end_addtogroup GENERIC_STRUCTURES */

/* ---------------------------------------------------------------------------
* Public API Declarations and Documentation
*-------------------------------------------------------------------------- */

typedef struct _acdb_file_t AcdbFile;
#include "acdb_begin_pack.h"
struct _acdb_file_t {
	uint32_t fileNameLen;
	/**< Full file path name length*/
	char fileName[MAX_FILENAME_LENGTH];
	/**< Array that holds the ACDB file path and name, which cannot exceed 256 characters, including the NULL-termiated character*/
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_data_files_t AcdbDataFiles;
#include "acdb_begin_pack.h"
struct _acdb_data_files_t {
	uint32_t num_files;
	/**< Number of ACDB files*/
	AcdbFile acdbFiles[MAX_ACDB_FILE_COUNT];
	/**< Array of ACDB file full paths. size: num_files*/
}
#include "acdb_end_pack.h"
;

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_GRAPH Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_GRAPH
@{ */

/**
	Queries ACDB SW for a subgraph sequence(a usecase graph) given a graph key vector.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_GRAPH.
	@param[in] cmd
		Pointer to AcdbGraphKeyVector.
	@param[in] cmd_size
		Size of AcdbGraphKeyVector.
	@param[out] rsp
		Pointer to AcdbGetGraphRsp
	@param[in] rsp_size
		Size of AcdbGetGraphRsp

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
	AcdbGraphKeyVectorCmd
	AcdbGetGraphRsp
*/
#define ACDB_CMD_GET_GRAPH	ACDB_CMD_ID(1)

typedef struct _acdb_subgraph_t AcdbSubgraph;
#include "acdb_begin_pack.h"
struct _acdb_subgraph_t {
	/**<src Subgraph ID*/
	uint32_t sg_id;
	/**<number of dst Subgraph IDs*/
	uint32_t num_dst_sgids;
	/**< dst subgraph IDs*/
	uint32_t dst_sg_ids[0];
}
#include "acdb_end_pack.h"
;

/**
	Input: Command structure used to qurey ACDB SW for subgraph information
	@sa acdb_get_graph
*/

typedef struct _acdb_graph_key_vector_t AcdbGraphKeyVector;
#include "acdb_begin_pack.h"
struct _acdb_graph_key_vector_t {
	/**< number of keys*/
	uint32_t num_keys;
	/**< Graph key vector*/
	AcdbKeyValuePair *graph_key_vector;
}
#include "acdb_end_pack.h"
;

/**
	Output: Response structure containing a subgraph and its dst subgraphs
	@sa acdb_get_graph
*/
typedef struct _acdb_get_graph_rsp_t AcdbGetGraphRsp;
#include "acdb_begin_pack.h"
struct _acdb_get_graph_rsp_t {
	/**< Number of SubGraphs for a given key combination*/
	uint32_t num_subgraphs;
	/**< Size of payload*/
	uint32_t size;
	/**< Pointer to array of SubGraphs data for a given key combination.*/
	AcdbSubgraph* subgraphs;
}
#include "acdb_end_pack.h"
;
/** @} */ /* end_addtogroup ACDB_CMD_GET_GRAPH */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUBGRAPH_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_SUBGRAPH_DATA
@{ */

/**
	Queries ACDB SW for container and module connection data (connections
	between modules within a subgraph) for a specific subgraph.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_SUBGRAPH_DATA.
	@param[in] cmd
		Pointer to AcdbSgIdGraphKeyVector.
	@param[in] cmd_size
		Size of AcdbSgIdGraphKeyVector.
	@param[out] rsp
		This is a pointer to AcdbGetSubgraphDataRsp.
		AcdbGetSubgraphDataRsp Description.
	@param[in] rsp_size
		This must equal the size of AcdbBlob.

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
	AcdbGetSubgraphDataRsp
*/
#define ACDB_CMD_GET_SUBGRAPH_DATA	ACDB_CMD_ID(2)

/**
	Input: Represents a subgrpah ID and GKV
*/
typedef struct _acdb_sgid_graph_key_vector_t AcdbSgIdGraphKeyVector;
#include "acdb_begin_pack.h"
struct _acdb_sgid_graph_key_vector_t {
	/**< number of SubGraph ID*/
	uint32_t num_sgid;
	/**< SubGraph ID*/
	uint32_t* sg_ids;
	/** <Graph key vector */
	AcdbGraphKeyVector graph_key_vector;
}
#include "acdb_end_pack.h"
;

/**
	Output: Represents a Subgraph's driver properties data and SPF properties data
*/

typedef struct _acdb_property_data_t AcdbPropertyData;
#include "acdb_begin_pack.h"
struct _acdb_property_data_t {
	/**< Property ID*/
	uint32_t prop_id;
	/**< property payload size*/
	uint32_t prop_payload_size;
	char prop_payload[0];
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_sub_graph_property_data_t AcdbSubGraphPropertyData;
#include "acdb_begin_pack.h"
struct _acdb_sub_graph_property_data_t {
	/**< SubGraph ID*/
	uint32_t sg_id;
	/**< number of properties*/
	uint32_t num_props;
	/**< Array of prop_data of type AcdbPropertyData*/
	char prop_data[0];
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_driver_property_data_t AcdbDriverPropertyData;
#include "acdb_begin_pack.h"
struct _acdb_driver_property_data_t {
	/**< number of SubGraph ID*/
	uint32_t num_sgid;
	/**< size of payload*/
	uint32_t size;
	AcdbSubGraphPropertyData* sub_graph_prop_data;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_get_subgraph_data_rsp_t AcdbGetSubgraphDataRsp;
#include "acdb_begin_pack.h"
struct _acdb_get_subgraph_data_rsp_t {

	AcdbDriverPropertyData driver_prop;
	/**< Size of container data.*/
	AcdbBlob spf_blob;
}
#include "acdb_end_pack.h"
;
/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUBGRAPH_CONNECTIONS Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_SUBGRAPH_CONNECTIONS
@{ */

/**
	Queries ACDB SW for data describing connections between and within
	subgraphs given a subgraph pair.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_SUBGRAPH_CONNECTIONS.
	@param[in] cmd
		This is a pointer to AcdbSubGraphList.
	@param[in] cmd_size
		This should equal the size of AcdbSubGraphList.
	@param[out] rsp
		This is a pointer to AcdbBlob including connection data between src and dst subgraphs.
	@param[in] rsp_size
		This should equal the size of AcdbBlob

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
	AcdbSubgraphPair
	AcdbConnectionData
*/
#define ACDB_CMD_GET_SUBGRAPH_CONNECTIONS		ACDB_CMD_ID(3)

/**
	input: structure containing some subgraphs and their dst subgraphs
	@sa acdb_get_graph
*/
typedef struct _acdb_subgraph_list_t AcdbSubGraphList;
#include "acdb_begin_pack.h"
struct _acdb_subgraph_list_t {
	/**< Number of SubGraphs for a given key combination*/
	uint32_t num_subgraphs;
	/**< Pointer to array of SubGraphs data for a given key combination.*/
	AcdbSubgraph* subgraphs;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_CONNECTIONS */


/* ---------------------------------------------------------------------------
 * ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST Declarations and Documentation
 *-------------------------------------------------------------------------- */
 /** @addtogroup ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST
 @{ */

 /**
 Queries ACDB SW for subgraph calibration data for non-persistent PIDs. The calibration data is in the format of:
 IID, PID, payload size, payload. . .


 @param[in] cmd_id
 Command ID is ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST.
 @param[in] cmd
 This is a pointer to AcdbSgIdCalKeyVector
 @param[in] cmd_size
 This is the size of AcdbSgIdCalKeyVector
 @param[out] rsp
 This is a pointer to AcdbBlob
 @param[in] rsp_size
 This is the size of AcdbBlob

 @return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.


 @sa
 acdb_ioctl
 */
#define ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST		ACDB_CMD_ID(4)

/** Input: Represents subgraph and query data type and CKVs
*/
typedef struct _acdb_sgid_calkey_vector_t AcdbSgIdCalKeyVector;
#include "acdb_begin_pack.h"
struct _acdb_sgid_calkey_vector_t {
	/**< Number of Subgraphs*/
	uint32_t num_sg_ids;
	/**< Subgraph IDs*/
	uint32_t* sg_ids;
	/**< Calibration Key vector prior*/
	AcdbGraphKeyVector cal_key_vector_prior;
	/**< Calibration Key vector new*/
	AcdbGraphKeyVector cal_key_vector_new;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST */

 /* ---------------------------------------------------------------------------
 * ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST Declarations and Documentation
 *-------------------------------------------------------------------------- */
 /** @addtogroup ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST
 @{ */

 /**
 Queries ACDB SW for subgraph calibration data for persistent PIDs. The calibration data is in the format of:
IID, PID, payload size, payload. . .

 @param[in] cmd_id
 Command ID is ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST.
 @param[in] cmd
 This is a pointer to AcdbSgIdCalKeyVector
 @param[in] cmd_size
 This is the size of AcdbSgIdCalKeyVector
 @param[out] rsp
 This is a pointer to AcdbSgIdsPersistCalData
 @param[in] rsp_size
 This is the size of AcdbSgIdsPersistCalData

 @return
		 - AR_EOK -- Command executed successfully.
		 - AR_EBADPARAM -- Invalid input parameters were provided.
		 - AR_EFAILED -- Command execution failed.


 @sa
 acdb_ioctl
 */
#define ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST		ACDB_CMD_ID(5)

/*Output*/

typedef struct _acdb_sgid_persist_data_t AcdbSgIdPersistData;
#include "acdb_begin_pack.h"
struct _acdb_sgid_persist_data_t {
	/**< Subgraph ID*/
	uint32_t sg_id;
	/**< Buffer size. */
	uint32_t persist_data_size;
	/**< persist cal data. */
	char buf[0];
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_sgid_persist_cal_data_t AcdbSgIdPersistCalData;
#include "acdb_begin_pack.h"
struct _acdb_sgid_persist_cal_data_t {
	/**< Number of Subgraphs*/
	uint32_t num_sg_ids;
	/**<total size*/
	uint32_t cal_data_size;
	/**< Persist cal data*/
	AcdbSgIdPersistData* cal_data;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS
@{ */

/**
	Queries ACDB SW for list of Global-Persistent PIDs identifier data.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS.
	@param[in] cmd
		A pointer to AcdbSgIdGlbPsistCalKeyVector
	@param[in] cmd_size
		This must equal the size of AcdbSgIdGlbPsistCalKeyVector
	@param[out] rsp
		Pointer to AcdbGlbPsistIdentifierList.
	@param[in] rsp_size
		This must equal the size of AcdbGlbPsistIdentifierList

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS		ACDB_CMD_ID(6)


/**
	Output: Represents a Subgraph's all global-persistent type pids identifier information
*/

typedef struct _acdb_global_persistent_cal_info_t AcdbGlbPsistCalInfo;
#include "acdb_begin_pack.h"
struct _acdb_global_persistent_cal_info_t {
	/**< cal identifier*/
	uint32_t cal_identifier;
	/**< number of iids using the pid*/
	uint32_t num_iids;
	/**< iid list*/
	uint32_t iids[0];
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_get_blb_psist_identifier_t AcdbGlbPsistIdentifierList;
#include "acdb_begin_pack.h"
struct _acdb_get_blb_psist_identifier_t {
	/**< number of global-persistent cal info entries*/
	uint32_t num_glb_persist_identifiers;
	/**< Size of payload*/
	uint32_t size;
	/**< Size of container data.*/
	AcdbGlbPsistCalInfo* global_persistent_cal_info ;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS */


/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA
@{ */

/**
	Query non-persistent or persistent calibration data for a subgraph’s based on calibration key
	information.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA.
	@param[in] cmd
		This is a pointer to AcdbGlbPersistCalDataCmdType
	@param[in] cmd_size
		This is the size of AcdbGlbPersistCalDataCmdType
	@param[out] rsp
		This is a pointer to AcdbBlob
	@param[in] rsp_size
		This is the size of AcdbBlob

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA		ACDB_CMD_ID(7)

typedef struct _acdb_get_subgraph_glb_persist_cal_data_cmd_type AcdbGlbPersistCalDataCmdType;
#include "acdb_begin_pack.h"
struct _acdb_get_subgraph_glb_persist_cal_data_cmd_type {
	/**<Cal Identifier> */
	uint32_t cal_Id;
}
#include "acdb_end_pack.h"
;
/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_DRIVER_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_DRIVER_DATA
@{ */

/**
	Queries ACDB SW for a specific parameter payload given the
	Module ID and a Key Vector(CKV/TKV/GKV/etc...).

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_DRIVER_DATA.
	@param[in] cmd
		A pointer to AcdbDriverGetDataCmd
	@param[in] cmd_size
		This must equal the size of AcdbDriverGetDataCmd
	@param[out] rsp
		Pointer to AcdbBlob. Blob contains PID,PayloadSize, Payload for all PIDs in that module.
	@param[in] rsp_size
		This must equal the size of AcdbBlob

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_DRIVER_DATA		ACDB_CMD_ID(8)

/**< Input: Represents component IDs along with a calibration key vector*/
typedef struct _acdb_driver_data_t AcdbDriverData;
#include "acdb_begin_pack.h"
struct _acdb_driver_data_t {
	/**< Key Vector associated with the driver data*/
	AcdbGraphKeyVector key_vector;
	/**< Driver Module ID*/
	uint32_t module_id;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_DRIVER_DATA */


/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_MODULE_TAG_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup ACDB_CMD_GET_MODULE_TAG_DATA
@{ */

/**
	Query for all necessary data for the given SGID, module_tag, and keys. Clients should provide “Don’t care” value for those tag indices which are not used in a tag_key_vec.
	For a given tag, all its indices should be provided with a value.

	@param[in] cmd_id
		Command ID is ACDB_CMD_GET_MODULE_TAG_DATA.
	@param[in] cmd
		This is a pointer to AcdbGetModuleTagCmd.
	@param[in] cmd_size
		This is the size of AcdbGetModuleTagCmd.
	@param[out] rsp
		This is a pointer to AcdbBlob.
	@param[in] rsp_size
		This is the size of AcdbBlob.

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
	AcdbCalDataRsp
*/
#define ACDB_CMD_GET_MODULE_TAG_DATA	 ACDB_CMD_ID(9)
/** Input:
*/

typedef struct _acdb_module_tag_t AcdbModuleTag;
#include "acdb_begin_pack.h"
struct _acdb_module_tag_t {
	/**< Module tag ID*/
	uint32_t tag_id;
	AcdbGraphKeyVector tag_key_vector;
}
#include "acdb_end_pack.h"
;

/** Input:
*/
typedef struct _acdb_sgid_module_tag_t AcdbSgIdModuleTag;
#include "acdb_begin_pack.h"
struct _acdb_sgid_module_tag_t {
	/**< Num of Subgraph IDs*/
	uint32_t num_sg_ids;
	/**< Subgraph IDs*/
	uint32_t* sg_ids;
	/**< Module tag*/
	AcdbModuleTag module_tag;

}
#include "acdb_end_pack.h"
;


/** @} */ /* end_addtogroup ACDB_CMD_GET_MODULE_TAG_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_TAGGED_MODULES Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_TAGGED_MODULES

@{ */

/**
	  Query for tagged module ID(s) and instance ID(s).

	  @param[in] cmd_id
	  Command ID is ACDB_CMD_GET_TAGGED_MODULES.
	  @param[in] cmd
	  This is a pointer to AcdbGetTaggedModulesReq.
	  @param[in] cmd_size
	  This is the size of AcdbGetTaggedModulesReq.
	  @param[out] rsp
	  This is a pointer to AcdbGetTaggedModulesRsp.
	  @param[in] rsp_size
	  This is the size of AcdbGetTaggedModulesRsp.

	  @return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	  @sa
	  acdb_ioctl
	  */
#define ACDB_CMD_GET_TAGGED_MODULES	 ACDB_CMD_ID(10)

//Input
typedef struct _acdb_get_tagged_modules_req_t AcdbGetTaggedModulesReq;
#include "acdb_begin_pack.h"
struct _acdb_get_tagged_modules_req_t {
	/**< Num of Subgraph IDs*/
	uint32_t num_sg_ids;
	/**< Subgraph ID list*/
	uint32_t* sg_ids;
	/**< tag ID*/
	uint32_t tag_id;
}
#include "acdb_end_pack.h"
;

//output

typedef struct _acdb_module_instance_t AcdbModuleInstance;
#include "acdb_begin_pack.h"
struct _acdb_module_instance_t {
	/**< Module ID*/
	uint32_t mid_id;
	/**< Module instance ID*/
	uint32_t mid_iid;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_get_tagged_modules_t AcdbGetTaggedModulesRsp;
#include "acdb_begin_pack.h"
struct _acdb_get_tagged_modules_t {
	/**< Number of tagged modules*/
	uint32_t num_tagged_mids;
	/**< Pointer to array of tagged modules*/
	AcdbModuleInstance* tagged_mid_list;
}
#include "acdb_end_pack.h"
;


/** @} */ /* end_addtogroup ACDB_CMD_GET_TAGGED_MODULES */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_KEY_CAPABILITIES Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_KEY_CAPABILITIES

@{ */

/**
	Query for tagged module ID(s) and instance ID(s).

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_KEY_CAPABILITIES.
	@param[in] cmd
	There is no input structure; set this to NULL.
	@param[in] cmd_size
	There is no input structure; set this to 0.
	@param[out] rsp
	There is no output structure; set this to NULL.
	@param[in] rsp_size
	There is no output structure; set this to 0.

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_KEY_CAPABILITIES	 ACDB_CMD_ID(11)

//output

typedef struct _acdb_get_key_capabilities_t AcdbGetKeyCapabilitiesRsp;
#include "acdb_begin_pack.h"
struct _acdb_get_key_capabilities_t {
	AcdbGraphKeyVector cal_key_vector;
	/**< Number of modules*/
	uint32_t num_modules;
	/**< Pointer to array of Module instances*/
	AcdbModuleInstance* module_Instances;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_KEY_CAPABILITIES */

/* ---------------------------------------------------------------------------
* ACDB_CMD_SET_CALIBRATION_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_SET_CALIBRATION_DATA
@{ */

 /**
	Set calibration data for a list of subgraphs and a given CKV. This is used for third party audio plugin GLS clients, who should query those third party module instances through module tag API firstly.
	Then set cal data for those module instances to Delta ACDB and SPF.

	@param[in] cmd_id
	Command ID is ACDB_CMD_SET_CALIBRATION_DATA.
	@param[in] cmd
	There is no input structure; set this to NULL.
	@param[in] cmd_size
	There is no input structure; set this to 0.
	@param[out] rsp
	There is no output structure; set this to NULL.
	@param[in] rsp_size
	There is no output structure; set this to 0.

	@return
		- AR_EOK -- Command executed successfully.
		- AR_EBADPARAM -- Invalid input parameters were provided.
		- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_SET_CALIBRATION_DATA	 ACDB_CMD_ID(12)

//input

typedef struct _acdb_set_calibration_data_t AcdbSetCalibrationDataReq;
#include "acdb_begin_pack.h"
struct _acdb_set_calibration_data_t {
	/**< Number of subgraph IDs*/
	uint32_t num_sg_ids;
	/**<Subgraph ID list */
	uint32_t* sg_ids;
	/**<cal key vector */
	AcdbGraphKeyVector cal_key_vector;
	/**< cal blob size*/
	uint32_t cal_blob_size;
	/**< cal blob*/
	void* cal_blob;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_SET_CALIBRATION_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_ENABLE_PERSISTANCE Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_ENABLE_PERSISTANCE
@{ */

/**
	Enable ACDB calibration data persistance. Calibration data set to ACDB
	will be persisted to a *.acdbdelta file

	@param[in] cmd_id
	Command ID is ACDB_CMD_ENABLE_PERSISTANCE.
	@param[in] cmd
	There is no input structure; set this to NULL.
	@param[in] cmd_size
	There is no input structure; set this to 0.
	@param[out] rsp
	There is no output structure; set this to NULL.
	@param[in] rsp_size
	There is no output structure; set this to 0.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_ENABLE_PERSISTANCE	 ACDB_CMD_ID(13)

/** @} */ /* end_addtogroup ACDB_CMD_ENABLE_PERSISTANCE */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_AMDB_REGISTRATION_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_AMDB_REGISTRATION_DATA
@{ */

/**
	Queries AMDB data for one Processor ID to register all dynamic custom
	modules

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_AMDB_REGISTRATION_DATA.
	@param[in] cmd
	This is a pointer to AcdbAmdbProcID.
	@param[in] cmd_size
	This is the size of AcdbAmdbProcID.
	@param[out] rsp
	This is a pointer to AcdbBlob.
	@param[in] rsp_size
	This is the size of AcdbBlob.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.
	- AR_ENOTEXIST -- No Proc ID is found.

	@sa
	acdb_ioctl
	AcdbAmdbProcID
*/
#define ACDB_CMD_GET_AMDB_REGISTRATION_DATA	 ACDB_CMD_ID(15)

/** @} */ /* end_addtogroup ACDB_CMD_GET_AMDB_REGISTRATION_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA
@{ */

/**
	Queries AMDB data for one Processor ID to deregister all dynamic custom
	modules

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA.
	@param[in] cmd
	This is a pointer to AcdbAmdbProcID.
	@param[in] cmd_size
	This is the size of AcdbAmdbProcID.
	@param[out] rsp
	This is a pointer to AcdbBlob. Blob contains procID, num of modules, moduleID1,...
	@param[in] rsp_size
	This is the size of AcdbBlob.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.
	- AR_ENOTEXIST -- No Proc ID is found.

	@sa
	acdb_ioctl
	AcdbAmdbProcID
*/
#define ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA	 ACDB_CMD_ID(16)

/** @} */ /* end_addtogroup ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUBGRAPH_PROCIDS Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_SUBGRAPH_PROCIDS
@{ */

/**
	Queries all supported proc ids for a given list of subgraphs

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_SUBGRAPH_PROCIDS.
	@param[in] cmd
	This is a pointer to AcdbCmdGetSubgraphProcIdsReq.
	@param[in] cmd_size
	This is the size of AcdbCmdGetSubgraphProcIdsReq.
	@param[out] rsp
	This is a pointer to AcdbCmdGetSubgraphProcIdsRsp.
	@param[in] rsp_size
	This is the size of AcdbCmdGetSubgraphProcIdsRsp.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.
	- AR_ENOTEXIST -- No Subgraph ID or Proc ID is found.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_SUBGRAPH_PROCIDS	 ACDB_CMD_ID(17)

/**< Maps the subgraph to a list of processors*/
typedef struct _acdb_sg_proc_id_map_t AcdbSgProcIdsMap;
#include "acdb_begin_pack.h"
struct _acdb_sg_proc_id_map_t
{
	/**< Subgraph ID*/
	uint32_t sg_id;
	/**< Number of Processor*/
	uint32_t num_proc_ids;
	/**< Array of Processor IDs*/
	uint32_t proc_ids[0];
}
#include "acdb_end_pack.h"
;

/**< Request structure that contains the list of subgraphs to retrieve
processor lists for*/
typedef struct _acdb_get_subgraph_proc_ids_req_t AcdbCmdGetSubgraphProcIdsReq;
#include "acdb_begin_pack.h"
struct _acdb_get_subgraph_proc_ids_req_t
{
	/**< Number of subgraphs*/
	uint32_t num_sg_ids;
	/**< Array of Subgraph IDs*/
	uint32_t *sg_ids;
}
#include "acdb_end_pack.h"
;

/**< Response structure that contains a list of mappings between subgraphs and
their associated processors*/
typedef struct _acdb_get_subgraph_proc_ids_rsp_t AcdbCmdGetSubgraphProcIdsRsp;
#include "acdb_begin_pack.h"
struct _acdb_get_subgraph_proc_ids_rsp_t
{
	/**< Number of subgraphs*/
	uint32_t num_sg_ids;
	/**< Size of sg_proc_ids array*/
	uint32_t size;
	/**< Array of maps that associate a subgraph to a list of processors*/
	AcdbSgProcIdsMap *sg_proc_ids;
}
#include "acdb_end_pack.h"
;
/** @} */ /* end_addtogroup ACDB_CMD_GET_SUBGRAPH_PROCIDS */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES
@{ */

/**
	Queries all supported proc ids for a given list of subgraphs

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES.
	@param[in] cmd
	This is a pointer to AcdbAmdbProcID.
	@param[in] cmd_size
	This is the size of AcdbAmdbProcID.
	@param[out] rsp
	This is a pointer to AcdbBlob. Blob contains procID, errorCode, Num of modules, moduleID1, lsw_handle1, msw_handle1, moduleID2, lsw_handle2, msw_handle2....(all are uint32_t type)
	@param[in] rsp_size
	This is the size of AcdbBlob.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.
	- AR_ENOTEXIST -- Proc ID is found.

	@sa
	acdb_ioctl
	AcdbAmdbProcID
*/
#define ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES	 ACDB_CMD_ID(18)

/** @} */ /* end_addtogroup ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_TAGS_FROM_GKV Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_TAGS_FROM_GKV
@{ */

/**
	Query ACDB SW for all the tags(and their associated modules) that are used
    within a usecase given the graph key vector

	@param[in] cmd_id
	Command ID is ACDB_CMD_GET_TAGS_FROM_GKV.
	@param[in] cmd
	This is a pointer to AcdbCmdGetTagsFromGkvReq.
	@param[in] cmd_size
	This is the size of AcdbCmdGetTagsFromGkvReq.
	@param[out] rsp
	This is a pointer to AcdbCmdGetTagsFromGkvRsp.
	@param[in] rsp_size
	This is the size of AcdbCmdGetTagsFromGkvRsp.

	@return
	- AR_EOK -- Command executed successfully.
	- AR_EBADPARAM -- Invalid input parameters were provided.
	- AR_EFAILED -- Command execution failed.
	- AR_ENOTEXIST - The grah key vector does not exist or there are no tags to return.

	@sa
	acdb_ioctl
*/
#define ACDB_CMD_GET_TAGS_FROM_GKV ACDB_CMD_ID(19)

/**< Request structure that contains the graph key vector*/
typedef struct _acdb_cmd_get_tags_from_gkv_req AcdbCmdGetTagsFromGkvReq;
#include "acdb_begin_pack.h"
struct _acdb_cmd_get_tags_from_gkv_req {
	/**< Graph Key Vector representing the usecase*/
	AcdbGraphKeyVector *graph_key_vector;
}
#include "acdb_end_pack.h"
;

/**< Contains a Modules ID and Instance ID*/
typedef struct _acdb_module AcdbModule;
#include "acdb_begin_pack.h"
struct _acdb_module {
	/**< Module ID*/
	uint32_t module_id;
	/**< Module Instance ID*/
	uint32_t module_iid;
}
#include "acdb_end_pack.h"
;

/**< Contains the mapping beween a Tag and one or more module*/
typedef struct _acdb_tag_module AcdbTagModule;
#include "acdb_begin_pack.h"
struct _acdb_tag_module {
	/**< Tag ID associated with the module(s)*/
	uint32_t tag_id;
	/**< Number of modules associated with this tag*/
	uint32_t num_modules;
	/**< List of modules associated with this tag*/
	AcdbModule modules[0];
}
#include "acdb_end_pack.h"
;

/**< Response structure that contains a list of Tag + Module(s) mapping*/
typedef struct _acdb_cmd_get_tags_from_gkv_rsp AcdbCmdGetTagsFromGkvRsp;
#include "acdb_begin_pack.h"
struct _acdb_cmd_get_tags_from_gkv_rsp {
	/**< Number of tags found*/
	uint32_t num_tags;
	/**< Size of tag_module_list in bytes*/
	uint32_t list_size;
	/**< List of Tags plus their associated module(s)*/
	AcdbTagModule *tag_module_list;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_TAGS_FROM_GKV */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_GRAPH_CAL_KVS Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_GRAPH_CAL_KVS
@{ */

/**
*    Queries for all SPF module calibration key vectors under a given graph
*    key vector.
*
*    Example:
*    If the ACDB file contains the usecase GKV:
*    <Stream, HandsetMic><DeviceRx, FluenceSM><Instance, 1>
*
*    and this usecase contains the calibration key Tables:
*
*    Volume Step | Sample Rate
*    ------------|------------
*    1           | 8k
*    2           | 8k
*
*    Volume Step
*    ------------
*    1
*    2
*
*    The resulting key vector list will contain:
*    CKV [] (default CKV aka zero CKV)
*    CKV [<Volume Step, 1>]
*    CKV [<Volume Step, 2>]
*    CKV [<Volume Step, 1><Sample Rate, 8k>]
*    CKV [<Volume Step, 2><Sample Rate, 8k>]
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_GRAPH_CAL_KVS.
*    @param[in] cmd
*    This is a pointer to AcdbGraphKeyVector.
*    @param[in] cmd_size
*    This is the size of AcdbGraphKeyVector.
*    @param[out] rsp
*    This is a pointer to AcdbKeyVectorList.
*    @param[in] rsp_size
*    This is the size of AcdbKeyVectorList.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- The graph key vector does not exist or no data was found.
*
*    @sa
*    acdb_ioctl
*/
#define ACDB_CMD_GET_GRAPH_CAL_KVS ACDB_CMD_ID(20)

/**< Maintains a list of key vectors */
typedef struct _acdb_key_vector_list_t AcdbKeyVectorList;
#include "acdb_begin_pack.h"
struct _acdb_key_vector_list_t
{
    /**< Number of key vectors in the key vector list */
    uint32_t num_key_vectors;
    /**< Size of the key vector list in bytes */
    uint32_t list_size;
    /**< List of Key Vectors in the format of
    [#keys, kvPair+,..., #keys, kvPair+] */
    AcdbKeyVector *key_vector_list;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_GRAPH_CAL_KVS */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_SUPPORTED_GKVS Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_SUPPORTED_GKVS
@{ */

/**
*    Queries for all graph key vectors that contain a subset of the provided
*    key ids. A graph key vector supports certain capabiliies if it matches
*    the key id subset. If the subset contains zero keys, this api will gather
*    all the graph key vectors defined in the ACDBs.
*
*    Example:
*    If the input keys contain:
*       <Sample Rate, Volume Step>
*
*    and the following tables are in the database:
*
*    Volume Step | Sample Rate
*    ------------|------------
*    1           | 8k
*    2           | 8k
*
*    Volume Step | Sample Rate | Bit Width
*    ------------|-------------|-----------
*    1           | 8k          | 16
*    1           | 8k          | 32
*    2           | 16k         | 16
*    2           | 16k         | 32
*
*    The resulting key vector list will contain:
*    GKV [<Volume Step, 1><Sample Rate, 8k>]
*    GKV [<Volume Step, 2><Sample Rate, 8k>]
*    GKV [<Volume Step, 1><Sample Rate, 8k><Bit Width, 16>]
*    GKV [<Volume Step, 1><Sample Rate, 8k><Bit Width, 32>]
*    GKV [<Volume Step, 2><Sample Rate, 16k><Bit Width, 16>]
*    GKV [<Volume Step, 2><Sample Rate, 16k><Bit Width, 32>]
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_SUPPORTED_GKVS.
*    @param[in] cmd
*    This is a pointer to AcdbUintList.
*    @param[in] cmd_size
*    This is the size of AcdbUintList.
*    @param[out] rsp
*    This is a pointer to AcdbKeyVectorList.
*    @param[in] rsp_size
*    This is the size of AcdbKeyVectorList.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- No data was found.
*
*    @sa
*    acdb_ioctl
*/
#define ACDB_CMD_GET_SUPPORTED_GKVS ACDB_CMD_ID(21)
/** @} */ /* end_addtogroup ACDB_CMD_GET_SUPPORTED_GKVS */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_DRIVER_MODULE_KVS Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_DRIVER_MODULE_KVS
@{ */

/**
*    Queries for all driver key vectors used by a driver module.
*
*    Example:
*    If the input keys contain:
*       <Sample Rate, Volume Step>
*
*    and the following tables are in the database:
*
*    Volume Step | Sample Rate
*    ------------|------------
*    1           | 8k
*
*    Volume Step | Sample Rate | Bit Width
*    ------------|-------------|-----------
*    1           | 8k          | 16
*    1           | 8k          | 32
*
*    The resulting key vector list will contain:
*    KV [<Volume Step, 1><Sample Rate, 8k>]
*    KV [<Volume Step, 1><Sample Rate, 8k><Bit Width, 16>]
*    KV [<Volume Step, 1><Sample Rate, 8k><Bit Width, 32>]
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_DRIVER_MODULE_KVS.
*    @param[in] cmd
*    This is a pointer to a uint32_t.
*    @param[in] cmd_size
*    This is the size of a uint32_t.
*    @param[out] rsp
*    This is a pointer to AcdbKeyVectorList.
*    @param[in] rsp_size
*    This is the size of AcdbKeyVectorList.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- No data was found.
*
*    @sa
*    acdb_ioctl
*/
#define ACDB_CMD_GET_DRIVER_MODULE_KVS ACDB_CMD_ID(22)
/** @} */ /* end_addtogroup ACDB_CMD_GET_DRIVER_MODULE_KVS */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_GRAPH_TAG_KVS Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_GRAPH_TAG_KVS
@{ */

/**
*    Retrieves all Tag and tag key vector variations defined in a usecase
*
*    Example:
*    Consider the following diagram and table:
*
*   + Subgraph 1  +     + Subgraph 2  +
*   | SGKV(A)     |     | SGKV(B)     |
*   |_____________|     |_____________|
*   | Module 1    |---->| Module 2    |
*   | (tag: SV)   |     | (tag: REI)  |
*   +-------------+     +-------------+
*
*     Tag                    | Tag Key Vector
*    ________________________|_______________________________________
*     Stream Volume(SV)      | Min Range | Max Range | Default Volume
*                            |_______________________________________
*                            | -0dB      | 10dB      | 0dB
*                            | -10dB     | 20dB      | 5dB
*                            | -20dB     | 30dB      | 10dB
*    ----------------------------------------------------------------
*     Render Endpoint Info   | Sample Rate | Bit Width | Num Ch
*     (REI)                  |_______________________________________
*                            | 48kHz       |16         | 2
*                            | 48kHz       |24         | 2
*                            | 48kHz       |32         | 2
*
*   If there were two subgraphs with two tagged modules in the usecase GKV[A,B],
*   the output of the API would be:
*
*   Stream Volume : [Min Range: -0db][Max Range: 10dB][Default Volume: 0dB]
*   Stream Volume : [Min Range: -10db][Max Range: 20dB][Default Volume: 5dB]
*   Stream Volume : [Min Range: -20db][Max Range: 30dB][Default Volume: 10dB]
*   Render Endpoint Info : [Sample Rate: 48kHz][Bit Width: 16][Num Ch: 2]
*   Render Endpoint Info : [Sample Rate: 48kHz][Bit Width: 24][Num Ch: 2]
*   Render Endpoint Info : [Sample Rate: 48kHz][Bit Width: 32][Num Ch: 2]
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_GRAPH_TAG_KVS.
*    @param[in] cmd
*    This is a pointer to AcdbGraphKeyVector.
*    @param[in] cmd_size
*    This is the size of AcdbGraphKeyVector.
*    @param[out] rsp
*    This is a pointer to AcdbTagKeyVectorList.
*    @param[in] rsp_size
*    This is the size of AcdbTagKeyVectorList.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- No data was found.
*
*    @sa
*    acdb_ioctl
*/
#define ACDB_CMD_GET_GRAPH_TAG_KVS ACDB_CMD_ID(23)

/**< Maintains a list of key vectors */
typedef struct _acdb_tag_key_vector_list_t AcdbTagKeyVectorList;
#include "acdb_begin_pack.h"
struct _acdb_tag_key_vector_list_t
{
    /**< Number of tag key vectors in the key vector list */
    uint32_t num_key_vectors;
    /**< Size of the tag key vector list in bytes */
    uint32_t list_size;
    /**< List of Tag Key Vectors in the format of
    [#keys, kvPair+,..., #keys, kvPair+] */
    AcdbTagKeyVector *key_vector_list;
}
#include "acdb_end_pack.h"
;
/** @} */ /* end_addtogroup ACDB_CMD_GET_GRAPH_TAG_KVS */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_CAL_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_CAL_DATA
@{ */

/**
*    Retrieves calibration under the specified subgraph and module
*    instance. If parameter_list.count = 0, this API will return calibration
*    for the entire module. If parameter_list.count > 0, this API will
*    return calibration for the specified parameters.
*
*    Module instances can be retrieved from that database using
*    ACDB_CMD_GET_TAGGED_MODULES or ACDB_CMD_GET_TAGS_FROM_GKV.
*    The module instance can be used here to retrieve data.
*
*    The output AcdbBlob is in the format of:
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*                              ...
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_CAL_DATA.
*    @param[in] cmd
*    This is a pointer to AcdbGetCalDataReq.
*    @param[in] cmd_size
*    This is the size of AcdbGetCalDataReq.
*    @param[out] rsp
*    This is a pointer to AcdbBlob.
*    @param[in] rsp_size
*    This is the size of AcdbBlob.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- No data was found.
*    - AR_ENEEDMORE -- Need more memory to write response
*
*    @sa
*    acdb_ioctl
*    ACDB_CMD_SET_CALIBRATION_DATA
*    ACDB_CMD_GET_TAGGED_MODULES
*    ACDB_CMD_GET_TAGS_FROM_GKV
*
*/
#define ACDB_CMD_GET_CAL_DATA ACDB_CMD_ID(24)

/**< Request structure for retrieving cal data for a module instance */
typedef struct _acdb_get_cal_data_req_t AcdbGetCalDataReq;
#include "acdb_begin_pack.h"
struct _acdb_get_cal_data_req_t {
    uint32_t subgraph_id;
    uint32_t module_iid;
    AcdbUintList parameter_list;
    AcdbGraphKeyVector cal_key_vector;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_CAL_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_GET_TAG_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_GET_TAG_DATA
@{ */

/**
*    Retrieves tag under the specified subgraph and module instance.
*    If parameter_list.count = 0, this API will return tag data
*    for the entire module. If parameter_list.count > 0, this API will
*    return calibration for the specified parameters.
*
*    Module instances can be retrieved from that database using
*    ACDB_CMD_GET_TAGGED_MODULES or ACDB_CMD_GET_TAGS_FROM_GKV.
*    The module instance can be used here to retrieve data.
*
*    The output AcdbBlob is in the format of:
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*                              ...
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*
*    @param[in] cmd_id
*    Command ID is ACDB_CMD_GET_TAG_DATA.
*    @param[in] cmd
*    This is a pointer to AcdbGetTagDataReq.
*    @param[in] cmd_size
*    This is the size of AcdbGetTagDataReq.
*    @param[out] rsp
*    This is a pointer to AcdbBlob.
*    @param[in] rsp_size
*    This is the size of AcdbBlob.
*
*    @return
*    - AR_EOK -- Command executed successfully.
*    - AR_EBADPARAM -- Invalid input parameters were provided.
*    - AR_EFAILED -- Command execution failed.
*    - AR_ENOTEXIST -- No data was found.
*    - AR_ENEEDMORE -- Need more memory to write response
*
*    @sa
*    acdb_ioctl
*   ACDB_CMD_SET_TAG_DATA
*
*/
#define ACDB_CMD_GET_TAG_DATA ACDB_CMD_ID(25)

/**< Request structure for retrieving tag data for a module instance */
typedef struct _acdb_get_tag_data_req_t AcdbGetTagDataReq;
#include "acdb_begin_pack.h"
struct _acdb_get_tag_data_req_t {
    uint32_t subgraph_id;
    uint32_t module_iid;
    AcdbUintList parameter_list;
    AcdbModuleTag module_tag;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_GET_TAG_DATA */

/* ---------------------------------------------------------------------------
* ACDB_CMD_SET_TAG_DATA Declarations and Documentation
*-------------------------------------------------------------------------- */
/** @addtogroup ACDB_CMD_SET_TAG_DATA
@{ */

/**
*	 Sets tag data to the AML heap and persist to delta file
*
*    The input blob is in the format of:
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*                              ...
*    +-------------+----------+------+------------+-------------+
*    | Instance ID | Param ID | Size | Error Code | Data[Size]  |
*    +-------------+----------+------+------------+-------------+
*
*	 @param[in] cmd_id
*	 Command ID is ACDB_CMD_SET_TAG_DATA.
*	 @param[in] cmd
*    This is a pointer to AcdbGetTagDataReq.
*	 @param[in] cmd_size
*    This is the size of AcdbGetTagDataReq.
*	 @param[out] rsp
*	 There is no output structure; set this to NULL.
*	 @param[in] rsp_size
*	 There is no output structure; set this to 0.
*
*	 @return
*		- AR_EOK -- Command executed successfully.
*		- AR_EBADPARAM -- Invalid input parameters were provided.
*		- AR_EFAILED -- Command execution failed.
*
*	 @sa
*	 acdb_ioctl
*    ACDB_CMD_GET_TAG_DATA
*    ACDB_CMD_ENABLE_PERSISTANCE
*
*/
#define ACDB_CMD_SET_TAG_DATA	 ACDB_CMD_ID(26)

/* Request structure for setting tag data to the AML heap and delta file */
typedef struct _acdb_set_tag_data_t AcdbSetTagDataReq;
#include "acdb_begin_pack.h"
struct _acdb_set_tag_data_t {
    /**< Number of subgraph IDs */
    uint32_t num_sg_ids;
    /**< Subgraph ID list */
    uint32_t* sg_ids;
    /** <cal key vector */
    AcdbModuleTag module_tag;
    /**< Size of the tag data blob */
    uint32_t blob_size;
    /**< Pointer to blob containing tag data */
    void* blob;
}
#include "acdb_end_pack.h"
;

/** @} */ /* end_addtogroup ACDB_CMD_SET_TAG_DATA */

/* ---------------------------------------------------------------------------
* Public Function API Definitions and Documentation
*-------------------------------------------------------------------------- */

/** @addtogroup PUBLIC_API
@param[in] AcdbDataFiles structure for holding ACDB DATA files
@return An ACDB Status Code
*/
int32_t acdb_init(AcdbDataFiles* acdb_data_files, AcdbFile* delta_file_path);

/** @addtogroup PUBLIC_API
@param[in] uint32 acdbdata_base_addr Base address for ACDB DATA
@return An ACDB Status Code
*/
int32_t acdb_flash_init(uint32_t* acdbdata_base_addr);

/** @addtogroup PUBLIC_API
*/
int32_t acdb_deinit(void);

/** @addtogroup PUBLIC_API
*/
int32_t acdbCmdIsPersistenceSupported(uint32_t *resp);


/** @ingroup ACDB_IOCTL

	Main entry function to the ACDB. This entry function takes any
	supported ACDB IOCTL and provides the appropriate response.

	See the individual command documentation for more details on the
	expectations of the command.

	@param[in] cmd_id
	Command ID to execute on the Audio Calibration Database. The
	cmd and the rsp must match the expected
	structures for that command. If not, the command fails.
	@param[in] cmd
	Pointer to the command structure.
	@param[in] cmd_size
	Size of the command structure.
	@param[out] rsp
	Pointer to the response structure.
	@param[in] rsp_size
	Size of the response structure.

	@return
	The result of the call as defined by the command.

	@dependencies
	None

*/
int32_t acdb_ioctl(uint32_t cmd_id,
	const void *cmd_struct, uint32_t cmd_struct_size,
	void *rsp_struct, uint32_t rsp_struct_size);

#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif /* __ACDB_API_H__ */



