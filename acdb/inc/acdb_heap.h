#ifndef __ACDB_HEAP_H__
#define __ACDB_HEAP_H__
/**
*==============================================================================
* \file acdb_heap.h
*
* \brief
*		Manages the calibration data set by ACDB SW clients in RAM.
*
* \copyright
*  Copyright (c) 2019-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
* 
*==============================================================================
*/

#include "acdb_delta_parser.h"

typedef struct _tree_node_t TreeNode;
struct _tree_node_t
{
	int32_t depth;      /**< Tree depth of the node */
	void *p_struct;     /**< Data stored in the node */
	TreeNode *left;     /**< Left subtree */
	TreeNode *right;    /**< Right subtree */
	TreeNode *parent;   /**< parent node */
    uint32_t type;      /**< Node Type (GSL, ARCT,...) */
};

typedef struct _heap_info_t HeapInfo;
struct _heap_info_t
{
	/**< Delta Acdb File index */
	int32_t file_index;
    /**< Root of the associated tree */
	TreeNode *root;
};

typedef struct _acdb_delta_heap_info_t AcdbDeltaHeapInfo;
struct _acdb_delta_heap_info_t
{
    /**< Number of Delta Acdb files */
	/** int32_t file_count; */
	/**< List of heap information */
	HeapInfo *heap_info[MAX_ACDB_FILE_COUNT];
};

typedef struct _kv_length_bin_t KVSubgraphMapBin;
struct _kv_length_bin_t
{
    uint32_t str_key_vector_size;
    char *str_key_vector;
    /**< Key Vector to Subgraph Map */
    AcdbDeltaKVToSubgraphDataMap *map;
};

//typedef struct _ckv_length_bin_t CkvSubgraphMapBin;
//struct _ckv_length_bin_t
//{
//	uint32_t str_key_vector_size;
//	char *str_key_vector;
//	/**< The list of CKV to Subgraph Maps */
//	LinkedList list;
//};

enum TreeDirection {
	TREE_DIR_LEFT = -1, 
	TREE_DIR_NONE,
	TREE_DIR_RIGHT
};

enum AcdbHeapCmd {
	ACDB_HEAP_CMD_INIT = 0,
	ACDB_HEAP_CMD_CLEAR,
	ACDB_HEAP_CMD_ADD_MAP,
	ACDB_HEAP_CMD_GET_MAP,
	ACDB_HEAP_CMD_GET_MAP_LIST,
	ACDB_HEAP_CMD_REMOVE_MAP,
    ACDB_HEAP_CMD_GET_HEAP_INFO,
};

void free_map(AcdbDeltaKVToSubgraphDataMap *map);
AcdbGraphKeyVector *get_key_vector_from_map(AcdbDeltaKVToSubgraphDataMap* map);
int32_t acdb_heap_ioctl(uint32_t cmd_id,
	uint8_t *pInput,
	uint8_t *pOutput,
	uint32_t nOutputSize);

#endif /*__ACDB_HEAP_H__*/
