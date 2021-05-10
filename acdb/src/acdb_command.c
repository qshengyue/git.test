
/**
*=============================================================================
* \file acdb_command.c
*
* \brief
*		Contains the implementation of the commands exposed by the
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

#include <math.h>

#include "acdb_command.h"
#include "acdb_parser.h"
#include "acdb_file_mgr.h"
#include "acdb_common.h"
#include "acdb_data_proc.h"

#include "ar_osal_error.h"
#include "ar_osal_file_io.h"


ar_fhandle *acdb_pkt_log_fd = NULL;
/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */
#define ACDB_SOFTWARE_VERSION_MAJOR 0x00000001
#define ACDB_SOFTWARE_VERSION_MINOR 0x0000000B
#define ACDB_SOFTWARE_VERSION_REVISION 0x00000000
#define ACDB_SOFTWARE_VERSION_CPLINFO 0x00000000

#define INF 4294967295U

#define ACDB_CLEAR_BUFFER(x) memset(&x, 0, sizeof(x))

/* ---------------------------------------------------------------------------
* Global Data Definitions
*--------------------------------------------------------------------------- */
uint32_t glb_buf_1[GLB_BUF_1_LENGTH];
uint32_t glb_buf_2[GLB_BUF_2_LENGTH];
uint32_t glb_buf_3[GLB_BUF_3_LENGTH];
/* ---------------------------------------------------------------------------
* Static Variable Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Function Declarations and Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */
typedef struct _acdb_audio_cal_context_info_t AcdbAudioCalContextInfo;
#include "acdb_begin_pack.h"
struct _acdb_audio_cal_context_info_t {
    AcdbOp op;
    AcdbOp data_op;
    /**< A flag indicating whether data is being requested for the first
    time (only CKV[new] is passed in) */
    bool_t is_first_time;
    /**< A flag indicating whether the module CKV found is the default key
    vector(no keys) */
    bool_t is_default_module_ckv;
    /**< A flag indicating whether process(true) or skip(false) the
    default CKV */
    bool_t ignore_get_default_data;
    /**< Keeps track of module IIDs that are associated with the
    default CKV */
    AcdbUintList default_data_iid_list;
    /**< The Subgraph to get calibration for */
    uint32_t subgraph_id;
    /**< The instance of the module that contains tag data */
    uint32_t instance_id;
    /**< The parameter(s) to get tag data for */
    AcdbUintList *parameter_list;
    /**< A pointer to the full CKV[new] */
    AcdbGraphKeyVector *ckv;
    /**< A pointer to the Delta CKV containing the difference between
    CKV[new] and CKV[old] */
    AcdbGraphKeyVector *delta_ckv;
    /**< A pointer to the Module CKV */
    AcdbGraphKeyVector *module_ckv;
    /**< The type of parameter to get data for (NONE, SHARED, GLOBAL
    SHARED) */
    AcdbDataPersistanceType param_type;
    /**< The DEF, DOT, and DOT2 data offsets */
    AcdbCkvLutEntryOffsets data_offsets;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_tag_data_context_info_t AcdbTagDataContextInfo;
#include "acdb_begin_pack.h"
struct _acdb_tag_data_context_info_t {
    AcdbOp op;
    AcdbOp data_op;
    /**< The Subgraph that contains tag data */
    uint32_t subgraph_id;
    /**< The instance of the module that contains tag data */
    uint32_t instance_id;
    /**< The parameter(s) to get tag data for */
    AcdbUintList *parameter_list;
    /**< The module tag containing the tag ID and TKV */
    AcdbModuleTag *tag;
    /**< The type of parameter to get data for (NONE, SHARED, GLOBAL
    SHARED) */
    AcdbDataPersistanceType param_type;
    /**< The DEF and DOT offsets */
    AcdbDefDotPair data_offsets;
}
#include "acdb_end_pack.h"
;

typedef struct _acdb_iid_ref_count_t AcdbIidRefCount;
#include "acdb_begin_pack.h"
struct _acdb_iid_ref_count_t
{
    uint32_t module_iid;
    uint32_t ref_count;
}
#include "acdb_end_pack.h"
;

/**< Contains offsets to the key id and value lists that
make up a key vector */
typedef struct _acdb_key_vector_lut_entry_t AcdbKeyVectorLutEntry;
struct _acdb_key_vector_lut_entry_t
{
    /**< Absolute offset of the key table that includes #keys
    and the key id list */
    uint32_t offset_key_tbl;
    /**< Absolute offset of the value combination in the LUT */
    uint32_t offset_value_list;
};

/**< Maintains offsets for key vectors in the Cal Key ID Table
and Cal Key Value LUT chunks */
typedef struct _acdb_key_vector_lut_t AcdbKeyVectorLut;
struct _acdb_key_vector_lut_t
{
    /**< The size of the lookup table in bytes*/
    uint32_t size;
    /**< Number of entries in the lookup table */
    uint32_t num_entries;
    /**< A pointer to the array that stores the lookup table
    entries. The format of an entry is:
    <#keys, Key ID Table Offset, Value LUT Offset */
    AcdbKeyVectorLutEntry* lookup_table;
};

/**< Maintains an unziped key vector; separate key list, separate value list */
typedef struct _acdb_key_value_list_t AcdbKeyValueList;
struct _acdb_key_value_list_t
{
    /**< Number of keys in the key vector */
    uint32_t num_keys;
    /**< list of key ids whose length is num_keys */
    uint32_t *key_list;
    /**< list of key value ids whose length is num_keys */
    uint32_t *value_list;
};


/*< Stores information about an Acdb File table*/
typedef struct _acdb_table_info_t AcdbTableInfo;
struct _acdb_table_info_t
{
    /*< Offset of the table in the ACDB file */
    uint32_t table_offset;
    /*< Size of the entire table */
    uint32_t table_size;
    /*< Size of the table entry */
    uint32_t table_entry_size;
};

/*< Contains infomation used to partitions and search in a sractch buffer*/
typedef struct _acdb_partition_search_info_t AcdbPartitionSearchInfo;
struct _acdb_partition_search_info_t
{
    /*< Number of Keys to use in the search */
    uint32_t num_search_keys;
    /*< Number of elements within the structure */
    uint32_t num_structure_elements;
    /*< Index of the entry within the table
    returned by the algorithm */
    uint32_t entry_index;
    /*< Offset of the entry within the table
    returned by the algorithm (In bytes)*/
    uint32_t entry_offset;
    /*< Buffer used to store and search partitions */
    AcdbBlob scratch_space;
    /*< Pointer to the structure containing the search keys */
    void *table_entry_struct;
};

/*< Contains infomation used to search a table for an item */
typedef struct _acdb_table_search_info_t AcdbTableSearchInfo;
struct _acdb_table_search_info_t
{
    /*< Number of Keys to use in the search */
    uint32_t num_search_keys;
    /*< Number of elements within the structure */
    uint32_t num_structure_elements;
    /*< Index of the entry within the table
    returned by the algorithm */
    uint32_t entry_index;
    /*< Offset of the entry within the table
    returned by the algorithm (In bytes)*/
    uint32_t entry_offset;
    /*< Pointer to the structure containing the search keys */
    void *table_entry_struct;
};

/* ---------------------------------------------------------------------------
* Function Prototypes
*--------------------------------------------------------------------------- */

/**
* \brief
*		Search Subgraph Calibration LUT for offsets to the Key ID and Key
*       Value tables. Returns a list of CKV entries for a subgraph.
*
* \param[in] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in] sz_ckv_entry_list: Size of the CKV entry list
* \param[out] ckv_entry_list: CKV Entries found during the search
* \param[out] num_entries: nNumber of CKV Entries found
* \return 0 on success, and non-zero on failure
*/
int32_t SearchSubgraphCalLut(
    uint32_t req_subgraph_id, uint32_t sz_ckv_entry_list,
    AcdbCalKeyTblEntry *ckv_entry_list, uint32_t *num_entries);

/**
* \brief
*		Search Subgraph Calibration LUT for offsets to the Key ID and Key
*       Value tables. Returns the absolute file offset to the list of CKV entries
*       for a subgraph.
*
* \param[in] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in] sz_ckv_entry_list: Size of the CKV entry list
* \param[out] ckv_entry_list: CKV Entries found during the search
* \param[out] num_entries: nNumber of CKV Entries found
* \return 0 on success, and non-zero on failure
*/
int32_t SearchSubgraphCalLut2(AcdbSgCalLutHeader *sg_lut_entry_header,
    uint32_t *ckv_entry_list_offset);

void AcdbClearAudioCalContextInfo(AcdbAudioCalContextInfo* info);

/**
* \brief
*		Finds the DEF, DOT, and DOT2 data offsets using the Cal Key Table Entry
*       offsets(Key ID Table offset, Value LUT offset) of a subgraph from the
*       Subgraph Calibration LUT chunk.
*       It also updates the module_ckv in the context info struct which is used
*       to locate data on the heap
*
* \param[in] ckv_entry: Contains offsets to the Key ID and Value LUT tables
* \param[in/out] info: Context info containing the module kv to be udpated
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbFindModuleCKV(AcdbCalKeyTblEntry *ckv_entry,
    AcdbAudioCalContextInfo *info);

/**
* \brief
*		Uses the DEF, DOT, and DOT2 data offsets in the context info to find
*       calibration data for different PID types(non-persist/persist)
*
* \param[in/out] info: Context info containing the module kv
* \param[in/out] blob_offset: current response blob offset
* \param[in/out] rsp: Response blob to write caldata to
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbGetSubgraphCalibration(AcdbAudioCalContextInfo *info,
    uint32_t *blob_offset, AcdbBlob *rsp);

/* ---------------------------------------------------------------------------
* Function Definitions
*--------------------------------------------------------------------------- */

/**
* \brief
*       Determines the number of partitions to create based on the table
*       information provided
*
*       Formula for determining partition size:
*
*       Stratch Buffer Size - (Stratch Buffer Size % Table Entry Size)
*
*       E.x
*           buffer_size = 2500 bytes, table_entry_size = 12 bytes
*           2496 = 2500 - (2500 % 12)
*           so (2496/12) = 208 table entries can fit into one partition
*
* \depends Elements withing a table entry structure should be uint32_t
*
* \param[in] table_info: Contains information about an ACDB Data file table
* \param[in] scratch_buf_size: size of the buffer that will be used for
*           storing partitions
* \param[out] num_partitions: the recommended number of partitions based
*           on scratch_buf_size
* \param[in] partition_size: the size of each partition
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t GetPartitionInfo(AcdbTableInfo *table_info, uint32_t scratch_buf_size,
    uint32_t* num_partitions, uint32_t *partition_size)
{
    if (IsNull(table_info) || IsNull(num_partitions) || IsNull(partition_size))
    {
        ACDB_ERR("Either table info, number of partitions, or partition size is null");
        return AR_EBADPARAM;
    }

    if (table_info->table_size > scratch_buf_size)
    {
        //Determine partition size which is the max readable size based on the scratch buffer
        *partition_size = (uint32_t)(
            scratch_buf_size - (scratch_buf_size % table_info->table_entry_size)
            );
        *num_partitions = (uint32_t)AcdbCeil(table_info->table_size, *partition_size);
    }
    else
    {
        *num_partitions = 1;
        *partition_size = table_info->table_size;
    }

    return AR_EOK;
}

/**
* \brief
*       Partitions an acdb file table and performs a binary search on each part.
*
* Example Setup
* //Setup Table Information
* table_info.table_offset = Starting Offset of the Table;
* table_info.table_size = Table Size;
* table_info.table_entry_size = sizeof(Table Entry);
*
* //Setup Search Information
* search_info.num_search_keys = search_index_count;
* search_info.num_structure_elements =
*     (sizeof(Table Entry) / sizeof(uint32_t));
* search_info.table_entry_struct = Entry to Search For;
* search_info.scratch_space.buf = &glb_buf_1;
* search_info.scratch_space.buf_size = sizeof(glb_buf_1);
*
* status = AcdbPartitionBinarySearch(&table_info, &search_info);
*
* \param[in] table_info: Contains information about an ACDB Data file table
* \param[in/out] part_info: Contains infomation used to
*               partitions and search in sractch buffer. The
*               part_info.table_entry_struct is updated if data is found
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t AcdbPartitionBinarySearch(AcdbTableInfo *table_info, AcdbPartitionSearchInfo *part_info)
{
    int32_t status = AR_EOK;
    uint32_t entry_index = 0;
    uint32_t actual_index = 0;
    uint32_t part_offset = 0;
    uint32_t file_offset = 0;
    Partition part = { 0 };
    uint32_t num_parts = 0;
    uint32_t partition_size = 0;

    if (IsNull(table_info) || IsNull(part_info) || table_info->table_size <= 0)
    {
        return AR_EBADPARAM;
    }

    status = GetPartitionInfo(table_info, part_info->scratch_space.buf_size,
        &num_parts, &partition_size);

    part.offset = table_info->table_offset;
    part.size = partition_size;

    for (uint32_t j = 0; j < num_parts; j++)
    {
        file_offset = part.offset;
        status = FileManReadBuffer(part_info->scratch_space.buf,
            part.size, &file_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Unable to read Partition[%d][size: %d bytes, offset: 0x%x]",
                j, part.offset, part.size);
            return AR_EFAILED;
        }

        part.offset += partition_size;

        //Binary Search
        if (SEARCH_ERROR == AcdbDataBinarySearch2(
            part_info->scratch_space.buf,
            part.size,
            part_info->table_entry_struct,
            part_info->num_search_keys,
            part_info->num_structure_elements,
            &entry_index))
        {
            //Nothing found in Partition X so continue
            status = AR_ENOTEXIST;
            continue;
        }
        else
        {
            //Found entry
            part_offset = j * part.size;
            actual_index = part_offset/sizeof(uint32_t) + entry_index;
            actual_index /= part_info->num_structure_elements;
            part_info->entry_index = actual_index;
            part_info->entry_offset = entry_index * sizeof(uint32_t);

            ACDB_MEM_CPY(
                part_info->table_entry_struct,
                ((uint8_t*)part_info->scratch_space.buf) + part_info->entry_offset,
                table_info->table_entry_size);

            status = AR_EOK;
            break;
        }
    }

    return status;
}

/**
* \brief
*       Performs a binary search on an entire ACDB Table. No scratch buffers
*       are used like in AcdbPartitionBinarySearch(..)
*
* Example Setup
* //Setup Table Information
* table_info.table_offset = Starting Offset of the Table;
* table_info.table_size = Table Size;
* table_info.table_entry_size = sizeof(Table Entry);
*
* //Setup Search Information
* search_info.num_search_keys = search_index_count;
* search_info.num_structure_elements =
*     (sizeof(Table Entry) / sizeof(uint32_t));
* search_info.table_entry_struct = Entry to Search For;
*
* status = AcdbTableBinarySearch(&table_info, &search_info);
*
* \param[in] table_info: Contains information about an ACDB Data file table
* \param[in/out] part_info: Contains infomation used to
*               partitions and search in sractch buffer. The
*               part_info.table_entry_struct is updated if data is found
*
* \return AR_EOK on success, non-zero otherwise
* \sa AcdbPartitionBinarySearch
*/
int32_t AcdbTableBinarySearch(
    AcdbTableInfo *table_info, AcdbTableSearchInfo *part_info)
{
    int32_t status = AR_EOK;
    uint32_t entry_index = 0;

    void *table_ptr = NULL;
    if (IsNull(table_info) || IsNull(part_info) || table_info->table_size <= 0)
    {
        return AR_EBADPARAM;
    }

    status = FileManGetFilePointer2(&table_ptr, table_info->table_offset);

    if (SEARCH_ERROR == AcdbDataBinarySearch2(
        table_ptr,
        table_info->table_size,
        part_info->table_entry_struct,
        part_info->num_search_keys,
        part_info->num_structure_elements,
        &entry_index))
    {
        //Nothing found in the table
        return AR_ENOTEXIST;
    }

    part_info->entry_index = entry_index;
    part_info->entry_offset = entry_index * sizeof(uint32_t);

    ACDB_MEM_CPY_SAFE(
        part_info->table_entry_struct,
        table_info->table_entry_size,
        ((uint8_t*)table_ptr) + part_info->entry_offset,
        table_info->table_entry_size);

    return status;
}

/**
* \brief
*       Create partitions for a table and store these partitions in
*       partition_buf (use glb_buf2, the smallest global buffer)
*
*       Buffers:
*       buf - Contains the table that needs to be partitioned
*       partition_buf - Holds the partitions in the format:
*       [p1.offset, p1.size, ..., pn.offset, pn.size]
*
*       Example
*       Size Of a Partition =
*       Global Buffer Size - (Global Buffer Size % Table Entry Size)
*       Ex.
*       glb_buf size = 2500Bytes
*       Cal LUT Entry size = 12Bytes
*       2500 - (2500 % 12) =
*       2496 (208 = (2496/12) Cal LUT Entries can fit into one partition)
*
* \param[in] table_offset: offset of ACDB table
* \param[in] table_size: size of ACDB table
* \param[in] table_entry_size: size of an entry in the ACDB table
* \param[in] buf_size: size of the buffer that stores each partition
* \param[in/out] partition_buf: buffer that stores partition info
* \param[in/out] num_partitions: number of partitions in partition_buf
*
* \return TRUE (create partitions), FALSE (don't create partitions)
*/
bool_t create_partitions(uint32_t table_offset, size_t table_size,
	size_t table_entry_size, size_t buf_size, uint32_t* partition_buf,
	uint32_t* num_partitions)
{
	bool_t should_partition = FALSE;
	uint32_t partition_size = 0;
	uint32_t parition_offset = 0;

	if (table_size > buf_size)
	{
		should_partition = TRUE;
		//Determine Partition Size

        //Max Readable size
		partition_size = (uint32_t)(buf_size - (buf_size % table_entry_size));
        *num_partitions = (uint32_t)AcdbCeil((uint32_t)table_size, partition_size);
		parition_offset = 0;
		for (uint32_t i = 0; i < (*num_partitions) * 2; i++)
		{
			if (i % 2 == 0)
			{
                //Store Size at odd index
				partition_buf[i] = partition_size;
			}
			else
			{
                //Store Offset at even index
				partition_buf[i] = table_offset + parition_offset;
				parition_offset += partition_size;
			}
		}
	}

	return should_partition;
}

/**
* \brief
*       Compares key vector values using memcmp
*
* \param[in] key_vector: The key vector used in the comparision
* \param[in/out] cmp_from_kv_values: The key values from key_vector to
*                compare with
* \param[in/out] cmp_to_kv_values: The key values to compare against
* \param[in/out] num_keys: number of keys in the key vector
*
* \return AR_EOK on success, non-zero otherwise
*/
bool_t CompareKeyVectorValues(AcdbGraphKeyVector key_vector, uint32_t* cmp_from_kv_values,
	uint32_t *cmp_to_kv_values, uint32_t num_keys)
{
	for (uint32_t i = 0; i < num_keys; i++)
		cmp_from_kv_values[i] = key_vector.graph_key_vector[i].value;

	if (0 == ACDB_MEM_CMP(cmp_from_kv_values, cmp_to_kv_values, num_keys * sizeof(uint32_t)))
		return TRUE;

	return FALSE;
}

/**
* \brief
*       Compares key vector ids using memcmp
*
* \param[in] key_vector: The key vector used in the comparision
* \param[in/out] cmp_from_kv_ids: The key ids from key_vector to
*                compare with
* \param[in/out] cmp_to_kv_ids: The key ids to compare against
* \param[in/out] num_keys: number of keys in the key vector
*
* \return AR_EOK on success, non-zero otherwise
*/
bool_t CompareKeyVectorIds(AcdbGraphKeyVector key_vector, uint32_t* cmp_from_kv_ids,
	uint32_t *cmp_to_kv_ids, uint32_t num_keys)
{
	for (uint32_t i = 0; i < num_keys; i++)
		cmp_from_kv_ids[i] = key_vector.graph_key_vector[i].key;

	if (0 == ACDB_MEM_CMP(cmp_from_kv_ids, cmp_to_kv_ids, num_keys * sizeof(uint32_t)))
		return TRUE;

	return FALSE;
}

/**
* \brief CopyFromFileToBuf
*		Copies data from the ACDB Data file at a given offset to the provided buffer
* \param[in] file_offset: location in the file to start reading from
* \param[in/out] dst_buf: the buffer to copy data into
* \param[in] sz_dst_buf: size of the destination buffer
* \param[in] sz_block: the block size used to read from the file
* \param[int] calblob: Module calibration blob passed in from the client
* \return AR_EOK on success, non-zero otherwise
*/
int32_t CopyFromFileToBuf(uint32_t file_offset, void* dst_buf, uint32_t sz_dst_buf, uint32_t sz_block)
{
	if (IsNull(dst_buf) || sz_block == 0 || sz_dst_buf == 0)
		return AR_EBADPARAM;

	int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t foffset = 0;
	//Block Size to use for reading from file

	//Destination size alighned to 'sz_max_buf' bytes
	uint32_t sz_dst_aligned = (sz_dst_buf - (sz_dst_buf % sz_block));

	//Size of the file read initially set to block size
	uint32_t read_size = sz_block;

	//The remaining bytes to read from the file into the destination
	uint32_t sz_remainder_block = (sz_dst_buf % sz_block);

	//Occurs when the block size is greater than the destination buffer size
	if (sz_dst_aligned == 0)
	{
		read_size = sz_remainder_block;
	}

	while (offset < sz_dst_buf)
	{
		if (offset + sz_remainder_block == sz_dst_buf)
		{
			read_size = sz_remainder_block;
		}

        foffset = file_offset + offset;
        status = FileManReadBuffer((uint8_t*)dst_buf + offset, read_size, &foffset);
        if (AR_EOK != status)
        {
            ACDB_ERR("Error[%d]: Failed to read data from file. The read size is %d bytes", read_size);
            return status;
        }

		if ((offset == sz_dst_aligned && sz_remainder_block != 0)||
			(sz_dst_aligned == 0 && sz_remainder_block != 0))
		{
			offset += sz_remainder_block;
			//read_size = sz_remainder_block;
		}
		else
		{
			offset += sz_block;
		}
	}

	return status;
}

/**
* \brief
*		Write data to a buffer ensuring not to write past the length of the buffer
* \param[in] dst_blob: the blob to write to
* \param[in] blob_offset: current offset within dst_blob
* \param[in] src_blob: the blob to write from
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbWriteBuffer(AcdbBlob *dst_buf, uint32_t *blob_offset, AcdbBlob *src_buf)
{
    if (*blob_offset
        + src_buf->buf_size > dst_buf->buf_size)
    {
        return AR_ENEEDMORE;
    }

    ACDB_MEM_CPY((uint8_t*)dst_buf->buf + *blob_offset,
        src_buf->buf, src_buf->buf_size);
    *blob_offset += src_buf->buf_size;

    return AR_EOK;
}

/**
* \brief
*		Search Tag Data Lookup Table for a matching TKV and retrieve the
*       def and dot offsets
*
* \param[int] tkv: tag key vector to search for
* \param[in] tag_data_tbl_offset: offset of tag data lut for a <SG, Tag> pair
* \param[out] offset_pair: Offset of def and dot offsets
*
* \return 0 on success, and non-zero on failure
*/
int32_t TagDataSearchLut(AcdbGraphKeyVector *tkv,
    uint32_t tag_data_tbl_offset, AcdbDefDotPair *offset_pair)
{
    int32_t status = AR_EOK;
	uint32_t i = 0;
	ChunkInfo ci_tag_data_lut = { ACDB_CHUNKID_MODULE_TAGDATA_LUT, 0, 0 };
    KeyTableHeader key_table_header = { 0 };
	size_t sz_tag_key_vector_entry = 0;
	uint32_t offset = 0;
	bool_t result = FALSE;

    ACDB_CLEAR_BUFFER(glb_buf_1);
    ACDB_CLEAR_BUFFER(glb_buf_3);

    status = ACDB_GET_CHUNK_INFO(&ci_tag_data_lut);
    if (AR_FAILED(status))
    {
        return status;
    }

    offset = ci_tag_data_lut.chunk_offset + tag_data_tbl_offset;
    status = FileManReadBuffer(&glb_buf_1, 2 * sizeof(uint32_t), &offset);
    if (status != 0)
    {
        ACDB_ERR("Error[%d]: Unable to read TKV length and "
            "number of TKV value entries", status);
        return status;
    }

    key_table_header.num_keys = glb_buf_1[0];
    key_table_header.num_entries = glb_buf_1[1];

	sz_tag_key_vector_entry =
        key_table_header.num_keys * sizeof(uint32_t)
        + 2 * sizeof(uint32_t);
	//sz_tag_data_lut = num_tag_key_vector_entries * sz_tag_key_vector_entry;

	/* Number of keys for this LUT must match the
     * number of keys in the input */
	if (key_table_header.num_keys != tkv->num_keys)
	{
		ACDB_ERR("Error[%d]: The LUT table key count %d does not "
            "match input TKV key count %d",
            AR_EFAILED, key_table_header.num_keys, tkv->num_keys);
		return AR_EFAILED;
	}

	result = FALSE;
	for (i = 0; i < key_table_header.num_entries; i++)
	{
		ACDB_CLEAR_BUFFER(glb_buf_1);

        status = FileManReadBuffer(
            &glb_buf_1, sz_tag_key_vector_entry, &offset);
        if (status != 0)
        {
            ACDB_ERR("Error[%d]: Unable to read TKV values entry", AR_EFAILED);
            return AR_EFAILED;
        }

		if (TRUE == CompareKeyVectorValues(
            *tkv, glb_buf_3, glb_buf_1, tkv->num_keys))
		{
			offset_pair->offset_def = (glb_buf_1 + key_table_header.num_keys)[0];
            offset_pair->offset_dot = (glb_buf_1 + key_table_header.num_keys)[1];
			result = TRUE;
			break;
		}
	}

	ACDB_CLEAR_BUFFER(glb_buf_1);
	ACDB_CLEAR_BUFFER(glb_buf_3);

	if (FALSE == result)
	{
		ACDB_ERR("Error[%d]: Unable to find matching TKV", AR_ENOTEXIST);
		return AR_ENOTEXIST;
	}

	return status;
}

/**
* \brief
*		Search Tag Data Key Table for a <Subgraph, Tag ID> pair and retrieve the
*       offset of the tag data table
*
* \param[int] subgraph_id: Subgraph to search for
* \param[in] tag_id: Module Tag to search for
* \param[out] offset_tag_data_tbl: Offset of the Tag Data Table
*
* \return 0 on success, and non-zero on failure
*/
int32_t TagDataSearchKeyTable(uint32_t subgraph_id, uint32_t tag_id, uint32_t *offset_tag_data_tbl)
{
    int32_t status = AR_EOK;
    int32_t search_index_count = 2;
    SubgraphTagLutEntry entry = { subgraph_id, tag_id, 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbTableSearchInfo search_info = { 0 };
    ChunkInfo ci_key_table = { ACDB_CHUNKID_MODULE_TAGDATA_KEYTABLE, 0, 0 };

    if (IsNull(offset_tag_data_tbl))
    {
        ACDB_ERR("Error[%d]: "
            "The offset input parameter is null.", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&ci_key_table);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to retrieve Tag Data Key Table chunk.", status);
        return status;
    }

    //Setup Table Information (skip table entry count)
    table_info.table_offset = ci_key_table.chunk_offset + sizeof(uint32_t);
    table_info.table_size = ci_key_table.chunk_size - sizeof(uint32_t);
    table_info.table_entry_size = sizeof(SubgraphTagLutEntry);

    //Setup Search Information
    search_info.num_search_keys = search_index_count;
    search_info.num_structure_elements =
        (sizeof(SubgraphTagLutEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = &entry;

    status = AcdbTableBinarySearch(&table_info, &search_info);

    if(AR_SUCCEEDED(status))
        *offset_tag_data_tbl = entry.offset;

    return status;
}

/**
* \brief
*		Retrieve tag data from the file manager or heap for a list of parameters
*
* \param[int] info: context information for tag data
* \param[out] rsp: Response blob containing calibration in the format of
*                  [<IID, PID, Size, Err Code, Data>, ...]
*
* \return 0 on success, and non-zero on failure
*/
int32_t TagDataGetParameterData(AcdbTagDataContextInfo *info, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t blob_offset = 0;
    uint32_t data_offset = 0;
    uint32_t param_size = 0;
    uint32_t padded_param_size = 0;
    uint32_t param_dot_offset = 0;
    uint32_t entry_index = 0;
    uint32_t num_id_entries = 0;
    AcdbModIIDParamIDPair iid_pid_pair = { 0 };
    AcdbDspModuleHeader module_header = { 0 };
    ChunkInfo ci_def = { 0 };
    ChunkInfo ci_dot = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbBlob buffer = { 0 };
    AcdbOp op = IsNull(rsp->buf) ?
        ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    if (IsNull(info) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: "
            "One or more input parameters are null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_def.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_DEF;
    ci_dot.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_DOT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;

    status = ACDB_GET_CHUNK_INFO(
        &ci_def, &ci_dot, &ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to retrieve def, dot, or data pool tables", status);
        return status;
    }

    offset = ci_def.chunk_offset + info->data_offsets.offset_def;
    status = FileManReadBuffer(&num_id_entries, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Failed to read number of tagged <MID, PID> entries.", status);
        return status;
    }

    status = FileManReadBuffer(&glb_buf_3[0],
        num_id_entries * sizeof(AcdbModIIDParamIDPair), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read List of <MID, PID>.", status);
        return status;
    }

    iid_pid_pair.module_iid = info->instance_id;
    module_header.module_iid = info->instance_id;

    for (uint32_t i = 0; i < info->parameter_list->count; i++)
    {
        iid_pid_pair.parameter_id = info->parameter_list->list[i];
        module_header.parameter_id = info->parameter_list->list[i];

        if (SEARCH_ERROR == AcdbDataBinarySearch2((void*)&glb_buf_3,
            num_id_entries * sizeof(AcdbModIIDParamIDPair),
            &iid_pid_pair, 2,
            (int32_t)(sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)),
            &entry_index))
        {
            status = AR_ENOTEXIST;
            ACDB_DBG("Error[%d]: Unable to find Parameter(0x%x). Skipping..",
                status, module_header.parameter_id);
            continue;
        }

        //Get from heap
        status = GetSubgraphCalData(
            info->param_type, ACDB_DATA_ACDB_CLIENT,
            &info->tag->tag_key_vector,
            info->subgraph_id, module_header.module_iid,
            module_header.parameter_id, &blob_offset, rsp);

        if (AR_SUCCEEDED(status))
        {
            continue;
        }

        //Get from file manager

        offset = ci_dot.chunk_offset
            + info->data_offsets.offset_dot;

        /* Calculation for DOT offset of <MID, PID> pair
        *
        * Offset of  +  Number of  +  Offset of the desired DOT entry based
        *    DOT       DOT entries         on index of <MID,PID> in DEF
        */
        param_dot_offset =
            offset + sizeof(uint32_t) + sizeof(uint32_t) * (uint32_t)(
                entry_index
                / (sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)));

        status = FileManReadBuffer(&data_offset,
            sizeof(uint32_t), &param_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: "
                "Failed to read data pool offset from DOT.", status);
            return status;
        }

        offset = ci_data_pool.chunk_offset + data_offset;

        status = FileManReadBuffer(&param_size,
            sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: "
                "Failed to read param size from data pool.", status);
            return status;
        }

        padded_param_size = ACDB_ALIGN_8_BYTE(param_size);
        module_header.param_size = param_size;

        switch (op)
        {
        case ACDB_OP_GET_SIZE:
            rsp->buf_size += sizeof(AcdbDspModuleHeader)
                + padded_param_size;
            break;
        case ACDB_OP_GET_DATA:

            //Write Module IID, and Parameter ID
            buffer.buf = (void*)&module_header;
            buffer.buf_size = sizeof(AcdbDspModuleHeader);
            status = AcdbWriteBuffer(rsp, &blob_offset, &buffer);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to write module header for"
                    "<sg, mod, param>[0x%x, 0x%x, 0x%x]",
                    status, info->subgraph_id, module_header.module_iid,
                    module_header.parameter_id);
                return status;
            }

            //Write Payload
            if (rsp->buf_size < (blob_offset + module_header.param_size))
                return AR_ENEEDMORE;

            if (module_header.param_size > 0)
            {
                status = FileManReadBuffer(
                    (uint8_t*)rsp->buf + blob_offset,
                    module_header.param_size,
                    &offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: "
                        "Failed to read param(0x%x) data",
                        status, module_header.parameter_id);
                    return status;
                }

                blob_offset += padded_param_size;
            }
            break;
        default:
            break;
        }

    }

    return status;
}

/**
* \brief
*		Retrieve tag data from the file manager or heap for:
*           1. An entire subgraph OR
*           2. An entire module
*
* \param[int] info: context information for tag data
* \param[int] blob_offset: offset within the rsp blob
* \param[out] rsp: Response blob containing calibration in the format of
*                  [<IID, PID, Size, Err Code, Data>, ...]
*
* \return 0 on success, and non-zero on failure
*/
int32_t TagDataGetSubgraphData(AcdbTagDataContextInfo *info,
    uint32_t *blob_offset, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    ChunkInfo ci_def = { ACDB_CHUNKID_MODULE_TAGDATA_DEF, 0, 0 };
    ChunkInfo ci_dot = { ACDB_CHUNKID_MODULE_TAGDATA_DOT, 0, 0 };
    ChunkInfo ci_data_pool = { ACDB_CHUNKID_DATAPOOL, 0, 0 };
    uint32_t num_id_entries = 0;
    uint32_t num_data_offset = 0;
    uint32_t num_param_cal_found = 0;
    uint32_t cur_def_offset = 0;
    uint32_t cur_dot_offset = 0;
    uint32_t cur_dpool_offset = 0;
    uint32_t data_offset = 0;
    uint32_t padded_param_size = 0;
    AcdbDspModuleHeader module_header = { 0 };


    status = ACDB_GET_CHUNK_INFO(&ci_def, &ci_dot, &ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to retrieve def, dot, or data pool tables", status);
        return status;
    }

    cur_def_offset = ci_def.chunk_offset + info->data_offsets.offset_def;
    status = FileManReadBuffer(&num_id_entries, sizeof(uint32_t), &cur_def_offset);
    if (status != 0)
    {
        ACDB_ERR("Error[%d]: Unable to read number of Tag ID entries", AR_EFAILED);
        return AR_EFAILED;
    }

    //Tag Data DOT Table Chunk
    cur_dot_offset = ci_dot.chunk_offset + info->data_offsets.offset_dot;
    status = FileManReadBuffer(&num_data_offset, sizeof(uint32_t), &cur_dot_offset);
    if (status != 0)
    {
        ACDB_ERR("Error[%d]: Unable to read number of Tag ID entries", AR_EFAILED);
        return AR_EFAILED;
    }

    if (num_id_entries != num_data_offset)
    {
        ACDB_ERR("Error[%d]: Tag def entry count (%d) does not match"
            "Tag dot entry count (%d)",
            num_id_entries, num_data_offset, AR_EFAILED);
        return AR_EFAILED;
    }

    for (uint32_t i = 0; i < num_id_entries; i++)
    {
        status = FileManReadBuffer(&module_header, sizeof(AcdbMiidPidPair),
            &cur_def_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read <iid, pid> entry", status);
            return status;
        }

        //If Get Module Data is called
        if((info->data_op == ACDB_OP_GET_MODULE_DATA) &&
            (module_header.module_iid != info->instance_id))
        {
            cur_dot_offset += sizeof(uint32_t);
            continue;
        }

        num_param_cal_found++;

        //Get data from Heap
        status = GetSubgraphCalData(info->param_type, ACDB_DATA_ACDB_CLIENT,
            &info->tag->tag_key_vector, info->subgraph_id, module_header.module_iid,
            module_header.parameter_id, blob_offset, rsp);

        if (AR_SUCCEEDED(status))
        {
            cur_dot_offset += sizeof(uint32_t);
            continue;
        }

        //Get data from file manager
        status = FileManReadBuffer(
            &data_offset, sizeof(uint32_t), &cur_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read number of "
                "<iid, pid> entries", status);
            return status;
        }

        cur_dpool_offset = ci_data_pool.chunk_offset + data_offset;
        status = FileManReadBuffer(
            &module_header.param_size, sizeof(uint32_t), &cur_dpool_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read param size", status);
            return status;
        }

        padded_param_size = ACDB_ALIGN_8_BYTE(module_header.param_size);

        switch (info->op)
        {
        case ACDB_OP_GET_SIZE:
            rsp->buf_size +=
                sizeof(AcdbDspModuleHeader) + padded_param_size;
            break;
        case ACDB_OP_GET_DATA:
            if (rsp->buf_size >= (*blob_offset + sizeof(AcdbDspModuleHeader)))
            {
                ACDB_MEM_CPY((uint8_t*)rsp->buf + *blob_offset,
                    &module_header, sizeof(AcdbDspModuleHeader));
                *blob_offset += sizeof(AcdbDspModuleHeader);
            }
            else
            {
                return AR_ENEEDMORE;
            }

            if (rsp->buf_size >= (*blob_offset + module_header.param_size))
            {
                if (module_header.param_size > 0)
                {
                    status = FileManReadBuffer(
                        (uint8_t*)rsp->buf + *blob_offset,
                        module_header.param_size,
                        &cur_dpool_offset);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Failed to read param data",
                            status);
                        return status;
                    }

                    *blob_offset += module_header.param_size;

                    if (padded_param_size != module_header.param_size)
                    {
                        ar_mem_set((uint8_t*)rsp->buf + *blob_offset, 0,
                            padded_param_size - module_header.param_size);

                        *blob_offset += padded_param_size
                            - module_header.param_size;
                    }
                }
            }
            else
            {
                return AR_ENEEDMORE;
            }

            break;
        default:
            break;
        }
    }

    if (num_param_cal_found == 0) return AR_ENOTEXIST;

    return status;
}

int32_t AcdbCmdGetModuleTagData(AcdbSgIdModuleTag *req, AcdbBlob *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    uint32_t offset_tag_data_tbl = 0;
    uint32_t blob_offset = 0;
    AcdbTagDataContextInfo info = { 0 };

    if (rsp_size < sizeof(AcdbBlob))
    {
        return AR_EBADPARAM;
    }

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: "
            "One or more input parameters are null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->module_tag.tag_key_vector.num_keys == 0)
    {
        ACDB_ERR("Error[%d]: The TKV cannot be empty. If the intent is to "
            "retrieve default data, use the CKV-based 'GET' APIs", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->num_sg_ids == 0 || IsNull(req->sg_ids))
    {
        ACDB_ERR("Error[%d]: The subgraph list cannot be empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = AcdbSort2(
        req->module_tag.tag_key_vector.num_keys * sizeof(AcdbKeyValuePair),
        req->module_tag.tag_key_vector.graph_key_vector,
        sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to sort Tag Key Vector", AR_EBADPARAM);
        return status;
    }

    /* Tag data does not have a param type. Currently tag data is
     * stored under non-persistent data in the heap */
    info.param_type = ACDB_DATA_NON_PERSISTENT;
    info.op = IsNull(rsp->buf) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;
    info.data_op = ACDB_OP_GET_SUBGRAPH_DATA;
    info.tag = &req->module_tag;

    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        info.subgraph_id = req->sg_ids[i];

        //Search Tag Key Table Chunk
        offset_tag_data_tbl = 0;
        status = TagDataSearchKeyTable(info.subgraph_id,
            info.tag->tag_id, &offset_tag_data_tbl);
        if (AR_FAILED(status))
        {

            ACDB_DBG("Error[%d]: No tag data for Tag(0x%x) "
                "found under Subgraph(0x%x). Skipping..",
                status, info.tag->tag_id ,info.subgraph_id);
            status = AR_EOK;
            continue;
        }

        //Search Tag Data LUT
        status = TagDataSearchLut(&info.tag->tag_key_vector,
            offset_tag_data_tbl, &info.data_offsets);
        if (AR_FAILED(status))
        {
            ACDB_DBG("Error[%d]: No matching TKV "
                "found under Subgraph(0x%x) with Tag(0x%x). Skipping..",
                status, info.subgraph_id, info.tag->tag_id);
            status = AR_EOK;
            continue;
        }

        //Get Module Tag Data
        status = TagDataGetSubgraphData(&info, &blob_offset, rsp);
        if (AR_FAILED(status) && status != AR_ENOTEXIST)
        {
            break;
        }
    }

    if (AR_FAILED(status) && status != AR_ENOTEXIST)
        return status;

    else if (rsp->buf_size <= 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: No tag data found", status);
    }

    return status;
}

int32_t AcdbCmdGetTagData(AcdbGetTagDataReq *req, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset_tag_data_tbl = 0;
    uint32_t blob_offset = 0;
    AcdbTagDataContextInfo info = { 0 };

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: "
            "One or more input parameters are null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->module_tag.tag_key_vector.num_keys == 0)
    {
        ACDB_ERR("Error[%d]: The TKV cannot be empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = AcdbSort2(
        req->module_tag.tag_key_vector.num_keys * sizeof(AcdbKeyValuePair),
        req->module_tag.tag_key_vector.graph_key_vector,
        sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to sort Tag Key Vector", AR_EBADPARAM);
        return status;
    }

    /* Tag data does not have a param type. Currently tag data is
    * stored under non-persistent data in the heap */
    info.param_type = ACDB_DATA_NON_PERSISTENT;
    info.op = IsNull(rsp->buf) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;
    info.data_op = ACDB_OP_GET_MODULE_DATA;
    info.tag = &req->module_tag;
    info.subgraph_id = req->subgraph_id;
    info.instance_id = req->module_iid;
    info.parameter_list = &req->parameter_list;

    if (info.op == ACDB_OP_GET_SIZE)
    {
        ACDB_DBG("Getting data for Tag(0x%x) and TKV:",
            info.tag->tag_id);
        LogKeyVector(&info.tag->tag_key_vector, TAG_KEY_VECTOR);
    }

    //Search Tag Key Table Chunk
    offset_tag_data_tbl = 0;
    status = TagDataSearchKeyTable(info.subgraph_id,
        info.tag->tag_id, &offset_tag_data_tbl);
    if (AR_FAILED(status) && status == AR_ENOTEXIST)
    {
        ACDB_ERR("Error[%d]: The <subgraph, tag>[0x0%x, 0x0%x] "
            "entry does not exist", status,
            info.subgraph_id, info.tag->tag_id);
        return status;
    }
    else if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: An error occured while searching "
            "the tag key table for entry <Subgraph, Tag>[0x0%x,0x0%x]",
            status, info.subgraph_id, info.tag->tag_id);
        return status;
    }

    //Search Tag Data LUT
    status = TagDataSearchLut(&info.tag->tag_key_vector,
        offset_tag_data_tbl, &info.data_offsets);
    if (AR_FAILED(status) && status == AR_ENOTEXIST)
    {
        return status;
    }
    else if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: An error occured while searching "
            "the tag data lut", status);
        return status;
    }

    if (req->parameter_list.count == 0 ||
        IsNull(req->parameter_list.list))
    {
        //Get Tag data for an entire module
        status = TagDataGetSubgraphData(&info, &blob_offset, rsp);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            ACDB_ERR("Error[%d]: No data found for "
                "<subgraph, tag, module instance>[0x%x, 0x%x, 0x%x]",
                status, info.subgraph_id, info.tag->tag_id, info.instance_id);
            return status;
        }
        else if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: An error occured while retrieving all tag"
                " data for <subgraph, tag, module instance>[0x%x, 0x%x, 0x%x]",
                status, info.subgraph_id, info.tag->tag_id, info.instance_id);
            return status;
        }
    }
    else
    {
        //Get Tag Data for the specified list of parameters
        status = TagDataGetParameterData(&info, rsp);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            ACDB_ERR("Error[%d]: No data found with the parameter list "
                "for <subgraph, tag, module instance>[0x%x, 0x%x, 0x%x]",
                status, info.subgraph_id, info.tag->tag_id, info.instance_id);
            return status;
        }
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: An error occured while retrieving "
                "data with parameter list for "
                "<subgraph, tag, module instance>[0x%x, 0x%x, 0x%x]",
                status, info.subgraph_id, info.tag->tag_id, info.instance_id);
            return status;
        }
    }

    return status;
}

int32_t AcdbCmdSetTagData(AcdbSetTagDataReq *req)
{
    int32_t status = AR_EOK;
    uint32_t is_delta_data_supported = 0;
    uint32_t subgraph_id = 0;
    uint32_t offset_tag_data_tbl = 0;

    if (IsNull(req))
    {
        ACDB_ERR("Error[%d]: The input request is null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->module_tag.tag_key_vector.num_keys == 0)
    {
        ACDB_ERR("Error[%d]: The TKV cannot be empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->num_sg_ids == 0 || IsNull(req->sg_ids))
    {
        ACDB_ERR("Error[%d]: The subgraph list cannot be empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->blob_size == 0 || IsNull(req->blob))
    {
        ACDB_ERR("Error[%d]: The input blob is empty. There are not parameters to set",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    /* Ensure the Tag + Subgraph combination exists in the database
     * before commiting data to the heap */
    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        subgraph_id = req->sg_ids[i];

        status = TagDataSearchKeyTable(subgraph_id,
            req->module_tag.tag_id, &offset_tag_data_tbl);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            ACDB_ERR("Error[%d]: The <subgraph, tag>[0x0%x, 0x0%x] "
                "entry does not exist.", status,
                subgraph_id, req->module_tag.tag_id);
            return status;
        }
        else if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: An error occured while searching "
                "the tag key table for entry <Subgraph, Tag>[0x0%x,0x0%x]",
                status, subgraph_id, req->module_tag.tag_id);
            return status;
        }
    }

    status = DataProcSetTagData(req);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to set subgraph tag to heap", status);
        return status;
    }

    status = acdbCmdIsPersistenceSupported(&is_delta_data_supported);
    if (AR_SUCCEEDED(status) && is_delta_data_supported)
    {
        status = acdb_delta_data_ioctl(
            ACDB_DELTA_DATA_CMD_SAVE, NULL, 0, NULL);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to save delta file", status);
        }
    }
    else
    {
        ACDB_INFO("Error[%d]: Unable to save delta data. "
            "Is delta data persistance enabled?", status);
    }

    return status;
}

/**
* \brief
*		Retrieve calibration data for a parameter from the data pool
* \param[int] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in/out] rsp: A data blob containg calibration data
* \param[in] offset_pair: calibration data def and dot offsets
* \return 0 on success, and non-zero on failure
*/
int32_t CalDataGetParameterData(AcdbAudioCalContextInfo *info, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t blob_offset = 0;
    uint32_t data_offset = 0;
    uint32_t param_size = 0;
    uint32_t padded_param_size = 0;
    uint32_t param_dot_offset = 0;
    uint32_t entry_index = 0;
    uint32_t num_id_entries = 0;
    uint32_t num_iid_found = 0;
    AcdbModIIDParamIDPair iid_pid_pair = { 0 };
    AcdbDspModuleHeader module_header = { 0 };
    ChunkInfo ci_def = { 0 };
    ChunkInfo ci_dot = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbBlob buffer = { 0 };
    AcdbIidRefCount *iid_ref = NULL;
    AcdbOp op = IsNull(rsp->buf) ?
        ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    if (IsNull(info) || IsNull(rsp))
    {
        ACDB_ERR("The input request, or response are null");
        return AR_EBADPARAM;
    }

    ci_def.chunk_id = ACDB_CHUNKID_CALDATADEF;
    ci_dot.chunk_id = ACDB_CHUNKID_CALDATADOT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;

    status = ACDB_GET_CHUNK_INFO(&ci_def, &ci_dot, &ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: "
            "Unable to retrieve def, dot, or data pool tables", status);
        return status;
    }

    offset = ci_def.chunk_offset + info->data_offsets.offset_def;

    status = FileManReadBuffer(&num_id_entries, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of <MID, PID> entries.", status);
        return status;
    }

    status = FileManReadBuffer(
        &glb_buf_3[0], num_id_entries * sizeof(AcdbModIIDParamIDPair), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read List of <MID, PID>.", status);
        return status;
    }

    iid_pid_pair.module_iid = info->instance_id;
    module_header.module_iid = info->instance_id;

    if (info->is_default_module_ckv && info->ignore_get_default_data)
    {
        //Ensure global buffer can store the default data IID Reference list
        if (GLB_BUF_1_LENGTH < num_id_entries * 2)
        {
            ACDB_ERR("Error[%d]: Need more memory to store IID reference list",
                AR_ENEEDMORE);
            return AR_ENEEDMORE;
        }

        ACDB_CLEAR_BUFFER(glb_buf_1);

        iid_ref = (AcdbIidRefCount*)glb_buf_1;
    }

    for (uint32_t i = 0; i < info->parameter_list->count; i++)
    {
        iid_pid_pair.parameter_id = info->parameter_list->list[i];
        module_header.parameter_id = info->parameter_list->list[i];

        if (SEARCH_ERROR == AcdbDataBinarySearch2((void*)&glb_buf_3,
            num_id_entries * sizeof(AcdbModIIDParamIDPair),
            &iid_pid_pair, 2,
            (int32_t)(sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)),
            &entry_index))
        {
            status = AR_ENOTEXIST;
            ACDB_DBG("Error[%d]: Unable to find Parameter(0x%x). Skipping..",
                status, module_header.parameter_id);
            continue;
        }

        if (info->is_default_module_ckv && info->ignore_get_default_data)
        {
            //Add IIDs to list and ignore get data
            if (iid_ref->module_iid != module_header.module_iid && num_iid_found == 0)
            {
                iid_ref->module_iid = module_header.module_iid;
                iid_ref->ref_count = 1;
                num_iid_found++;
            }
            else if (iid_ref->module_iid != module_header.module_iid)
            {
                iid_ref++;
                iid_ref->module_iid = module_header.module_iid;
                iid_ref->ref_count = 1;
                num_iid_found++;
            }
            else
            {
                /* Increase ref count of the current <IID, Ref Count> Obj */
                iid_ref->ref_count++;
            }

            continue;
        }
        else
        {
            /* For other CKVs, if the module is in the default data list, remove it */
            if (!IsNull(info->default_data_iid_list.list))
            {
                iid_ref = (AcdbIidRefCount*)
                    info->default_data_iid_list.list;
                bool_t found_iid = FALSE;
                for (uint32_t j = 0; j < info->
                    default_data_iid_list.count; j++)
                {
                    if ((module_header.module_iid == iid_ref->module_iid)
                        && iid_ref->ref_count > 0)
                    {
                        iid_ref->ref_count--;
                        found_iid = TRUE;
                        break;
                    }
                    else if ((module_header.module_iid == iid_ref->module_iid)
                        && iid_ref->ref_count <= 0)
                        break;

                    iid_ref++;
                }

                if (info->is_default_module_ckv && !found_iid)
                {
                    continue;
                }
            }
        }

        //Get from heap
        status = GetSubgraphCalData(
            info->param_type, ACDB_DATA_ACDB_CLIENT,
            info->ckv,
            info->subgraph_id, module_header.module_iid,
            module_header.parameter_id, &blob_offset, rsp);

        if (AR_SUCCEEDED(status))
        {
            continue;
        }

        //Get from file manager

        offset = ci_dot.chunk_offset
            + info->data_offsets.offset_dot;

        /* Calculation for DOT offset of <MID, PID> pair
        *
        * Offset of  +  Number of  +  Offset of the desired DOT entry based
        *    DOT       DOT entries         on index of <MID,PID> in DEF
        */
        param_dot_offset =
            offset + sizeof(uint32_t) + sizeof(uint32_t) * (uint32_t)(
                entry_index
                / (sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)));

        status = FileManReadBuffer(&data_offset,
            sizeof(uint32_t), &param_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: "
                "Failed to read data pool offset from DOT.", status);
            return status;
        }

        offset = ci_data_pool.chunk_offset + data_offset;

        status = FileManReadBuffer(&param_size,
            sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: "
                "Failed to read param size from data pool.", status);
            return status;
        }

        padded_param_size = ACDB_ALIGN_8_BYTE(param_size);
        module_header.param_size = param_size;

        switch (op)
        {
        case ACDB_OP_GET_SIZE:
            rsp->buf_size += sizeof(AcdbDspModuleHeader)
                + padded_param_size;
            break;
        case ACDB_OP_GET_DATA:

            //Write Module IID, and Parameter ID
            buffer.buf = (void*)&module_header;
            buffer.buf_size = sizeof(AcdbDspModuleHeader);
            status = AcdbWriteBuffer(rsp, &blob_offset, &buffer);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to write module header for"
                    "<Subgraph(0x%x), Instance(0x%x), Parameter(0x%x)>",
                    status, info->subgraph_id, module_header.module_iid,
                    module_header.parameter_id);
                return status;
            }

            //Write Payload
            if (rsp->buf_size < (blob_offset + module_header.param_size))
                return AR_ENEEDMORE;

            if (module_header.param_size > 0)
            {
                status = FileManReadBuffer(
                    (uint8_t*)rsp->buf + blob_offset,
                    module_header.param_size,
                    &offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: "
                        "Failed to read param(0x%x) data",
                        status, module_header.parameter_id);
                    return status;
                }

                blob_offset += padded_param_size;
            }

            break;
        default:
            break;
        }

    }

    if (info->is_default_module_ckv && !info->ignore_get_default_data)
    {
        ACDB_FREE(info->default_data_iid_list.list);
        info->default_data_iid_list.list = NULL;
    }
    else if (info->is_default_module_ckv && info->ignore_get_default_data)
    {
        /* Copy the IID Ref Count list from GLB_BUF_1 since it will be
        * cleared outside of this call */
        info->default_data_iid_list.count = num_iid_found;

        //List of AcdbIidRefCount
        info->default_data_iid_list.list = (uint32_t*)
            ACDB_MALLOC(AcdbIidRefCount, num_iid_found);

        ACDB_MEM_CPY_SAFE(info->default_data_iid_list.list,
            num_iid_found * sizeof(AcdbIidRefCount),
            &glb_buf_1[0], num_iid_found * sizeof(AcdbIidRefCount));

        iid_ref = NULL;
    }


    return status;
}

int32_t AcdbCmdGetCalData(AcdbGetCalDataReq *req, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t ckv_list_offset = 0;
    uint32_t default_ckv_entry_offset = 0;
    uint32_t blob_offset = 0;
    AcdbSgCalLutHeader sg_cal_lut_header = { 0 };
    AcdbCalKeyTblEntry ckv_entry = { 0 };
    AcdbAudioCalContextInfo info =
    {
        ACDB_OP_NONE,
        ACDB_OP_NONE,
        FALSE, FALSE, FALSE,
        { 0 }, 0, 0,
        NULL, NULL, NULL, NULL,
        ACDB_DATA_UNKNOWN,
        { 0 }
    };

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " req or rsp");
        return AR_EBADPARAM;
    }

    /* CKV Setup
    * 1. GLB_BUF_2 stores the module CKV that will be used to locate data in
    * the heap(if it exists)
    * 2. The Delta CKV and CKV[New] are allocated on the heap by
    * AcdbComputeDeltaCKV
    *
    * tmp - stores the amount of padding after graph_key_vectornum_keys
    * tmp2 - stores the position of the KeyValuePair list
    *
    * The first info.module_ckv->graph_key_vector assignment has the effect
    * of storing the address of the pointer in glb_buf_2 after
    * graph_key_vector.num_keys
    *
    * The second info.module_ckv->graph_key_vector assignment has the effect
    * of pointer the info.module_ckv->graph_key_vector to the actual list
    * of key value pairs
    *
    * Whats inside global buffer 2 after these operations?
    *
    * The first 'sizeof(AcdbGraphKeyVector)' bytes is AcdbGraphKeyVector
    * followed by 'sizeof(AcdbKeyValuePair) * num_keys' bytes which is
    * the KV list
    */
    info.module_ckv = (AcdbGraphKeyVector*)&glb_buf_2[0];
    uint32_t tmp = (sizeof(AcdbGraphKeyVector) - sizeof(AcdbKeyValuePair *)) / sizeof(uint32_t);
    uint32_t tmp2 = sizeof(AcdbGraphKeyVector) / sizeof(uint32_t);
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp];
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp2];

    info.ckv = &req->cal_key_vector;

    /* This flag must be set to true since delta CKVs are
    * not applicable here */
    info.is_first_time = TRUE;
    info.op = IsNull(rsp->buf) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;
    info.data_op = ACDB_OP_GET_MODULE_DATA;
    info.subgraph_id = req->subgraph_id;
    info.instance_id = req->module_iid;
    info.parameter_list = &req->parameter_list;

    sg_cal_lut_header.subgraph_id = req->subgraph_id;
    status = SearchSubgraphCalLut2(&sg_cal_lut_header, &ckv_list_offset);
    if (AR_FAILED(status))
    {
        return status;
    }

    if (sg_cal_lut_header.num_ckv_entries == 0)
    {
        ACDB_ERR("Error[%d]: Subgraph(0x%x) has no CKV entries in"
            " the Subgraph Cal LUT. At least one entry should be"
            " present.",
            AR_ENOTEXIST, req->subgraph_id);
        return AR_ENOTEXIST;
    }

    info.ignore_get_default_data =
        sg_cal_lut_header.num_ckv_entries == 1 ? FALSE : TRUE;

    //The default CKV will be skipped and handeled last
    if (sg_cal_lut_header.num_ckv_entries > 1 && info.is_first_time)
        sg_cal_lut_header.num_ckv_entries++;

    default_ckv_entry_offset = ckv_list_offset;

    for (uint32_t k = 0; k < sg_cal_lut_header.num_ckv_entries; k++)
    {
        /* The Default CKV is always the first CKV in the list of CKVs to
        * search. It will be skipped and processed last */
        if ((k == sg_cal_lut_header.num_ckv_entries - 1) &&
            info.ignore_get_default_data == TRUE && info.is_first_time)
        {
            info.ignore_get_default_data = FALSE;

            status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                &default_ckv_entry_offset);
        }
        else
        {
            status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                &ckv_list_offset);
        }

        if (AR_FAILED(status))
        {
            info.ckv = NULL;
            AcdbClearAudioCalContextInfo(&info);
            return status;
        }

        status = AcdbFindModuleCKV(&ckv_entry, &info);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            continue;
        }
        else if (AR_FAILED(status))
        {
            AcdbClearAudioCalContextInfo(&info);
            return status;
        }

        if (req->parameter_list.count == 0 ||
            IsNull(req->parameter_list.list))
        {
            //Get data for an entire module
            status = AcdbGetSubgraphCalibration(&info, &blob_offset, rsp);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                //No calibration found for subgraph with module ckv_entry
                //has_cal_data = FALSE;
                continue;
            }
            else if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: An error occured while getting cal "
                    "data for <subgraph, module instance>[0x%x, 0x%x]",
                    status, info.subgraph_id, info.instance_id);

                info.ckv = NULL;
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }
        }
        else
        {
            //Get Data for a list of parameters
            status = CalDataGetParameterData(&info, rsp);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                //No calibration found for subgraph with module ckv_entry
                //has_cal_data = FALSE;
                continue;
            }
            else if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: An error occured while getting cal "
                    "data for <subgraph, module instance>[0x%x, 0x%x]",
                    status, info.subgraph_id, info.instance_id);

                info.ckv = NULL;
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }
        }
    }

    if (rsp->buf_size == 0)
        status = AR_ENOTEXIST;

    if (AR_FAILED(status) && status == AR_ENOTEXIST)
    {
        ACDB_ERR("Error[%d]: No data found for "
            "<subgraph, module instance>[0x%x, 0x%x].", status,
            info.subgraph_id, info.instance_id);
    }
    else if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: An error occured while getting cal data for "
            "<subgraph, module instance>[0x%x, 0x%x].", status,
            info.subgraph_id, info.instance_id);
    }

    //Clean Up Context Info
    info.ckv = NULL;
    AcdbClearAudioCalContextInfo(&info);

    return status;
}

/**
* \brief
*       Search for a <Subgraph ID, Tag ID> pair and return the definition
*       table offset in the lut_entry parameter
*
* \param[in/out] ci_lut: chunk information for ACDB_CHUNKID_TAGGED_MODULE_LUT.
* \param[in/out] lut_entry: Contains the <Subgraph ID, Tag ID, Def Offset>.
*                           The Def Offset will be populated by the search
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t SearchTaggedModuleMapLut(ChunkInfo *ci_lut, SubgraphTagLutEntry *lut_entry)
{
    int32_t status = AR_EOK;
    //uint32_t offset = 0;

    //Lookup Table
    //Binary Search based on <Subgraph ID, Tag ID>
    uint32_t search_index_count = 2;
    uint32_t lut_size = 0;
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    if (IsNull(ci_lut) || IsNull(lut_entry))
    {
        ACDB_ERR("Error[%d]: Invalid input parameter",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    lut_size = ci_lut->chunk_size - sizeof(uint32_t);

    //Setup Table Information
    table_info.table_offset = ci_lut->chunk_offset + sizeof(uint32_t);
    table_info.table_size = lut_size;
    table_info.table_entry_size = sizeof(SubgraphTagLutEntry);

    //Setup Search Information
    search_info.num_search_keys = search_index_count;
    search_info.num_structure_elements =
        (sizeof(SubgraphTagLutEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = lut_entry;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        //ACDB_DBG("Error[%d]: Failed to find Subgraph ID(0x%x) Tag ID(0x%x)",
        //    status, lut_entry->sg_id, lut_entry->tag_id);
        return status;
    }

    ACDB_CLEAR_BUFFER(glb_buf_1);

    return status;
}

/**
* \brief
*       Builds the tagged module list response. The size of the list is
*       accumulated on the first call. The data is filled once the
*       response buffer is allocated.
*
* \param[in/out] ci_def: chunk information for ACDB_CHUNKID_TAGGED_MODULE_DEF.
* \param[in/out] def_offset_list: list of def offsets definition tables that
*                                 contain <ModuleID, InstanceID>'s
* \param[in/out] rsp: the response structure to populate
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t BuildTaggedModulePayload(ChunkInfo *ci_def, AcdbUintList *def_offset_list, AcdbGetTaggedModulesRsp *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_modules = 0;
    uint32_t total_num_module_inst = 0;
    uint32_t sz_module_instances = 0;
    uint32_t expected_size = 0;
    uint32_t actual_size = 0;
    uint32_t mid_list_offset = 0;
    AcdbOp op = IsNull(rsp->tagged_mid_list) ?
        ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    if (IsNull(ci_def) || IsNull(def_offset_list) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: Invalid input parameter",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    expected_size = rsp->num_tagged_mids * sizeof(AcdbModuleInstance);
    rsp->num_tagged_mids = 0;

    for (uint32_t i = 0; i < def_offset_list->count; i++)
    {
        offset = ci_def->chunk_offset;
        offset += def_offset_list->list[i];
        status = FileManReadBuffer(&num_modules, sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read number of module instances "
                "from definition table", status);
            return status;
        }

        if (num_modules == 0) continue;

        if (op == ACDB_OP_GET_SIZE)
        {
            total_num_module_inst += num_modules;
        }
        else
        {
            total_num_module_inst += num_modules;
            sz_module_instances = num_modules * sizeof(AcdbModuleInstance);
            actual_size += sz_module_instances;

            if (actual_size > expected_size)
            {
                ACDB_ERR("Error[%d]: Buffer not large enough to store "
                    "response. Expected Size: %d bytes. Actual Size: %d bytes",
                    AR_ENEEDMORE, expected_size, actual_size);
                return AR_ENEEDMORE;
            }

            status = FileManReadBuffer(
                (uint8_t*)rsp->tagged_mid_list + mid_list_offset,
                sz_module_instances, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read number of module instances"
                    " from definition table", status);
                return status;
            }

            mid_list_offset += sz_module_instances;
        }
    }

    rsp->num_tagged_mids = total_num_module_inst;

    if (rsp->num_tagged_mids == 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: No data found", status);
    }

    return status;
}

int32_t AcdbCmdGetTaggedModules(AcdbGetTaggedModulesReq* req, AcdbGetTaggedModulesRsp* rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    ChunkInfo ci_lut = { ACDB_CHUNKID_TAGGED_MODULE_LUT, 0, 0 };
    ChunkInfo ci_def = { ACDB_CHUNKID_TAGGED_MODULE_DEF, 0, 0 };
    SubgraphTagLutEntry lut_entry = { 0 };

    uint32_t def_offset_list_length = GLB_BUF_2_LENGTH;
    AcdbUintList def_offset_list = { 0 };

    if (req == NULL || rsp_size < sizeof(AcdbGetTaggedModulesRsp))
    {
        ACDB_ERR("Error[%d]: Invalid input parameter(s)", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (req->num_sg_ids == 0 || IsNull(req->sg_ids))
    {
        ACDB_ERR("Error[%d]: The subgraph list cannot be empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&ci_lut, &ci_def);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Tagged Module "
            "lookup/definition tables.", status);
        return status;
    }

    def_offset_list.list = &glb_buf_2[0];

    lut_entry.tag_id = req->tag_id;

    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        lut_entry.sg_id = req->sg_ids[i];

        status = SearchTaggedModuleMapLut(&ci_lut, &lut_entry);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            //ACDB_DBG("Error[%d]: Subgraph(0x%x) does not contain Tag(0x%x). "
            //    "Skipping..", status, lut_entry.tag_id, lut_entry.sg_id);
            continue;
        }
        else if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Error occured while looking for "
                "Tag(0x%x) under Subgraph(0x%x)", status,
                lut_entry.tag_id, lut_entry.sg_id);
            return status;
        }

        //Add to def offset list
        if (def_offset_list.count > def_offset_list_length)
        {
            /* This will only occur if there are 1000 subgraphs passed in to
             * this API that contain the tag that we are looking for. glb_buf_2
             * can store up to 1000 def offsets */
            ACDB_ERR("Error[%d]: Not enough space to store "
                "additional def offset", status);
            return AR_ENOMEMORY;
        }

        def_offset_list.list[def_offset_list.count] = lut_entry.offset;
        def_offset_list.count++;
    }

    if (def_offset_list.count == 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: Tag(0x%x) not found in subgraphs provided.",
            status, lut_entry.tag_id);
        return status;
    }

    //Build Instance ID List
    status = BuildTaggedModulePayload(&ci_def, &def_offset_list, rsp);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to build tagged module list response.", status);
    }

    return status;
}

/**
* \brief
*       Search for the first occurance of a <Module ID, Key Table Offset,
*       Value Table Offset> tuple using the Module ID as a search key and
*       returns the file offset of the first occurrance.
*
* \param[in/out] cal_lut_entry: GSL Calibration Lookuptable entry in the
*           format of <MID, Key Table Off. Value Table Off.>
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t DriverDataFindFirstOfModuleID(GslCalLutEntry *cal_lut_entry, uint32_t* file_offset)
{
    int32_t status = AR_EOK;
    //uint32_t offset = 0;
    ChunkInfo gsl_cal_lut = { ACDB_CHUNKID_GSL_CAL_LUT, 0, 0 };

    //Lookup Table
    //Binary Search based on MID
    uint32_t search_index_count = 1;
    uint32_t sz_gsl_cal_lut = 0;
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    if (IsNull(cal_lut_entry) || IsNull(file_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameters is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = AcdbGetChunkInfo(gsl_cal_lut.chunk_id,
        &gsl_cal_lut.chunk_offset, &gsl_cal_lut.chunk_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get GSL Calibration Data Chunk", status);
        return status;
    }

    sz_gsl_cal_lut = gsl_cal_lut.chunk_size - sizeof(uint32_t);

    //Setup Table Information
    table_info.table_offset = gsl_cal_lut.chunk_offset + sizeof(uint32_t);
    table_info.table_size = sz_gsl_cal_lut;
    table_info.table_entry_size = sizeof(GslCalLutEntry);

    //Setup Search Information
    search_info.num_search_keys = search_index_count;
    search_info.num_structure_elements =
        (sizeof(GslCalLutEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = cal_lut_entry;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to find Module ID(0x%x)", cal_lut_entry->mid);
        return status;
    }

    *file_offset = gsl_cal_lut.chunk_offset
        + sizeof(uint32_t) //Number of <Module, key table, value table> entries
        + search_info.entry_offset;

    ACDB_CLEAR_BUFFER(glb_buf_1);

    return status;
}

/**
* \brief
*       Search for a <Module ID, Key Table Offset, Value Table Offset>
*       tuple using the Module ID and Key Table Offset as search keys
*
* \param[in/out] cal_lut_entry: GSL Calibration Lookuptable entry in the
*           format of <MID, Key Table Off. Value Table Off.>
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t DriverDataSearchForModuleID(GslCalLutEntry *cal_lut_entry)
{
	int32_t status = AR_EOK;
	//uint32_t offset = 0;
	ChunkInfo gsl_cal_lut = { ACDB_CHUNKID_GSL_CAL_LUT, 0, 0 };

	//Lookup Table
    //Binary Search based on MID and Key Table Offset
	uint32_t search_index_count = 2;
    uint32_t sz_gsl_cal_lut = 0;
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

	status = AcdbGetChunkInfo(gsl_cal_lut.chunk_id,
        &gsl_cal_lut.chunk_offset, &gsl_cal_lut.chunk_size);
	if (AR_FAILED(status))
	{
		ACDB_ERR("Error[%d]: Failed to get GSL Calibration Data Chunk", status);
		return status;
	}

    sz_gsl_cal_lut = gsl_cal_lut.chunk_size - sizeof(uint32_t);

    //Setup Table Information
    table_info.table_offset = gsl_cal_lut.chunk_offset + sizeof(uint32_t);
    table_info.table_size = sz_gsl_cal_lut;
    table_info.table_entry_size = sizeof(GslCalLutEntry);

    //Setup Search Information
    search_info.num_search_keys = search_index_count;
    search_info.num_structure_elements =
        (sizeof(GslCalLutEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = cal_lut_entry;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to find Module ID(0x%x)", cal_lut_entry->mid);
        return status;
    }

	ACDB_CLEAR_BUFFER(glb_buf_1);

	return status;
}

int32_t DriverDataSearchForKeys(AcdbDriverData *req, uint32_t *offset_calkey_table)
{
	int32_t status = AR_EOK;
	uint32_t offset = 0;
    uint32_t num_keys = 0;
	ChunkInfo gsl_cal_key_tbl = { ACDB_CHUNKID_GSL_CALKEY_TBL, 0, 0 };

    status = ACDB_GET_CHUNK_INFO(&gsl_cal_key_tbl);
	if (AR_FAILED(status))
	{
		ACDB_ERR("Error[%d]: Unable to get GSL Key Table Chunk", status);
		return status;
	}

    offset = gsl_cal_key_tbl.chunk_offset;

    do
    {
        *offset_calkey_table = offset;
        status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read number of keys", status);
            return status;
        }

        if (num_keys != req->key_vector.num_keys)
        {
            offset += num_keys * sizeof(uint32_t);
            continue;
        }

        if (num_keys > 0)
        {
            status = FileManReadBuffer(
                &glb_buf_1, num_keys * sizeof(uint32_t), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read key vector", status);
                return status;
            }

            if (TRUE == CompareKeyVectorIds(
                req->key_vector, glb_buf_3, glb_buf_1, num_keys))
            {
                *offset_calkey_table -= gsl_cal_key_tbl.chunk_offset;
                return status;
            }
        }
        else
        {
            *offset_calkey_table -= gsl_cal_key_tbl.chunk_offset;
            return status;
        }

    } while (offset <
        gsl_cal_key_tbl.chunk_offset + gsl_cal_key_tbl.chunk_size);

    status = AR_ENOTEXIST;
    ACDB_ERR("Error[%d]: Unable to find matching key vector. "
        "One or more key IDs do not exist.", status);

	ACDB_CLEAR_BUFFER(glb_buf_1);
	ACDB_CLEAR_BUFFER(glb_buf_3);

	return status;
}

int32_t DriverDataSearchCalDataLUT(AcdbDriverData *req, AcdbBlob* rsp,
    uint32_t offset_caldata_table, AcdbDefDotPair *offset_pair)
{
	int32_t status = AR_EOK;
	uint32_t offset = 0;
    uint32_t num_struct_elements = 0;
    uint32_t entry_size = 0;
    uint32_t table_size = 0;
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };
    KeyTableHeader caldata_lut_header = { 0 };
	ChunkInfo gsl_cal_data_tbl = { ACDB_CHUNKID_GSL_CALDATA_TBL, 0, 0 };

    if (IsNull(req) || IsNull(rsp) || IsNull(offset_pair))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&gsl_cal_data_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get GSL "
            "Calibration Data Chunk", status);
        return status;
    }

    offset = gsl_cal_data_tbl.chunk_offset + offset_caldata_table;
    status = FileManReadBuffer(
        &caldata_lut_header, sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to seek/read GSL "
            "Calibration Data LUT header", status);
        return status;
    }

    //Plus 2 for def and dot offsets
    num_struct_elements = caldata_lut_header.num_keys + 2;
    entry_size = sizeof(uint32_t) * num_struct_elements;
    table_size = caldata_lut_header.num_entries * entry_size;

    //glb_buf_3 stores key vector values to search for
    for (uint32_t i = 0; i < req->key_vector.num_keys; i++)
    {
        glb_buf_3[i] = req->key_vector.graph_key_vector[i].value;
    }

    //Setup Table Information
    table_info.table_offset = offset;
    table_info.table_size = table_size;
    table_info.table_entry_size = entry_size;

    //Setup Search Information
    search_info.num_search_keys = caldata_lut_header.num_keys;
    search_info.num_structure_elements = num_struct_elements;
    search_info.table_entry_struct = &glb_buf_3;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to find matching key vector. "
            "One or more key values do not match", status);
        return status;
    }

	ACDB_MEM_CPY(offset_pair,
        &glb_buf_3[0] + (caldata_lut_header.num_keys) * sizeof(uint32_t),
		sizeof(AcdbDefDotPair));

	ACDB_CLEAR_BUFFER(glb_buf_1);
	ACDB_CLEAR_BUFFER(glb_buf_3);
	return status;
}

int32_t DriverDataGetCalibration(AcdbDriverData *req, AcdbBlob* rsp, AcdbDefDotPair *offset_pair)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_pids = 0;
    uint32_t num_caldata_offsets = 0;
    uint32_t param_size = 0;
    uint32_t *pid_list = NULL;
    uint32_t *caldata_offset_list = NULL;
    uint32_t param_offset = 0;
    uint32_t pid = 0;

    ChunkInfo ci_def = { ACDB_CHUNKID_GSL_DEF, 0, 0 };
    ChunkInfo ci_dot = { ACDB_CHUNKID_GSL_DOT, 0, 0 };
    ChunkInfo ci_data_pool = { ACDB_CHUNKID_DATAPOOL, 0, 0 };

    if (IsNull(req) || IsNull(rsp) || IsNull(offset_pair))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&ci_def, &ci_dot, &ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get chunk info for either "
            "DEF, DOT, or Data Pool", status);
        return status;
    }

    //Def
    offset = ci_def.chunk_offset + offset_pair->offset_def;
    status = FileManReadBuffer(&num_pids, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of Parameter IDs", status);
        return status;
    }

    status = FileManReadBuffer(&glb_buf_2, sizeof(uint32_t)* num_pids, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Parameter List", status);
        return status;
    }

    //Dot
    offset = ci_dot.chunk_offset + offset_pair->offset_dot;
    status = FileManReadBuffer(&num_caldata_offsets, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of parameter calibration offset", status);
        return status;
    }

    status = FileManReadBuffer(&glb_buf_3, sizeof(uint32_t)* num_caldata_offsets, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read parameter calibration offset list", status);
        return status;
    }

    if (num_pids != num_caldata_offsets)
    {
        ACDB_ERR("Number of parameter IDs in DEF to not match number off offsets in DOT");
        return AR_EFAILED;
    }

    //glb_buf_2 stores pid list
    //glb_buf_3 stores dot data pool offsets
    pid_list = &glb_buf_2[0];
    caldata_offset_list = &glb_buf_3[0];

    if (rsp->buf != NULL)
    {
		if (rsp->buf_size >= sizeof(uint32_t))
		{
			ACDB_MEM_CPY_SAFE(rsp->buf, sizeof(uint32_t), &req->module_id, sizeof(uint32_t));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        param_offset += sizeof(uint32_t);
    }
    else
    {
        rsp->buf_size += sizeof(req->module_id);
    }

    for (uint32_t i = 0; i < num_pids; i++)
    {
        pid = pid_list[i];
        offset = ci_data_pool.chunk_offset + caldata_offset_list[i];

        status = FileManReadBuffer(&param_size, sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read parameter size", status);
            return status;
        }

        if (rsp->buf != NULL)
        {
			if (rsp->buf_size >= (param_offset + (2 * sizeof(uint32_t))))
			{
				ACDB_MEM_CPY_SAFE(rsp->buf + param_offset, sizeof(uint32_t),
                &pid, sizeof(uint32_t));
            param_offset += sizeof(uint32_t);

				ACDB_MEM_CPY_SAFE(rsp->buf + param_offset, sizeof(uint32_t),
                &param_size, sizeof(uint32_t));
            param_offset += sizeof(uint32_t);
			}
			else
			{
				return AR_ENEEDMORE;
			}

            if (param_size != 0)
            {
				if ((rsp->buf_size - param_offset) >= param_size)
				{
                status = FileManReadBuffer(
                    (uint8_t*)rsp->buf + param_offset,
                    param_size, &offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Failed to read "
                        "parameter payload", status);
                    return status;
                }
				}
				else
				{
					return AR_ENEEDMORE;
				}

                param_offset += param_size;
            }
        }
        else
        {
            //<PID, ParamSize, Payload[ParamSize]>
            rsp->buf_size += 2 * sizeof(uint32_t) + param_size;
        }
    }

    pid_list = NULL;
    caldata_offset_list = NULL;
    ACDB_CLEAR_BUFFER(glb_buf_1);
    ACDB_CLEAR_BUFFER(glb_buf_2);
    ACDB_CLEAR_BUFFER(glb_buf_3);

    return status;
}

int32_t AcdbCmdGetDriverData(AcdbDriverData *req, AcdbBlob* rsp, uint32_t rsp_size)
{
	int32_t status = AR_EOK;
	GslCalLutEntry cal_lut_entry = { 0 };
    AcdbDefDotPair offset_pair = { 0 };

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input "
            "parameters are null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

	if (rsp_size < sizeof(AcdbBlob))
	{
        ACDB_ERR("Error[%d]: The response structure size is smaller"
            "than the size of AcdbBlob ", AR_EBADPARAM);
        return AR_EBADPARAM;
	}

    if (req->key_vector.num_keys > 0)
    {
        status = AcdbSort2(
            req->key_vector.num_keys * sizeof(AcdbKeyValuePair),
            req->key_vector.graph_key_vector,
            sizeof(AcdbKeyValuePair), 0);

        if (AR_FAILED(status))
        {
            return status;
        }
    }

    /* The modules in the ACDB_CHUNKID_GSL_CAL_LUT can be associated with
     * more than one key vector. So performing a binary search on just the
     * module ID will not necessarily yield the correct result. In the
     * example below item 1 and 2 are possible candidates.
     *
     * Ex
     * <Module ID, Key Table Offset, LUT Offset>
     * List of GSL LUT Entries:
     * [0x801 KT1 LUT1 | 0x801 KT2 LUT2 | 0x802 KT3 LUT4 | ...]
     *                 [ 0x801 x   x    ] <== search key, x = dont care
     * To alleviate this we calculate the Key Table offset and use it in
     * the binary search so that the search key is unique.
     */
	cal_lut_entry.mid = req->module_id;
	cal_lut_entry.offset_caldata_table = 0;
	cal_lut_entry.offset_calkey_table = 0;

    /* A module will only have one entry in the Key Table and Lookup
     * Table if the key count is 0
     */
    //if (req->key_vector.num_keys > 0)
    //{
        status = DriverDataSearchForKeys(req,
            &cal_lut_entry.offset_calkey_table);
        if (AR_FAILED(status)) return status;
    //}

	status = DriverDataSearchForModuleID(&cal_lut_entry);
    if (AR_FAILED(status)) return status;

	status = DriverDataSearchCalDataLUT(req, rsp,
        cal_lut_entry.offset_caldata_table, &offset_pair);
	if (AR_FAILED(status)) return status;


    status = DriverDataGetCalibration(req, rsp, &offset_pair);
    if (AR_FAILED(status)) return status;

	return status;
}

/**
* \brief
*		Search the GKV Key Table for a matching GKV and get back the offset
*       to the key value table
*
* \depends The input gkv must be sorted
*
* \param[in] gkv: The GKV to search for
* \param[out] gkv_lut_offset: The offset of the input GKV's value lookup table
*
* \return 0 on success, and non-zero on failure
*/
int32_t SearchGkvKeyTable(AcdbGraphKeyVector *gkv, uint32_t *gkv_lut_offset)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_key_tables = 0;
    uint32_t key_table_size = 0;
    uint32_t key_table_entry_size = 0;
    bool_t found = FALSE;
    ChunkInfo ci_key_id_table = { 0 };
    KeyTableHeader key_table_header = { 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbTableSearchInfo search_info = { 0 };

    if (IsNull(gkv) || IsNull(gkv_lut_offset))
    {
        ACDB_ERR("Error[%d]: The gkv input parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_key_id_table.chunk_id = ACDB_CHUNKID_GKVKEYTBL;
    status = ACDB_GET_CHUNK_INFO(&ci_key_id_table);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Graph Key ID Chunk.",
            status);
        return status;
    }

    offset = ci_key_id_table.chunk_offset;
    status = FileManReadBuffer(&num_key_tables, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of GKV Key ID tables.",
            status);
        return status;
    }

    if (num_key_tables > ci_key_id_table.chunk_size)
    {
        ACDB_ERR("Error[%d]: Number of GKV tables %d is greater than the table size %d.",
            AR_EFAILED, num_key_tables, ci_key_id_table.chunk_size);
        return AR_EFAILED;
    }

    for (uint32_t i = 0; i < num_key_tables; i++)
    {
        status = FileManReadBuffer(&key_table_header,
            sizeof(KeyTableHeader), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read key table header.",
                status);
            return status;
        }

        key_table_entry_size = key_table_header.num_keys
            * sizeof(uint32_t)
            + sizeof(uint32_t);
        key_table_size = key_table_header.num_entries
            * key_table_entry_size;

        if (key_table_header.num_keys != gkv->num_keys)
        {
            offset += key_table_size;
        }
        else
        {
            //GLB BUF 2 stores Key IDs
            for (uint32_t j = 0; j < gkv->num_keys; j++)
            {
                glb_buf_2[j] = gkv->graph_key_vector[j].key;
            }

            //Setup Table Information
            table_info.table_offset = offset;
            table_info.table_size = key_table_size;
            table_info.table_entry_size = key_table_entry_size;

            //Setup Search Information
            search_info.num_search_keys = key_table_header.num_keys;
            search_info.num_structure_elements =
                key_table_header.num_keys + 1;//key_ids + GKVLUT Offset
            search_info.table_entry_struct = &glb_buf_2;

            status = AcdbTableBinarySearch(&table_info, &search_info);

            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to find graph key vector. "
                    "No matching key IDs were found", status);
                return status;
            }

            *gkv_lut_offset = glb_buf_2[gkv->num_keys];
            found = TRUE;
            return status;
        }
    }

    if (!found)
    {
        ACDB_ERR("Error[%d]: Unable to find graph key vector. "
            "There are no key vectors with %d keys", AR_ENOTEXIST,
            gkv->num_keys);
        return AR_ENOTEXIST;
    }

    return status;
}

/**
* \brief
*		Search the GKV Lookup Table for a matching value combination and get back
*       the offset to the Subgrah List and Subgraph Property Data List
*
* \depends The input gkv must be sorted
*
* \param[in] gkv: The GKV to search for
* \param[in] gkv_lut_offset: The lookup table offset to start at
* \param[out] sg_list_offset: Offset to the subgraph list
* \param[out] sg_data_offset: Offset to the subgph properdy data list
*
* \return 0 on success, and non-zero on failure
*/
int32_t SearchGkvLut(AcdbGraphKeyVector *gkv, uint32_t gkv_lut_offset,
    uint32_t *sg_list_offset, uint32_t *sg_data_offset)

{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t key_table_size = 0;
    uint32_t key_table_entry_size = 0;
    ChunkInfo ci_key_value_table = { 0 };
    KeyTableHeader key_table_header = { 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    if (IsNull(gkv) || IsNull(sg_list_offset) || IsNull(sg_data_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameter(s) are null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_key_value_table.chunk_id = ACDB_CHUNKID_GKVLUTTBL;
    status = ACDB_GET_CHUNK_INFO(&ci_key_value_table);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Graph Key Lookup Chunk.",
            status);
        return status;
    }

    offset = ci_key_value_table.chunk_offset + gkv_lut_offset;
    status = FileManReadBuffer(&key_table_header,
        sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read key table header.",
            status);
        return status;
    }

    //Value List + SG List offset + SG Data Offset
    key_table_entry_size = key_table_header.num_keys
        * sizeof(uint32_t)
        + 2 * sizeof(uint32_t);
    key_table_size = key_table_header.num_entries
        * key_table_entry_size;

    if (key_table_header.num_keys != gkv->num_keys)
    {
        ACDB_ERR("Error[%d]: The lookup table key count does not "
            "match the input key vector key count.",
            AR_ENOTEXIST);
        return AR_ENOTEXIST;
    }
    else
    {
        //GLB BUF 2 stores Key Values
        for (uint32_t j = 0; j < gkv->num_keys; j++)
        {
            glb_buf_2[j] = gkv->graph_key_vector[j].value;
        }

        //Setup Table Information
        table_info.table_offset = offset;
        table_info.table_size = key_table_size;
        table_info.table_entry_size = key_table_entry_size;

        //Setup Search Information
        search_info.num_search_keys = key_table_header.num_keys;
        search_info.num_structure_elements =
            key_table_header.num_keys + 2;//Values + List Offset + Data Offset
        search_info.table_entry_struct = &glb_buf_2;
        search_info.scratch_space.buf = &glb_buf_1;
        search_info.scratch_space.buf_size = sizeof(glb_buf_1);

        status = AcdbPartitionBinarySearch(&table_info, &search_info);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to find graph key vector. "
                "No matching values were found", status);
            return status;
        }

        *sg_list_offset = glb_buf_2[gkv->num_keys];
        *sg_data_offset = glb_buf_2[gkv->num_keys + 1];
    }

    return status;
}

int32_t BuildGetGraphResponse(AcdbGetGraphRsp *rsp, uint32_t sg_list_offset)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
	uint32_t expected_size = rsp->size;
    ChunkInfo ci_data_pool = { 0 };
    SgListHeader sg_list_header = { 0 };

    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The rsp input parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    status = ACDB_GET_CHUNK_INFO(&ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Data Pool chunk info.",
            status);
        return status;
    }

    offset = ci_data_pool.chunk_offset + sg_list_offset;
    status = FileManReadBuffer(&sg_list_header, sizeof(SgListHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read the subgraph list header", status);
        return status;
    }

    rsp->num_subgraphs = sg_list_header.num_subgraphs;
    rsp->size = sg_list_header.size - sizeof(sg_list_header.num_subgraphs);

    if (IsNull(rsp->subgraphs))
    {
        return AR_EOK;
    }

	if (expected_size >= rsp->size)
	{
    status = FileManReadBuffer(rsp->subgraphs, rsp->size, &offset);
	}
	else
	{
		return AR_ENEEDMORE;
	}
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read the subgraph list", status);
        return status;
    }

    return status;
}

int32_t AcdbCmdGetGraph(AcdbGraphKeyVector *gkv, AcdbGetGraphRsp *rsp,
    uint32_t rsp_struct_size)
{
    int32_t status = AR_EOK;
    uint32_t gkv_lut_offset = 0;
    uint32_t sg_list_offset = 0;
    uint32_t sg_data_offset = 0;
    AcdbGraphKeyVector graph_kv = { 0 };

    if (IsNull(gkv) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " GKV or Response", status);
        return AR_EBADPARAM;
    }

    if (rsp_struct_size < sizeof(AcdbGetGraphRsp))
    {
        ACDB_ERR("Error[%d]: The size of the response structure is less "
            "than %d bytes", status, sizeof(AcdbGetGraphRsp));
        return AR_EBADPARAM;
    }

    if (IsNull(rsp->subgraphs))
    {
        //Log GKV when this API is called for the first time
        ACDB_INFO("Retrieving subgraph list for the following graph key vector:");
        LogKeyVector(gkv, GRAPH_KEY_VECTOR);
    }

    if (gkv->num_keys == 0)
    {
        ACDB_DBG("Error[%d]: Detected empty usecase. "
            "No data will be returned. Skipping..", AR_EOK);
        return AR_EOK;
    }
    //GLB BUF 1 is used for the GKV search
    //GLB BUF 2 is used for storing the found GKV entry
    //GLB BUF 3 is used to store the sorted GKV

    graph_kv.num_keys = gkv->num_keys;
    graph_kv.graph_key_vector = (AcdbKeyValuePair*)&glb_buf_3[0];
    ACDB_MEM_CPY_SAFE(&glb_buf_3[0], gkv->num_keys * sizeof(AcdbKeyValuePair),
        gkv->graph_key_vector, gkv->num_keys * sizeof(AcdbKeyValuePair));

    //Sort Key Vector by Key IDs(position 0 of AcdbKeyValuePair)
    status = AcdbSort2(gkv->num_keys * sizeof(AcdbKeyValuePair),
        graph_kv.graph_key_vector, sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort graph key vector.", status);
        return status;
    }

    status = SearchGkvKeyTable(&graph_kv, &gkv_lut_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to find the graph key vector", status);
        return status;
    }

    status = SearchGkvLut(&graph_kv, gkv_lut_offset,
        &sg_list_offset, &sg_data_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to find the graph key vector", status);
        return status;
    }

    status = BuildGetGraphResponse(rsp, sg_list_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to build response", status);
    }

    return status;
}

int32_t BuildSpfPropertyBlob(uint32_t prop_index, size_t spf_blob_offset,
	uint32_t *prop_data_offset,
	AcdbGetSubgraphDataRsp *pOutput)
{
	/*
	Order of Spf Property Data Blob:
	------------------------------------
	Property Index | Spf Property
	------------------------------------
	0 | Subgraph Config

	1 | Container Config

	2 | Module List

	3 | Module Properties

	4 | Module Connections

	5 | Data Links
	*/

	int32_t status = AR_EOK;
	uint32_t errcode = 0;

	/* Subgraph Spf Property Data Header:
	 * <MIID, PID, ParamSize> (does not include error code)*/
	uint32_t sz_sg_spf_prop_header = 3 * sizeof(uint32_t);
	uint32_t sz_spf_param = 0;
	uint32_t sz_padded_spf_param = 0;
	uint32_t padding = 0;
	uint32_t idx = 0;//GLB BUF index

	if (pOutput->spf_blob.buf != NULL)
	{
		//Copy Header
		if (pOutput->spf_blob.buf_size >= (spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)) + sz_sg_spf_prop_header))
		{
		ACDB_MEM_CPY(pOutput->spf_blob.buf + spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)), &glb_buf_1 + *prop_data_offset, sz_sg_spf_prop_header);
		}
		else
		{
			return AR_ENEEDMORE;
		}

		*prop_data_offset += sz_sg_spf_prop_header - sizeof(sz_spf_param);

		//Copy Payload Size
		idx = (*prop_data_offset) / sizeof(uint32_t);
		sz_spf_param = glb_buf_1[idx];

		*prop_data_offset += sizeof(sz_spf_param);

		//Add paddig if necessary before sending the response back
		if ((sz_spf_param % 8) != 0)
		{
			sz_padded_spf_param = sz_spf_param + (8 - ((sz_spf_param % 8)));

			padding = (8 - ((sz_spf_param % 8)));
		}
		else
		{
			sz_padded_spf_param = sz_spf_param;
		}

		//Insert Error Code
		if (pOutput->spf_blob.buf_size >= (spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)) + sizeof(errcode)))
		{
		ACDB_MEM_CPY(pOutput->spf_blob.buf + spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)), &errcode, sizeof(errcode));
		}
		else
		{
			return AR_ENEEDMORE;
		}

		spf_blob_offset += sizeof(errcode);

		//Copy Payload
		idx = (*prop_data_offset) / sizeof(uint32_t);
		if (pOutput->spf_blob.buf_size >= (spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)) + sz_spf_param))
		{
		ACDB_MEM_CPY(pOutput->spf_blob.buf + spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)), &glb_buf_1[idx], sz_spf_param);
		}
		else
		{
			return AR_ENEEDMORE;
		}

		*prop_data_offset += sz_spf_param;

		//Add Padding
		if ((sz_spf_param % 8) != 0)
		{
			memset((uint8_t*)pOutput->spf_blob.buf + spf_blob_offset + *prop_data_offset + (prop_index * sizeof(errcode)), 0, padding);
			*prop_data_offset += padding;
		}

	}
	else
	{
		//Add <IID, ParamID>
		*prop_data_offset += sz_sg_spf_prop_header - sizeof(uint32_t);

		//Copy Payload Size
		idx = (*prop_data_offset) / sizeof(uint32_t);
		sz_spf_param = glb_buf_1[idx];

		//Add paddig if necessary before sending the response back
		if ((sz_spf_param % 8) != 0)
		{
			sz_padded_spf_param = sz_spf_param + (8 - ((sz_spf_param % 8)));

			//*padded_bytes += (8 - ((sz_spf_param % 8)));
		}
		else
		{
			sz_padded_spf_param = sz_spf_param;
		}

		//Add Param Size
		*prop_data_offset += sizeof(sz_padded_spf_param);

		*prop_data_offset += sz_padded_spf_param;
	}

	return status;
}

int32_t BuildDriverPropertyBlob(uint32_t sz_driver_prop_data, uint32_t *driver_data_buf, AcdbGetSubgraphDataRsp *pOutput, uint32_t *rsp_offset)
{
	if (IsNull(driver_data_buf) || IsNull(rsp_offset))
	{
		return AR_EBADPARAM;
	}

	int32_t status = AR_EOK;
	uint32_t sg_driver_prop_offset = 0;

	//Subgraph Driver Property Data Header: <SubgraphID, #SubgraphProperties>
	size_t sz_sg_driver_prop_header = 2 * sizeof(uint32_t);

	//<SubgraphPropertyID, PropertySize>
	size_t sz_prop_header = 2 * sizeof(uint32_t);

	uint32_t sg_id = driver_data_buf[0];
	uint32_t num_sg_prop = driver_data_buf[1];
	uint32_t sz_prop = 0;
	uint32_t sz_all_prop_payload = 0;
	uint32_t tmp_sz_sg_driver_prop_data = 0;
	uint32_t index = 0;

	if (pOutput->driver_prop.sub_graph_prop_data != NULL)
	{
		//SG ID
		if (pOutput->driver_prop.size >= (*rsp_offset + sizeof(uint32_t)))
		{
		ACDB_MEM_CPY(pOutput->driver_prop.sub_graph_prop_data + *rsp_offset, &sg_id, sizeof(uint32_t));
		}
		else
		{
			return AR_ENEEDMORE;
		}

		sg_driver_prop_offset += sizeof(sg_id);
		*rsp_offset += sizeof(sg_id);

		//Number of SG Properties
		if (pOutput->driver_prop.size >= (*rsp_offset + sizeof(uint32_t)))
		{
		ACDB_MEM_CPY(pOutput->driver_prop.sub_graph_prop_data + *rsp_offset, &num_sg_prop, sizeof(uint32_t));
		}
		else
		{
			return AR_ENEEDMORE;
		}

		sg_driver_prop_offset += sizeof(num_sg_prop);
		*rsp_offset += sizeof(num_sg_prop);

		for (uint32_t i = 0; i < num_sg_prop; i++)
		{
			//SG Property ID
			index = (uint32_t)(sg_driver_prop_offset / sizeof(uint32_t));
			if (pOutput->driver_prop.size >= (*rsp_offset + sizeof(uint32_t)))
			{
			ACDB_MEM_CPY(pOutput->driver_prop.sub_graph_prop_data + *rsp_offset, &driver_data_buf[index], sizeof(uint32_t));
			}
			else
			{
				return AR_ENEEDMORE;
			}

			sg_driver_prop_offset += sizeof(uint32_t);
			*rsp_offset += sizeof(uint32_t);

			//Property Size
			index = (uint32_t)(sg_driver_prop_offset / sizeof(uint32_t));
			if (pOutput->driver_prop.size >= (*rsp_offset + sizeof(uint32_t)))
			{
			ACDB_MEM_CPY(pOutput->driver_prop.sub_graph_prop_data + *rsp_offset, &driver_data_buf[index], sizeof(uint32_t));
			}
			else
			{
				return AR_ENEEDMORE;
			}
			index = (uint32_t)(sg_driver_prop_offset / sizeof(uint32_t));
			sz_prop = driver_data_buf[index];
			sz_all_prop_payload += sz_prop;

			sg_driver_prop_offset += sizeof(uint32_t);
			*rsp_offset += sizeof(uint32_t);

			//Property Paylaod
			index = (uint32_t)(sg_driver_prop_offset / sizeof(uint32_t));
			if (pOutput->driver_prop.size >= (*rsp_offset + sz_prop))
			{
			ACDB_MEM_CPY(pOutput->driver_prop.sub_graph_prop_data + *rsp_offset, &driver_data_buf[index], sz_prop);
			}
			else
			{
				return AR_ENEEDMORE;
			}
			sg_driver_prop_offset += sz_prop;
			*rsp_offset += sz_prop;
		}
	}
	else
	{
		sg_driver_prop_offset += (uint32_t)(sz_sg_driver_prop_header);
		*rsp_offset += (uint32_t)(sz_sg_driver_prop_header);

		for (uint32_t i = 0; i < num_sg_prop; i++)
		{
			//SG Property ID
			sg_driver_prop_offset += sizeof(uint32_t);
			*rsp_offset += sizeof(uint32_t);

			//Property Size
			index = (uint32_t)(sg_driver_prop_offset / sizeof(uint32_t));
			sz_prop = driver_data_buf[index];

			sg_driver_prop_offset += sizeof(uint32_t);
			*rsp_offset += sizeof(uint32_t);

			//Skip the actual payload
			sg_driver_prop_offset += sz_prop;
			*rsp_offset += sz_prop;

			//Property Paylaod
			sz_all_prop_payload += sz_prop;
		}
	}

	//Accumulate size of subgraph property data
	tmp_sz_sg_driver_prop_data =
		(uint32_t)sz_sg_driver_prop_header + num_sg_prop *((uint32_t)sz_prop_header)
		+sz_all_prop_payload;

	if (sz_driver_prop_data != tmp_sz_sg_driver_prop_data)
	{
		status = AR_EFAILED;
		ACDB_ERR_MSG_1("Driver property data size does not match accumulated data size", status);
	}

	return status;
}

int32_t BuildSubgraphDataRsp(AcdbSgIdGraphKeyVector *pInput, uint32_t sg_data_offset, AcdbGetSubgraphDataRsp *pOutput)
{
	int32_t status = AR_EOK;
	uint32_t i = 0;
	uint32_t offset = 0;

	//Data offset for spf portion of the output Blob
	uint32_t spf_blob_offset = 0;
	uint32_t driver_prop_data_offset = 0;
    ChunkInfo ci_data_pool = { ACDB_CHUNKID_DATAPOOL, 0, 0 };

	//Subgraph Property Data Header: <TotalPropertyDataSize, #Subgraphs>
	size_t sz_sg_prop_data_header = 2 * sizeof(uint32_t);

	//Subgraph Property Data Header: <SubgraphID, DataSize>
	size_t sz_sg_obj_header = 2 * sizeof(uint32_t);

	//Accumulated sum of driver property data size for all subgraphs
	size_t sz_all_driver_prop_data = 0;

	//Size of driver property data for a subgraphs
	size_t sz_sg_driver_prop_data = 0;

	//Size of spf property data for a subgraph
	size_t sz_sg_spf_prop_data = 0;

	//Size of the Spf Data portion of the output blob
	size_t sz_spf_blob = 0;
	bool_t found = FALSE;

	ACDB_CLEAR_BUFFER(glb_buf_1);
	ACDB_CLEAR_BUFFER(glb_buf_3);

    status = ACDB_GET_CHUNK_INFO(&ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get data pool chunk", status);

        return status;
    }

	//status = AcdbGetChunkInfo(ACDB_CHUNKID_DATAPOOL, &chkBuf, &chkLen);

	//if (AR_EOK != status) return status;

	//Sort Subgraph IDs
	//for (i = 0; i < pInput->num_sgid; i++)
	//{
	//	glb_buf_3[i] = pInput->sg_ids[i];
	//}

	//AcdbSort(glb_buf_3, pInput->num_sgid);

	for (i = 0; i < pInput->num_sgid; i++)
	{
		found = FALSE;
        offset = ci_data_pool.chunk_offset + sg_data_offset + (uint32_t)sz_sg_prop_data_header;
		do
		{
            status = FileManReadBuffer(&glb_buf_1, sz_sg_obj_header, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read subgraph ID from subgraph property data", status);

                return status;
            }

			if (0 == ACDB_MEM_CMP(&glb_buf_1[0], &pInput->sg_ids[i], sizeof(uint32_t)))
			{
				found = TRUE;
				break;
			}
			else
			{
                offset += glb_buf_1[1]; //+ (uint32_t)sz_sg_prop_data_header - sz_sg_obj_header;
			}

		} while (offset < ci_data_pool.chunk_offset + ci_data_pool.chunk_size && !found);

		if (!found)
		{
			ACDB_ERR("Error[%d]: subgraph %d does not exist", pInput->sg_ids[i]);

			return AR_EFAILED;
		}
		else
		{
			//copy driver prop and spf prop data
			//offset += (uint32_t)sz_sg_obj_header;

			ACDB_CLEAR_BUFFER(glb_buf_1);

            status = FileManReadBuffer(&sz_sg_driver_prop_data, sizeof(uint32_t), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read subgraph driver property size", status);

                return status;
            }

			sz_all_driver_prop_data += sz_sg_driver_prop_data;

            status = FileManReadBuffer(&glb_buf_1, sz_sg_driver_prop_data, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read subgraph driver properties", status);

                return status;
            }

			//Build Driver Property Data Blob
			status = BuildDriverPropertyBlob((uint32_t)sz_sg_driver_prop_data, &glb_buf_1[0], pOutput, &driver_prop_data_offset);
			if (AR_EOK != status)
			{
				ACDB_ERR("AcdbCmdGetSubgraphData() failed, Unable to parse driver property data");
				return status;
			}

			ACDB_CLEAR_BUFFER(glb_buf_1);

            status = FileManReadBuffer(&sz_sg_spf_prop_data, sizeof(uint32_t), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read subgraph SPF property size", status);

                return status;
            }

            status = FileManReadBuffer(&glb_buf_1, sz_sg_spf_prop_data, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read subgraph SPF properties", status);

                return status;
            }

			uint32_t prop_data_offset = 0;

			int property_index = 0;
			while (prop_data_offset < sz_sg_spf_prop_data)
			{
				status = BuildSpfPropertyBlob(property_index, spf_blob_offset, &prop_data_offset, pOutput);

				if (AR_EOK != status)
				{
					ACDB_ERR("Error[%d]: Unable to build spf blob. Encountered error while "
                        "getting property at index %d", status, property_index);

					return status;
				}

				property_index++;
			}

			//Add sizeof(error code) for each spf property
			sz_spf_blob += (sz_sg_spf_prop_data + (property_index * sizeof(uint32_t)));

			spf_blob_offset = (uint32_t)sz_spf_blob;
		}
	}

	pOutput->driver_prop.num_sgid = pInput->num_sgid;

	pOutput->driver_prop.size = (uint32_t)sz_all_driver_prop_data;

	pOutput->spf_blob.buf_size = (uint32_t)sz_spf_blob;

	return status;
}

int32_t AcdbCmdGetSubgraphData(AcdbSgIdGraphKeyVector *pInput, AcdbGetSubgraphDataRsp *pOutput, uint32_t rsp_struct_size)
{
	int32_t status = AR_EOK;

	uint32_t gkvLutOff = 0;

	uint32_t sgListOffset = 0;

	uint32_t sgDataOffset = 0;
    AcdbGraphKeyVector graph_kv = { 0 };
	if (rsp_struct_size < sizeof(AcdbGetSubgraphDataRsp))
	{
		ACDB_ERR("AcdbCmdGetSubgraphData() failed, rsp_struct_size is invalid");
		return AR_EBADPARAM;
	}

    graph_kv.num_keys = pInput->graph_key_vector.num_keys;
    graph_kv.graph_key_vector = (AcdbKeyValuePair*)&glb_buf_3[0];
    ACDB_MEM_CPY_SAFE(&glb_buf_3[0],
        graph_kv.num_keys * sizeof(AcdbKeyValuePair),
        pInput->graph_key_vector.graph_key_vector,
        graph_kv.num_keys * sizeof(AcdbKeyValuePair));

    //Sort Key Vector by Key IDs(position 0 of AcdbKeyValuePair)
    status = AcdbSort2(graph_kv.num_keys * sizeof(AcdbKeyValuePair),
        graph_kv.graph_key_vector, sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort graph key vector.", status);
        return status;
    }

	status = SearchGkvKeyTable(&graph_kv, &gkvLutOff);

	if (AR_EOK != status) return status;

	status = SearchGkvLut(&graph_kv, gkvLutOff, &sgListOffset, &sgDataOffset);

	if (AR_EOK != status) return status;

	status = BuildSubgraphDataRsp(pInput, sgDataOffset, pOutput);

	if (AR_EOK != status) return status;

	return status;
}

int32_t AcdbCmdGetSubgraphConn(AcdbSubGraphList *pInput, AcdbBlob *pOutput, uint32_t rsp_struct_size)
{
	int32_t result = AR_EOK;
	uint32_t sgConnDefOffset = 0;
	uint32_t sgConnDotOffset = 0;
	uint32_t datapoolOffset = 0;
	uint32_t mid = 0;
	uint32_t pid = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t offset = 0;
	uint32_t data_offset = 0;
	uint32_t input_offset = 0;
	uint32_t errcode = 0;
	uint32_t payload_size = 0;
	uint32_t new_payload_size = 0;
	size_t totalsize = 0;
	bool_t found = FALSE;

    ChunkInfo ci_lut = { ACDB_CHUNKID_SGCONNLUT, 0,0 };
    ChunkInfo ci_def = { ACDB_CHUNKID_SGCONNDEF, 0,0 };
    ChunkInfo ci_dot = { ACDB_CHUNKID_SGCONNDOT, 0,0 };
    ChunkInfo ci_data_pool = { ACDB_CHUNKID_DATAPOOL, 0, 0 };

	if (rsp_struct_size < 8)
	{
		ACDB_ERR("AcdbCmdGetSubgraphConn() failed, rsp_struct_size is invalid");
		return AR_EBADPARAM;
	}

    result = ACDB_GET_CHUNK_INFO(&ci_lut, &ci_def, &ci_dot, &ci_data_pool);
    if (AR_FAILED(result))
    {
        ACDB_ERR("Error[%d]: Failed to read one or more chunks", result);
        return result;
    }

	for (i = 0;i < pInput->num_subgraphs;i++)
	{
		uint32_t src_ID = 0, num_dst_sgid = 0, dest_ID = 0;
		ACDB_MEM_CPY(&src_ID, pInput->subgraphs + input_offset, sizeof(src_ID));

		input_offset += sizeof(src_ID);
		ACDB_MEM_CPY(&num_dst_sgid, pInput->subgraphs + input_offset, sizeof(num_dst_sgid));

		input_offset += sizeof(num_dst_sgid);

		for (j = 0; j < num_dst_sgid; j++)
		{
			offset = ci_lut.chunk_offset + sizeof(uint32_t); //no of sgConnLut entries, Ignore this for ACDB SW
			ACDB_MEM_CPY(&dest_ID, pInput->subgraphs + input_offset, sizeof(dest_ID));

			input_offset += sizeof(dest_ID);
			do
			{
				found = FALSE;
				memset(&glb_buf_1, 0, sizeof(glb_buf_1));

                result = FileManReadBuffer(&glb_buf_1, 2 * sizeof(uint32_t), &offset);
                if (AR_FAILED(result))
                {
                    ACDB_ERR("Error[%d]: Unable to read subgraph ID", result);
                    return result;
                }

				if (memcmp(&src_ID, &glb_buf_1[0], sizeof(src_ID)) == 0 && (memcmp(&dest_ID, &glb_buf_1[1], sizeof(dest_ID))) == 0)
				{
					found = TRUE;
					memset(&glb_buf_1, 0, sizeof(glb_buf_1));

                    result = FileManReadBuffer(&glb_buf_1, 2 * sizeof(uint32_t), &offset);
                    if (AR_FAILED(result))
                    {
                        ACDB_ERR("Error[%d]: Unable to read subgraph def and dot offsets", result);
                        return result;
                    }

					sgConnDefOffset = glb_buf_1[0];
					sgConnDotOffset = glb_buf_1[1];

					break;
				}
				else
				{
                    offset -= 2 * sizeof(uint32_t);
					offset = offset + (4 * sizeof(uint32_t));
				}
			} while (offset < ci_lut.chunk_offset + ci_lut.chunk_size && !found);

			if (!found)
			{
				ACDB_ERR("AcdbCmdGetSubgraphConn() failed, subgraph connection does not exist");
				return AR_EFAILED;
			}

			//obtaining SubGraphConnDef payload
			uint32_t num_def_entries = 0;
			uint32_t num_dot_entries = 0;
			uint32_t def_offset = ci_def.chunk_offset + sgConnDefOffset;
			uint32_t dot_offset = ci_dot.chunk_offset + sgConnDotOffset;

            result = FileManReadBuffer(&num_def_entries, sizeof(uint32_t), &def_offset);
            if (AR_FAILED(result))
            {
                ACDB_ERR("Error[%d]: Unable to read def entry count", result);
                return result;
            }

            result = FileManReadBuffer(&num_dot_entries, sizeof(uint32_t), &dot_offset);
            if (AR_FAILED(result))
            {
                ACDB_ERR("Error[%d]: Unable to read dot entry count", result);
                return result;
            }

			//Num Entries in Def and Num Entries in Dot should be equal
			if (num_def_entries != num_dot_entries)
			{
				ACDB_ERR("Error[%d]: Number of entries between Def and Dot tables do not match.", AR_EFAILED);
				return AR_EFAILED;
			}

			for (uint32_t k = 0; k < num_def_entries; k++)
			{
				//Read MID,PID
                result = FileManReadBuffer(&mid, sizeof(uint32_t), &def_offset);
                if (AR_FAILED(result))
                {
                    ACDB_ERR("Error[%d]: Unable to read APM Module ID", result);
                    return result;
                }

                result = FileManReadBuffer(&pid, sizeof(uint32_t), &def_offset);
                if (AR_FAILED(result))
                {
                    ACDB_ERR("Error[%d]: Unable to read APM Parameter ID", result);
                    return result;
                }

				totalsize += sizeof(mid) + sizeof(pid);

				//Read ParamSize, Payload
                result = FileManReadBuffer(&datapoolOffset, sizeof(uint32_t), &dot_offset);
                if (AR_FAILED(result))
                {
                    ACDB_ERR("Error[%d]: Unable to read APM Parameter ID", result);
                    return result;
                }

                //obtaining datapool payload
                uint32_t dpo = ci_data_pool.chunk_offset + datapoolOffset;
                result = FileManReadBuffer(&payload_size, sizeof(uint32_t), &dpo);
                if (AR_FAILED(result))
                {
                    ACDB_ERR("Error[%d]: Unable to read size of parameter 0x%x", result, pid);
                    return result;
                }

				//Add paddig if necessary before sending the response back
				new_payload_size = ACDB_ALIGN_8_BYTE(payload_size);

				totalsize +=
					sizeof(errcode) +
					sizeof(new_payload_size) +
					new_payload_size;

				if (pOutput->buf != NULL)
				{
					if (pOutput->buf_size >= (data_offset + (4 * sizeof(uint32_t))))
					{
					ACDB_MEM_CPY(pOutput->buf + data_offset, &mid, sizeof(mid));
					data_offset += sizeof(mid);
					ACDB_MEM_CPY(pOutput->buf + data_offset, &pid, sizeof(pid));
					data_offset += sizeof(pid);
					ACDB_MEM_CPY(pOutput->buf + data_offset, &payload_size, sizeof(payload_size));
					data_offset += sizeof(payload_size);
					ACDB_MEM_CPY(pOutput->buf + data_offset, &errcode, sizeof(errcode));
					data_offset += sizeof(errcode);
					}
					else
					{
						return AR_ENEEDMORE;
					}

					memset(&glb_buf_1, 0, sizeof(glb_buf_1));

                    result = FileManReadBuffer(&glb_buf_1, payload_size, &dpo);
                    if (AR_FAILED(result))
                    {
                        ACDB_ERR("Error[%d]: Unable to read size of parameter 0x%x", result, pid);
                        return result;
                    }

					if (pOutput->buf_size >= (data_offset + payload_size))
					{
					ACDB_MEM_CPY(pOutput->buf + data_offset, &glb_buf_1, payload_size);
					}
					else
					{
						return AR_ENEEDMORE;
					}
					data_offset += payload_size;

					//Add Padding
					if ((payload_size % 8) != 0)
					{
						memset((uint8_t*)pOutput->buf + data_offset, 0, new_payload_size - payload_size);
						data_offset += (new_payload_size - payload_size);
					}
				}
			}
		}
	}

	pOutput->buf_size = (uint32_t)totalsize;

	return result;
}

int32_t AcdbGetSubgraphCalibration(AcdbAudioCalContextInfo *info,
    uint32_t *blob_offset, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t cur_def_offset = 0;
    uint32_t cur_dot_offset = 0;
    uint32_t cur_dpool_offset = 0;
    uint32_t data_offset = 0;
    uint32_t padded_param_size = 0;
    uint32_t num_param_cal_found = 0;
    uint32_t num_iid_found = 0;
    ChunkInfo ci_cal_def = { 0 };
    ChunkInfo ci_cal_dot = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    uint32_t num_id_entries = 0;
    AcdbDspModuleHeader module_header = { 0 };
    AcdbIidRefCount *iid_ref = NULL;

    ci_cal_def.chunk_id = ACDB_CHUNKID_CALDATADEF;
    ci_cal_dot.chunk_id = ACDB_CHUNKID_CALDATADOT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    status = ACDB_GET_CHUNK_INFO(&ci_cal_def,
        &ci_cal_dot, &ci_data_pool);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration "
            "DEF, DOT, or Datapool chunk information", status);
        return status;
    }

    // Skip number of DOT offsets since number of IID Pairs is the same thing
    cur_dot_offset = ci_cal_dot.chunk_offset + info->data_offsets.offset_dot
        + sizeof(uint32_t);
    cur_def_offset = ci_cal_def.chunk_offset + info->data_offsets.offset_def;
    status = FileManReadBuffer(&num_id_entries, sizeof(uint32_t), &cur_def_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of "
            "<iid, pid> entries", status);
        return status;
    }

    if (info->is_default_module_ckv && info->ignore_get_default_data)
    {
        //Ensure global buffer can store the default data IID Reference list
        if (GLB_BUF_1_LENGTH < num_id_entries * 2)
        {
            ACDB_ERR("Error[%d]: Need more memory to store IID reference list",
                AR_ENEEDMORE);
            return AR_ENEEDMORE;
        }

        ACDB_CLEAR_BUFFER(glb_buf_1);

        iid_ref = (AcdbIidRefCount*)glb_buf_1;
    }

    for (uint32_t i = 0; i < num_id_entries; i++)
    {
        status = FileManReadBuffer(&module_header, sizeof(AcdbMiidPidPair),
            &cur_def_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read <iid, pid> entry", status);
            return status;
        }

        //If Get Module Data is called
        if ((info->data_op == ACDB_OP_GET_MODULE_DATA) &&
            (module_header.module_iid != info->instance_id))
        {
            cur_dot_offset += sizeof(uint32_t);
            continue;
        }

        if (info->is_default_module_ckv && info->ignore_get_default_data)
        {
            //Add IIDs to list and ignore get data
            if (iid_ref->module_iid != module_header.module_iid && num_iid_found == 0)
            {
                iid_ref->module_iid = module_header.module_iid;
                iid_ref->ref_count = 1;
                num_iid_found++;
                //iid_ref++;
            }
            else if (iid_ref->module_iid != module_header.module_iid)
            {
                iid_ref++;
                iid_ref->module_iid = module_header.module_iid;
                iid_ref->ref_count = 1;
                num_iid_found++;
            }
            else
            {
                /* Increase ref count of the current <IID, Ref Count> Obj */
                iid_ref->ref_count++;
            }

            continue;
        }
        else
        {
            /* For other CKVs, if the module is in the default data list, remove it */
            if (!IsNull(info->default_data_iid_list.list))
            {
                iid_ref = (AcdbIidRefCount*)
                    info->default_data_iid_list.list;
                bool_t found_iid = FALSE;
                for (uint32_t j = 0; j < info->
                    default_data_iid_list.count; j++)
                {
                    if ((module_header.module_iid == iid_ref->module_iid)
                        && iid_ref->ref_count > 0)
                    {
                        iid_ref->ref_count--;
                        found_iid = TRUE;
                        break;
                    }
                    else if ((module_header.module_iid == iid_ref->module_iid)
                        && iid_ref->ref_count <= 0)
                        break;

                    iid_ref++;
                }

                if (info->is_default_module_ckv && !found_iid)
                {
                    cur_dot_offset += sizeof(uint32_t);
                    continue;
                }
            }
        }

        //todo: Swap to this impementation when Global Persist is supported
        //status = DataProcGetPersistenceType(
        //    module_header.parameter_id, &param_type);
        //if (AR_FAILED(status))
        //{
        //    ACDB_ERR("Error[%d]: Failed to get parameter persist type "
        //        "for Parameter(0x%x)", status, module_header.parameter_id);
        //    return status;
        //}

        //if (info->param_type != param_type)
        //{
        //    cur_dot_offset += sizeof(uint32_t);
        //    continue;
        //}

        switch (info->param_type)
        {
        case ACDB_DATA_NON_PERSISTENT:
            if (AR_SUCCEEDED(IsPidPersistent(module_header.parameter_id)))
            {
                //cur_def_offset += sizeof(AcdbMiidPidPair);
                cur_dot_offset += sizeof(uint32_t);
                continue;
            }
            break;
        case ACDB_DATA_PERSISTENT:
            if (AR_FAILED(IsPidPersistent(module_header.parameter_id)))
            {
                //cur_def_offset += sizeof(AcdbMiidPidPair);
                cur_dot_offset += sizeof(uint32_t);
                continue;
            }
            break;
        //case ACDB_DATA_GLOBALY_PERSISTENT:
        //    break;
        default:
            break;
        }

        num_param_cal_found++;

        //Get data from Heap
        status = GetSubgraphCalData(info->param_type, ACDB_DATA_ACDB_CLIENT,
            info->module_ckv, info->subgraph_id, module_header.module_iid,
            module_header.parameter_id, blob_offset, rsp);

        if (AR_SUCCEEDED(status))
        {
            //cur_def_offset += sizeof(AcdbMiidPidPair);
            cur_dot_offset += sizeof(uint32_t);
            continue;
        }

        //Get data from file
        status = FileManReadBuffer(
            &data_offset, sizeof(uint32_t), &cur_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read number of "
                "<iid, pid> entries", status);
            return status;
        }

        cur_dpool_offset = ci_data_pool.chunk_offset + data_offset;
        status = FileManReadBuffer(
            &module_header.param_size, sizeof(uint32_t), &cur_dpool_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read param size", status);
            return status;
        }

        padded_param_size = ACDB_ALIGN_8_BYTE(module_header.param_size);

        switch (info->op)
        {
        case ACDB_OP_GET_SIZE:
            rsp->buf_size +=
                sizeof(AcdbDspModuleHeader) + padded_param_size;
            break;
        case ACDB_OP_GET_DATA:
			if (rsp->buf_size >= (*blob_offset + sizeof(AcdbDspModuleHeader)))
			{
                ACDB_MEM_CPY((uint8_t*)rsp->buf + *blob_offset,
                    &module_header, sizeof(AcdbDspModuleHeader));
			}
			else
			{
				return AR_ENEEDMORE;
			}
            *blob_offset += sizeof(AcdbDspModuleHeader);

			if (rsp->buf_size >= (*blob_offset + module_header.param_size))
			{
                if (module_header.param_size > 0)
                {
                    status = FileManReadBuffer(
                        (uint8_t*)rsp->buf + *blob_offset,
                        module_header.param_size,
                        &cur_dpool_offset);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Failed to read param data",
                            status);
                        return status;
                    }
                }
			}
			else
			{
				return AR_ENEEDMORE;
			}
            *blob_offset += padded_param_size;
            break;
        default:
            break;
        }
    }

    if (info->is_default_module_ckv && !info->ignore_get_default_data)
    {
        ACDB_FREE(info->default_data_iid_list.list);
        info->default_data_iid_list.list = NULL;
    }
    else if (info->is_default_module_ckv && info->ignore_get_default_data)
    {
        /* Copy the IID Ref Count list from GLB_BUF_1 since it will be
        * cleared outside of this call */
        info->default_data_iid_list.count = num_iid_found;

        //List of AcdbIidRefCount
        info->default_data_iid_list.list = (uint32_t*)
            ACDB_MALLOC(AcdbIidRefCount, num_iid_found);

        ACDB_MEM_CPY_SAFE(info->default_data_iid_list.list,
            num_iid_found * sizeof(AcdbIidRefCount),
            &glb_buf_1[0], num_iid_found * sizeof(AcdbIidRefCount));

        iid_ref = NULL;
    }

    if (info->is_default_module_ckv && info->ignore_get_default_data)
    {
        return AR_EOK;
    }
    else if (num_param_cal_found == 0) return AR_ENOTEXIST;

    return status;
}

int32_t AcdbFindModuleCKV(AcdbCalKeyTblEntry *ckv_entry,
    AcdbAudioCalContextInfo *info)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_keys_found = 0;
    uint32_t key_table_size = 0;
    uint32_t key_table_entry_size = 0;
    ChunkInfo ci_sg_key_tbl = { 0 };
    ChunkInfo ci_cal_lut = { 0 };
    AcdbGraphKeyVector module_ckv = { 0 };
    KeyTableHeader key_table_header = { 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    if (IsNull(ckv_entry) || IsNull(info))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " ckv entry(%p), info(%p)", AR_EBADPARAM, ckv_entry, info);
        return AR_EBADPARAM;
    }

    if (IsNull(info->module_ckv))
    {
        ACDB_ERR("Error[%d]: The module CKV in the context"
            " info struct is null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_sg_key_tbl.chunk_id = ACDB_CHUNKID_CALKEYTBL;
    ci_cal_lut.chunk_id = ACDB_CHUNKID_CALDATALUT;
    status = ACDB_GET_CHUNK_INFO(&ci_sg_key_tbl, &ci_cal_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph CKV Tables "
            "(Key ID and Value Lookup)chunk information", status);
        return status;
    }

    /*
     * GLB_BUF_1:
     *   1. Used to store the key ids
     *   2. Also stores the CKV LUT used in the partition binary search
     * GLB_BUF_3:
     *   1. The first portion of the buffer stores the Module CKV
     *   2. The other portion stores one CKV LUT entry found
     *      the partition binary search during
     */
    offset = ci_sg_key_tbl.chunk_offset + ckv_entry->offset_cal_key_tbl;
    status = FileManReadBuffer(&module_ckv.num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of CKV keys", status);
        return status;
    }

    if (module_ckv.num_keys > 0)
    {
        status = FileManReadBuffer(&glb_buf_1,
            module_ckv.num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read key id list", status);
        }
    }

    uint32_t ckv_lut_entry_index = module_ckv.num_keys * 2;

    module_ckv.graph_key_vector =
        (AcdbKeyValuePair*)&glb_buf_3[0];
    for (uint32_t i = 0; i < module_ckv.num_keys; i++)
    {
        module_ckv.graph_key_vector[i].key = glb_buf_1[i];
    }

    /* Check if the Module CKV exist within the new CKV. If it doesn't
     * skip this module CKV */
    num_keys_found = 0;
    for (uint32_t k = 0; k < module_ckv.num_keys; k++)
    {
        for (uint32_t l = 0; l < info->ckv->num_keys; l++)
        {
            if (module_ckv.graph_key_vector[k].key ==
                info->ckv->graph_key_vector[l].key)
            {
                glb_buf_3[ckv_lut_entry_index + num_keys_found] =
                    info->ckv->graph_key_vector[l].value;
                module_ckv.graph_key_vector[k].value =
                    info->ckv->graph_key_vector[l].value;
                num_keys_found += 1;
                break;
            }
        }
    }

    if (info->ckv->num_keys == 0 && module_ckv.num_keys != 0 &&
        info->is_first_time)
        return AR_ENOTEXIST;
    if (module_ckv.num_keys == 0 && !info->is_first_time)
        return AR_ENOTEXIST;
    if ((num_keys_found != module_ckv.num_keys) && !info->is_first_time)
        return AR_ENOTEXIST;
    if (!info->is_first_time)
    {
        bool_t deltaKey = FALSE;//Is delta key vector found

        /* We are checking for all the key combinations. Compare the KeyVector
         * KeyIDs in the CalKeyTable to the DeltaKV KeyIDs
         * Case 1:
         *   If all the keys are missing, go to the next CKV KeyID table offset
         * Case 2:
         *   If its a partial match, break and get the module data
         */
        for (uint32_t k = 0; k < module_ckv.num_keys; k++)
        {
            for (uint32_t l = 0; l < info->delta_ckv->num_keys; l++)
            {
                if (module_ckv.graph_key_vector[k].key ==
                    info->delta_ckv->graph_key_vector[l].key)
                {
                    deltaKey = TRUE;
                    break;
                }
            }

            if (deltaKey)
                break;
        }

        if (!deltaKey)
            return AR_ENOTEXIST;
            //continue;//return AR_ENOTEXIST as there is no difference
    }

    //Search LUT for Value Entry
    offset = ci_cal_lut.chunk_offset + ckv_entry->offset_cal_lut;
    status = FileManReadBuffer(&key_table_header, sizeof(KeyTableHeader),
        &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read value lookup table header",
            status);
    }

    if (0 == key_table_header.num_entries)
    {
        /* The lookup table should not be empty. If this is true,
         * then there is an issue with the ACDB file. */

        if (0 == module_ckv.num_keys)
        {
            ACDB_ERR("Error[%d]: Subgraph (0x%x) | There is a module in this"
                " subgraph with missing default calibration data. Check the"
                " subgraph.",
                AR_EFAILED, info->subgraph_id);
        }
        else
        {
            ACDB_ERR("Error[%d]: Subgraph (0x%x) | There is a module in this"
                " subgraph with missing calibration data for the module ckv"
                " below: ",
                AR_EFAILED, info->subgraph_id);

            LogKeyVector(&module_ckv, CAL_KEY_VECTOR);
        }

        return AR_EFAILED;
    }

    key_table_entry_size = (module_ckv.num_keys * sizeof(uint32_t))
        + sizeof(AcdbCkvLutEntryOffsets);
    key_table_size = key_table_header.num_entries * key_table_entry_size;

    //Setup Table Information
    table_info.table_offset = offset;
    table_info.table_size = key_table_size;
    table_info.table_entry_size = key_table_entry_size;

    //Setup Search Information
    search_info.num_search_keys = key_table_header.num_keys;
    search_info.num_structure_elements =
        key_table_header.num_keys + 3;//Values + DEF + DOT + DOT2
    search_info.table_entry_struct = &glb_buf_3[ckv_lut_entry_index];
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        //ACDB_ERR("Error[%d]: Unable to find cal key vector. "
        //    "No matching values were found", status);

        //if (info->op == ACDB_OP_GET_SIZE)
        //    LogKeyVector(&module_ckv, CAL_KEY_VECTOR);

        return status;
    }

    ACDB_MEM_CPY_SAFE(&info->data_offsets, sizeof(AcdbCkvLutEntryOffsets),
        &glb_buf_3[ckv_lut_entry_index + module_ckv.num_keys],
        sizeof(AcdbCkvLutEntryOffsets));

    info->module_ckv->num_keys = module_ckv.num_keys;

    if(info->module_ckv->num_keys > 0)
        ACDB_MEM_CPY_SAFE(info->module_ckv->graph_key_vector,
            module_ckv.num_keys * sizeof(AcdbKeyValuePair),
            module_ckv.graph_key_vector,
            module_ckv.num_keys * sizeof(AcdbKeyValuePair));


    /* Hypervisor
     * If we encounter the Default CKV
     * Indiciate that the default CKV has been found
     * Store the LUT Entry Offsets(DEF DOT, DOT2) separately for the default CKV
     * Skip getting data for the default CKV
     * If the next CKV is what we are looking for and it contains one of the IIDs that are also associated with the default CKV,
     */

    if (module_ckv.num_keys == 0)
    {
        info->is_default_module_ckv = TRUE;
        //info->ignore_get_default_data = TRUE;

        //Read IIDs into a tracking list. For non-default CKVs, if a module is found remove it from the default CKV IID list
    }
    else
    {
        info->is_default_module_ckv = FALSE;
        info->ignore_get_default_data = TRUE;
    }

    return status;
}

/**
* \brief
*		Calculates the Delta Key Vector which is the difference between the
*       old and new key vector and also updates the is_first_time flag in the
*       context info struct
*
* \depends
*       The Delta CKV and CKV[New] in the context info struct must be freed
*       when no longer in use
*
* \param[in] req: Contains CKV[New] and CKV[Old] to compute Delta KV
* \param[out] context_info: Context info containing the Delta CKV and CKV[New]
*             to be updated
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbComputeDeltaCKV(AcdbSgIdCalKeyVector *req,
    AcdbAudioCalContextInfo *context_info)
{
    int32_t status = AR_EOK;
    bool_t first_time = FALSE;
    uint32_t kv_size = 0;
    AcdbGraphKeyVector *cmdKV = NULL;
    AcdbGraphKeyVector *deltaKV = NULL;

    if (IsNull(context_info) || IsNull(req))
    {
        ACDB_ERR("Error[%d]: One or more input parameter(s) is null.",
            AR_EBADPARAM);

        return AR_EBADPARAM;
    }

    cmdKV = ACDB_MALLOC(AcdbGraphKeyVector, 1);
    if (IsNull(cmdKV))
    {
        ACDB_ERR("Error[%d]: Insufficient memory for allocating ckv",
            AR_ENOMEMORY);
        return AR_ENOMEMORY;
    }

    if (req->cal_key_vector_prior.num_keys == 0)
    {
        first_time = TRUE;
        cmdKV->num_keys = req->cal_key_vector_new.num_keys;
        cmdKV->graph_key_vector = ACDB_MALLOC(AcdbKeyValuePair, cmdKV->num_keys);
        if (IsNull(cmdKV))
        {
            ACDB_ERR("Error[%d]: Insufficient memory for allocating ckv",
                AR_ENOMEMORY);
            ACDB_FREE(cmdKV);
            return AR_ENOMEMORY;
        }

        kv_size = cmdKV->num_keys * sizeof(AcdbKeyValuePair);
        ACDB_MEM_CPY_SAFE(cmdKV->graph_key_vector, kv_size,
            req->cal_key_vector_new.graph_key_vector, kv_size);
    }
    else
    {
        status = AcdbSort2(
            req->cal_key_vector_prior.num_keys * sizeof(AcdbKeyValuePair),
            req->cal_key_vector_prior.graph_key_vector,
            sizeof(AcdbKeyValuePair), 0);

        if (AR_FAILED(status))
        {
            ACDB_FREE(cmdKV);
            ACDB_ERR("Error[%d]: Failed to sort the old CKV");
            return status;
        }

        status = AcdbSort2(
            req->cal_key_vector_new.num_keys * sizeof(AcdbKeyValuePair),
            req->cal_key_vector_new.graph_key_vector,
            sizeof(AcdbKeyValuePair), 0);

        if (AR_FAILED(status))
        {
            ACDB_FREE(cmdKV);
            ACDB_ERR("Error[%d]: Failed to sort the new CKV");
            return status;
        }

        //compute delta KV (from prior and new) and number of keys
        if (req->cal_key_vector_prior.num_keys != req->cal_key_vector_new.num_keys)
        {
            ACDB_ERR("CKV[old] num of keys = %d do not match "
                "CKV[new] num of keys = %d",
                req->cal_key_vector_prior.num_keys,
                req->cal_key_vector_new.num_keys);
            return AR_EFAILED;
        }

        cmdKV->num_keys = req->cal_key_vector_new.num_keys;
        cmdKV->graph_key_vector = ACDB_MALLOC(AcdbKeyValuePair, cmdKV->num_keys);
        if (IsNull(cmdKV))
        {
            ACDB_ERR("Error[%d]: Insufficient memory for allocating ckv",
                AR_ENOMEMORY);
            ACDB_FREE(cmdKV);
            return AR_ENOMEMORY;
        }

        kv_size = cmdKV->num_keys * sizeof(AcdbKeyValuePair);
        for (uint32_t i = 0; i < cmdKV->num_keys; i++)
        {
            ACDB_MEM_CPY_SAFE(cmdKV->graph_key_vector, kv_size,
                req->cal_key_vector_new.graph_key_vector, kv_size);
        }

        uint32_t key_count = 0;
        for (uint32_t i = 0; i < req->cal_key_vector_prior.num_keys; i++)
        {
            if (req->cal_key_vector_prior.graph_key_vector[i].key !=
                req->cal_key_vector_new.graph_key_vector[i].key)
            {
                status = AR_ENOTEXIST;
                break;
            }
            if (req->cal_key_vector_prior.graph_key_vector[i].value !=
                req->cal_key_vector_new.graph_key_vector[i].value)
                key_count++;
        }

        if (AR_EOK != status)
        {
            ACDB_ERR_MSG_1("CKV[new] and CKV[old] have ""different sets of "
                "keys", status);
            ACDB_FREE(cmdKV->graph_key_vector);
            ACDB_FREE(cmdKV);
            return status;
        }

        if (key_count == 0)
        {
            ACDB_ERR("Error[%d]: No calibration data can be found. There is no"
                " difference between CKV[new] and CKV[old].", AR_ENOTEXIST);
            ACDB_FREE(cmdKV->graph_key_vector);
            ACDB_FREE(cmdKV);
            return AR_ENOTEXIST;
        }

        deltaKV = ACDB_MALLOC(AcdbGraphKeyVector, 1);
        if (deltaKV == NULL)
        {
            ACDB_ERR("Error[%d]: Insufficient memory for allocating delta ckv",
                AR_ENOMEMORY);
            ACDB_FREE(cmdKV->graph_key_vector);
            ACDB_FREE(cmdKV);
            return AR_ENOMEMORY;
        }

        deltaKV->num_keys = key_count;
        deltaKV->graph_key_vector = ACDB_MALLOC(AcdbKeyValuePair, key_count);
        if (IsNull(deltaKV->graph_key_vector))
        {
            ACDB_FREE(deltaKV);
            ACDB_FREE(cmdKV->graph_key_vector);
            ACDB_FREE(cmdKV);
            ACDB_ERR("Error[%d]: Insufficient memory to allocate delta ckv key"
                " value pair list", AR_ENOMEMORY);
            return AR_ENOMEMORY;
        }

        key_count = 0;
        for (uint32_t i = 0; i < req->cal_key_vector_prior.num_keys; i++)
        {
            if (req->cal_key_vector_prior.graph_key_vector[i].value !=
                req->cal_key_vector_new.graph_key_vector[i].value)
            {
                deltaKV->graph_key_vector[key_count].key =
                    req->cal_key_vector_new.graph_key_vector[i].key;
                deltaKV->graph_key_vector[key_count].value =
                    req->cal_key_vector_new.graph_key_vector[i].value;
                key_count++;
            }
        }
    }

    context_info->ckv = cmdKV;
    context_info->delta_ckv = deltaKV;
    context_info->is_first_time = first_time;

    return status;
}

/**
* \brief
*		Frees memory allocated to the CKV and Delta CKV
*
* \param[in/out] info: Context info containing the Delta CKV and CKV[New]
* \return none
*/
void AcdbClearAudioCalContextInfo(AcdbAudioCalContextInfo* info)
{
    if (IsNull(info))
    {
        ACDB_ERR("Error[%d]: Input parameter is null: info(%p)",
            AR_EBADPARAM, info);
        return;
    }

    if (!IsNull(info->ckv))
    {
        if (!IsNull(info->ckv->graph_key_vector))
            ACDB_FREE(info->ckv->graph_key_vector);
        ACDB_FREE(info->ckv);
    }

    if (!IsNull(info->delta_ckv))
    {
        if (!IsNull(info->delta_ckv->graph_key_vector))
            ACDB_FREE(info->delta_ckv->graph_key_vector);
        ACDB_FREE(info->delta_ckv);
    }

    info->module_ckv = NULL;

    return;
}

int32_t AcdbCmdGetSubgraphCalDataNonPersist(AcdbSgIdCalKeyVector *req,
    AcdbBlob *rsp, uint32_t rsp_struct_size)
{
    int32_t status = AR_EOK;
    bool_t found_sg = FALSE;
    bool_t has_cal_data = FALSE;
    uint32_t offset = 0;
    uint32_t prev_sg_cal_lut_offset = 0;
    uint32_t prev_subgraph_id = 0;
    uint32_t sg_cal_lut_offset = 0;
    uint32_t default_ckv_entry_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t num_subgraph = 0;
    uint32_t num_subgraph_found = 0;
    ChunkInfo ci_sg_cal_lut = { 0 };
    AcdbSgCalLutHeader sg_cal_lut_header = { 0 };
    AcdbCalKeyTblEntry ckv_entry = { 0 };
    AcdbAudioCalContextInfo info =
    {
        ACDB_OP_NONE,
        ACDB_OP_GET_SUBGRAPH_DATA,
        FALSE, FALSE, FALSE,
        { 0 }, 0, 0,
        NULL, NULL, NULL, NULL,
        ACDB_DATA_UNKNOWN,
        { 0 }
    };

    if (rsp_struct_size < sizeof(AcdbBlob))
    {
        ACDB_ERR("Error[%d]: The response structure size is less than %d",
            AR_EBADPARAM, sizeof(AcdbBlob));
        return AR_EBADPARAM;
    }

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " req(%p) rsp(%p)", req, rsp);
        return AR_EBADPARAM;
    }

    ci_sg_cal_lut.chunk_id = ACDB_CHUNKID_CALSGLUT;
    status = ACDB_GET_CHUNK_INFO(&ci_sg_cal_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration LUT "
            "chunk information", status);
        return status;
    }

    info.param_type = ACDB_DATA_NON_PERSISTENT;
    if (rsp->buf_size > 0 && !IsNull(rsp->buf))
    {
        info.op = ACDB_OP_GET_DATA;
        memset(rsp->buf, 0, rsp->buf_size);
    }
    else
        info.op = ACDB_OP_GET_SIZE;

    if (info.op == ACDB_OP_GET_SIZE)
    {
        //Log CKV when this API is called for the first time
        ACDB_DBG("Retrieving non-persistent calibration for "
            "the following cal key vector:");
        ACDB_DBG("CKV[new]:");
        LogKeyVector(&req->cal_key_vector_new, CAL_KEY_VECTOR);
        ACDB_DBG("CKV[old]:");
        LogKeyVector(&req->cal_key_vector_prior, CAL_KEY_VECTOR);
    }

    /* CKV Setup
     * 1. GLB_BUF_2 stores the module CKV that will be used to locate data in
     * the heap(if it exists)
     * 2. The Delta CKV and CKV[New] are allocated on the heap by
     * AcdbComputeDeltaCKV
     *
     * tmp - stores the amount of padding after graph_key_vectornum_keys
     * tmp2 - stores the position of the KeyValuePair list
     *
     * The first info.module_ckv->graph_key_vector assignment has the effect
     * of storing the address of the pointer in glb_buf_2 after
     * graph_key_vector.num_keys
     *
     * The second info.module_ckv->graph_key_vector assignment has the effect
     * of pointer the info.module_ckv->graph_key_vector to the actual list
     * of key value pairs
     *
     * Whats inside global buffer 2 after these operations?
     *
     * The first 'sizeof(AcdbGraphKeyVector)' bytes is AcdbGraphKeyVector
     * followed by 'sizeof(AcdbKeyValuePair) * num_keys' bytes which is
     * the KV list
     */
    info.module_ckv = (AcdbGraphKeyVector*)&glb_buf_2[0];
    uint32_t tmp = (sizeof(AcdbGraphKeyVector) - sizeof(AcdbKeyValuePair *)) / sizeof(uint32_t);
    uint32_t tmp2 = sizeof(AcdbGraphKeyVector) / sizeof(uint32_t);
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp];
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp2];

    status = AcdbComputeDeltaCKV(req, &info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get the delta CKV", status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    offset = ci_sg_cal_lut.chunk_offset;
    status = FileManReadBuffer(&num_subgraph,
        sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of Subgraphs",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    /* Sort the subgraph list so that the linear search for subgraph
     * takes less time. The Subgraph CAL LUT sorts the subgraphs. By
     * sorting the input subgraph list we can locate the subgraphs
     * in the chunk sequentially. Therefore there is no need to reset
     * the table offset */
    status = AcdbSort2(req->num_sg_ids * sizeof(uint32_t),
        req->sg_ids, sizeof(uint32_t), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort subgraph list",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        found_sg = FALSE;
        has_cal_data = FALSE;
        /*offset = tmp_offset;*/

        while(offset < ci_sg_cal_lut.chunk_offset + ci_sg_cal_lut.chunk_size)
        {
            status = FileManReadBuffer(&sg_cal_lut_header,
                sizeof(AcdbSgCalLutHeader), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read Subgraph "
                    "Cal LUT header", status);
            }

            if (req->sg_ids[i] == sg_cal_lut_header.subgraph_id)
            {
                found_sg = TRUE;
                break;
            }
            else if (
                (req->sg_ids[i] < sg_cal_lut_header.subgraph_id) ||
                (req->sg_ids[i] > prev_subgraph_id &&
                 req->sg_ids[i] < sg_cal_lut_header.subgraph_id))
            {
                /* The input subgraph is a voice subgraph or it
                 * does not exist */
                offset = prev_sg_cal_lut_offset;
                break;
            }
            else
            {
                //Go to next Subgraph entry
                offset += (sg_cal_lut_header.num_ckv_entries
                        * sizeof(AcdbCalKeyTblEntry));
            }

            prev_sg_cal_lut_offset = offset;
            prev_subgraph_id = sg_cal_lut_header.subgraph_id;
        }

        if (!found_sg)
        {
            ACDB_ERR("Error[%d]: Subgraph(%x) is not an audio subgraph"
                " or does not exist. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            continue;
        }

        if (sg_cal_lut_header.num_ckv_entries == 0)
        {
            ACDB_ERR("Error[%d]: Subgraph(%x) contains zero CKV entries in"
                " the Subgraph Cal LUT. At least one entry should be"
                " present. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            continue;
        }

        info.ignore_get_default_data =
            sg_cal_lut_header.num_ckv_entries == 1 ? FALSE : TRUE;

        //The default CKV will be skipped and handeled last
        if (sg_cal_lut_header.num_ckv_entries > 1 && info.is_first_time)
            sg_cal_lut_header.num_ckv_entries++;

        for (uint32_t k = 0; k < sg_cal_lut_header.num_ckv_entries; k++)
        {
            /* The Default CKV is always the first CKV in the list of CKVs to
             * search. It will be skipped and processed last */
            if ((k == sg_cal_lut_header.num_ckv_entries - 1) &&
                info.ignore_get_default_data == TRUE && info.is_first_time)
            {
                sg_cal_lut_offset = default_ckv_entry_offset;
                info.ignore_get_default_data = FALSE;

                status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                    &sg_cal_lut_offset);
            }
            else
            {
                status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                    &offset);
                sg_cal_lut_offset = offset;
            }

            if (AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            info.subgraph_id = sg_cal_lut_header.subgraph_id;

            status = AcdbFindModuleCKV(&ckv_entry, &info);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                continue;
            }
            else if(AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            if (info.is_default_module_ckv)
            {
                default_ckv_entry_offset =
                    sg_cal_lut_offset - sizeof(ckv_entry);
            }

            //Get Calibration
            status = AcdbGetSubgraphCalibration(&info, &blob_offset, rsp);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                //No calibration found for subgraph with module ckv_entry
                has_cal_data = FALSE;
                continue;
            }
            else if (AR_FAILED(status) && status != AR_ENOTEXIST)
            {
                ACDB_ERR("Error[%d]: An error occured while getting "
                    "calibration data for Subgraph(%x)", status,
                    sg_cal_lut_header.subgraph_id);
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            has_cal_data = TRUE;
        }

        if (AR_FAILED(status) && !has_cal_data)
        {
            ACDB_ERR("Error[%d]: Unable to retrieve calibration data for "
                "Subgraph(%x). Skipping..", status,
                sg_cal_lut_header.subgraph_id);
            status = AR_EOK;
        }
        else
        {
            status = AR_EOK;
            num_subgraph_found++;
        }
    }

    if (num_subgraph_found == 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: No calibration found", status);
    }

    //Clean Up Context Info
    AcdbClearAudioCalContextInfo(&info);

    return status;
}

int32_t GetVcpmSubgraphCalTable(uint32_t num_subgraphs, uint32_t subgraph_id,
    uint32_t *file_offset, AcdbVcpmSubgraphCalTable *subgraph_cal_table)
{
    int32_t status = AR_EOK;
    bool_t found = FALSE;
    uint32_t offset = *file_offset;

    if (IsNull(subgraph_cal_table) || IsNull(file_offset))
    {
        return AR_EBADPARAM;
    }

    for (uint32_t i = 0; i < num_subgraphs; i++)
    {
        status = FileManReadBuffer(subgraph_cal_table, sizeof(AcdbVcpmSubgraphCalTable), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Module Header.", status);
            return status;
        }

        if (subgraph_id == subgraph_cal_table->subgraph_id)
        {
            *file_offset = offset;
            found = TRUE;
            return AR_EOK;
        }

        /* Minus sizeof <Major, Minor, Offset in MKT, Num CKV Tbls> since
        this was read as part of FileManReadBuffer for subgraph_cal_table */
        offset += subgraph_cal_table->table_size - (4 * sizeof(uint32_t));
    }

    status = !found ? AR_ENOTEXIST : status;

    return status;
}

int32_t GetVcpmMasterKeyTable(AcdbVcpmBlobInfo *vcpm_info,
	uint32_t file_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    uint32_t sz_key_table = 0;
    ChunkInfo ci_vcpm_master_key = { 0 };

    ci_vcpm_master_key.chunk_id = ACDB_CHUNKID_VCPM_MASTER_KEY;
    status = AcdbGetChunkInfo(ci_vcpm_master_key.chunk_id,
        &ci_vcpm_master_key.chunk_offset, &ci_vcpm_master_key.chunk_size);
    if (AR_FAILED(status)) return status;

    offset = ci_vcpm_master_key.chunk_offset + file_offset;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of vcpm master keys.", status);
        return status;
    }

    sz_key_table = num_keys * sizeof(AcdbVcpmKeyInfo);

    switch (vcpm_info->op)
    {
    case ACDB_OP_GET_SIZE:
        vcpm_info->chunk_master_key.size += sizeof(uint32_t);

        if (sz_key_table > 0)
        {
            vcpm_info->chunk_master_key.size += sz_key_table;
        }

        break;
    case ACDB_OP_GET_DATA:

        if (IsNull(rsp->buf)) return AR_EFAILED;

        //Write Num Keys
		if (tot_cal_size >= (vcpm_info->chunk_master_key.offset + sizeof(uint32_t)))
		{
        ACDB_MEM_CPY(
            (uint8_t*)rsp->buf + vcpm_info->chunk_master_key.offset,
            &num_keys, sizeof(uint32_t));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_info->chunk_master_key.offset += sizeof(uint32_t);

        if (sz_key_table > 0)
        {
            //Write Chunk data
			if (tot_cal_size >= (vcpm_info->chunk_master_key.offset + sz_key_table))
			{
            status = FileManReadBuffer(
                (uint8_t*)rsp->buf + vcpm_info->chunk_master_key.offset,
                sz_key_table, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read "
                    "number of vcpm master keys.", status);
                return status;
            }
			}
			else
			{
				return AR_ENEEDMORE;
			}
            vcpm_info->chunk_master_key.offset += sz_key_table;
        }
        break;
    default:
        break;
    }

    //It is possible for the Master Key Table to have 0 keys
    //else if(sz_key_table == 0)
    //{
    //
    //}

    return status;
}

int32_t GetVcpmCalibrationKeyIDTable(AcdbVcpmBlobInfo *vcpm_info,
	uint32_t table_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    uint32_t sz_key_table = 0;
    ChunkInfo ci_vcpm_cal_key = { 0 };

    ci_vcpm_cal_key.chunk_id = ACDB_CHUNKID_VCPM_CALKEY_TABLE;
    status = AcdbGetChunkInfo(ci_vcpm_cal_key.chunk_id,
        &ci_vcpm_cal_key.chunk_offset, &ci_vcpm_cal_key.chunk_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to get VCPM Cal Key Table Chunk");
        return status;
    }

    offset = ci_vcpm_cal_key.chunk_offset + table_offset;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of voice calibration keys.", status);
        return status;
    }

    sz_key_table = num_keys * sizeof(uint32_t);

    switch (vcpm_info->op)
    {
    case ACDB_OP_GET_SIZE:

        vcpm_info->chunk_cal_key_id.size += sizeof(uint32_t);

        if (sz_key_table > 0)
        {
            vcpm_info->chunk_cal_key_id.size += sz_key_table;
        }
        break;
    case ACDB_OP_GET_DATA:

        if (IsNull(rsp->buf)) return AR_EFAILED;

        /* If the Master Key table doesnt have any keys, then it is evident
         * that the voice key table is empty. It is a valid scenario if
         * num_keys is zero.
         */

        //Write Cal Key Table to Voc Cal Key Chunk
        //Write Num Keys
		if (tot_cal_size >= (vcpm_info->chunk_cal_key_id.offset + sizeof(num_keys)))
		{
        ACDB_MEM_CPY((uint8_t*)rsp->buf + vcpm_info->chunk_cal_key_id.offset,
            &num_keys, sizeof(num_keys));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_info->chunk_cal_key_id.offset += sizeof(num_keys);

        if (sz_key_table > 0)
        {
            //Write Key IDs
			if (tot_cal_size >= (vcpm_info->chunk_cal_key_id.offset + sz_key_table))
			{
            status = FileManReadBuffer(
                (uint8_t*)rsp->buf + vcpm_info->chunk_cal_key_id.offset,
                sz_key_table, &offset);
			}
			else
			{
				return AR_ENEEDMORE;
			}
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read voice calibration keys.",
                    status);
                return status;
            }
            vcpm_info->chunk_cal_key_id.offset += sz_key_table;
        }
        break;
    default:
        break;
    }

    return status;
}

int32_t GetVcpmCalibrationKeyValueTable(AcdbVcpmBlobInfo *vcpm_info, uint32_t table_offset,
	AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    uint32_t sz_key_table = 0;
    ChunkInfo ci_vcpm_cal_lut = { 0 };

    ci_vcpm_cal_lut.chunk_id = ACDB_CHUNKID_VCPM_CALDATA_LUT;
    status = AcdbGetChunkInfo(ci_vcpm_cal_lut.chunk_id,
        &ci_vcpm_cal_lut.chunk_offset, &ci_vcpm_cal_lut.chunk_size);
    if (AR_FAILED(status)) return status;

    offset = ci_vcpm_cal_lut.chunk_offset + table_offset;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of voice calibration key values.", status);
        return status;
    }

    sz_key_table = num_keys * sizeof(uint32_t);

    switch (vcpm_info->op)
    {
    case ACDB_OP_GET_SIZE:

        vcpm_info->chunk_cal_key_lut.size += sizeof(uint32_t);

        if (sz_key_table > 0)
        {
            vcpm_info->chunk_cal_key_lut.size += sz_key_table;
        }
        break;
    case ACDB_OP_GET_DATA:

        if (IsNull(rsp->buf)) return AR_EFAILED;

        //Write Num Keys
		if (tot_cal_size >= (vcpm_info->chunk_cal_key_lut.offset + sizeof(num_keys)))
		{
        ACDB_MEM_CPY((uint8_t*)rsp->buf + vcpm_info->chunk_cal_key_lut.offset,
            &num_keys, sizeof(num_keys));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_info->chunk_cal_key_lut.offset += sizeof(num_keys);

        if (sz_key_table > 0)
        {
            //Write Key Values
            offset += sizeof(uint32_t);//num entries is always 1
			if (tot_cal_size >= (vcpm_info->chunk_cal_key_lut.offset + sz_key_table))
			{
            status = FileManReadBuffer((uint8_t*)rsp->buf +
                vcpm_info->chunk_cal_key_lut.offset, sz_key_table, &offset);
			}
			else
			{
				return AR_ENEEDMORE;
			}
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read voice calibration key values.", status);
                return status;
            }
            vcpm_info->chunk_cal_key_lut.offset += sz_key_table;
        }
        break;
    default:
        break;
    }

    return status;
}

int32_t GetVcpmModuleParamPair(
    AcdbVcpmBlobInfo *vcpm_info, AcdbMiidPidPair *id_pair,
	AcdbVcpmParamInfo *param_info, uint32_t table_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    ChunkInfo ci_vcpm_cal_def = { 0 };

    if (IsNull(vcpm_info) || IsNull(id_pair) ||
        IsNull(param_info) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameter(s) is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_vcpm_cal_def.chunk_id = ACDB_CHUNKID_VCPM_CALDATA_DEF;
    status = AcdbGetChunkInfo(ci_vcpm_cal_def.chunk_id,
        &ci_vcpm_cal_def.chunk_offset, &ci_vcpm_cal_def.chunk_size);
    if (AR_FAILED(status)) return status;

    //Skip num_mid_pid_pairs
    offset = ci_vcpm_cal_def.chunk_offset + table_offset + sizeof(uint32_t);

    status = FileManReadBuffer(id_pair, sizeof(AcdbMiidPidPair), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read <iid, pid>.", status);
        return status;
    }

    param_info->is_persistent =
        AR_SUCCEEDED(
            IsPidPersistent(id_pair->parameter_id)
        ) ? 0x1 : 0x0;

    switch (vcpm_info->op)
    {
    case ACDB_OP_GET_SIZE:
        vcpm_info->chunk_data_pool.size += sizeof(AcdbMiidPidPair);
        break;
    case ACDB_OP_GET_DATA:
        if (IsNull(rsp->buf)) return AR_EFAILED;

		if (tot_cal_size >= (vcpm_info->chunk_data_pool.offset + sizeof(AcdbMiidPidPair)))
		{
        ACDB_MEM_CPY((uint8_t*)rsp->buf + vcpm_info->chunk_data_pool.offset,
            id_pair, sizeof(AcdbMiidPidPair));
		}
		else
		{
			return AR_ENEEDMORE;
		}

        param_info->offset_vcpm_data_pool =
            vcpm_info->chunk_data_pool.offset -
            vcpm_info->chunk_data_pool.base_offset;
        vcpm_info->chunk_data_pool.offset += sizeof(AcdbMiidPidPair);
        break;
    default:
        break;
    }
    return status;
}

int32_t GetVcpmParamPayload(
    AcdbVcpmBlobInfo *vcpm_info, AcdbMiidPidPair *id_pair,
    AcdbVcpmParamInfo *param_info,
    uint32_t table_offset, AcdbBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t data_pool_blob_offset = 0;
    uint32_t param_size = 0;
    uint32_t padded_param_size = 0;
    uint32_t padding = 0;
    uint32_t error_code = 0;
    AcdbBlob blob = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbGraphKeyVector kv = { 0 };
    AcdbDataPersistanceType persist_type = ACDB_DATA_UNKNOWN;

    if (IsNull(vcpm_info) || IsNull(id_pair) ||
        IsNull(param_info) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameter(s) is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    persist_type = (AcdbDataPersistanceType)param_info->is_persistent;

    //Get data from heap
    blob.buf = rsp->buf;
	blob.buf_size = rsp->buf_size;
    status = GetSubgraphCalData(
        persist_type, ACDB_DATA_ACDB_CLIENT,
        &kv, vcpm_info->subgraph_id,
        id_pair->module_iid, id_pair->parameter_id,
        &data_pool_blob_offset, &blob);

    if (AR_SUCCEEDED(status))
    {
        switch (vcpm_info->op)
        {
        case ACDB_OP_GET_SIZE:
            vcpm_info->chunk_data_pool.size +=
                blob.buf_size;
            break;
        case ACDB_OP_GET_DATA:
            if (IsNull(rsp->buf)) return AR_EFAILED;
            vcpm_info->chunk_data_pool.offset += data_pool_blob_offset;
            break;

        default:
            break;
        }

        return status;
    }

    status = AR_EOK;

    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    status = AcdbGetChunkInfo(ci_data_pool.chunk_id,
        &ci_data_pool.chunk_offset, &ci_data_pool.chunk_size);
    if (AR_FAILED(status)) return status;

    offset = ci_data_pool.chunk_offset + table_offset;
    status = FileManReadBuffer(&param_size, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read parameter size.", status);
        return status;
    }

    padded_param_size = ACDB_ALIGN_8_BYTE(param_size);
    padding = padded_param_size - param_size;

    switch (vcpm_info->op)
    {
    case ACDB_OP_GET_SIZE:
        //Write size, errorcode, payload
        vcpm_info->chunk_data_pool.size +=
            2 * sizeof(uint32_t) + padded_param_size;
        break;
    case ACDB_OP_GET_DATA:
        if (IsNull(rsp->buf)) return AR_EFAILED;

        //Param Size
        ACDB_MEM_CPY(rsp->buf + vcpm_info->chunk_data_pool.offset,
            &param_size, sizeof(uint32_t));
        vcpm_info->chunk_data_pool.offset += sizeof(uint32_t);

        //Error Code
        ACDB_MEM_CPY(rsp->buf + vcpm_info->chunk_data_pool.offset,
            &error_code, sizeof(uint32_t));
        vcpm_info->chunk_data_pool.offset += sizeof(uint32_t);

        if (param_size == 0) return status;

        //Payload
        status = FileManReadBuffer(
            (uint8_t*)rsp->buf + vcpm_info->chunk_data_pool.offset,
            (size_t)param_size, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read parameter payload.", status);
            return status;
        }
        vcpm_info->chunk_data_pool.offset += param_size;

        //Padding
        memset((uint8_t*)rsp->buf + vcpm_info->chunk_data_pool.offset,
            0, padding);

        vcpm_info->chunk_data_pool.offset += padding;
        break;

    default:
        break;
    }

    return status;
}

int32_t GetVcpmCalDataObject(AcdbVcpmBlobInfo *vcpm_info, uint32_t *cal_obj_size,
	uint32_t *table_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t file_offset = *table_offset;
    uint32_t offset_data_pool = 0;
    uint32_t offset_vcpm_ckv_lut = 0;
    AcdbVcpmCalDataObj data_obj = { 0 };
    AcdbVcpmParamInfo param_info = { 0 };
    AcdbMiidPidPair id_pair = { 0 };

    if (IsNull(vcpm_info) || IsNull(cal_obj_size) || IsNull(table_offset) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: Input parameter(s) is null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    file_offset = *table_offset;
    status = FileManReadBuffer(&data_obj, sizeof(AcdbVcpmCalDataObj), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read cal data object.", status);
        return status;
    }

    offset_vcpm_ckv_lut = data_obj.offset_vcpm_ckv_lut;
    data_obj.offset_vcpm_ckv_lut = vcpm_info->chunk_cal_key_lut.offset;

    /* Write CKV LUT offset and num param payload offsets to
     * VCPM Subgraph Info Chunk.
     */
    *cal_obj_size += 2 * sizeof(uint32_t);//<LUT Offset, NumDataOffsets>
    if (!IsNull(rsp->buf) && vcpm_info->op == ACDB_OP_GET_DATA)
    {
        data_obj.offset_vcpm_ckv_lut -=
            vcpm_info->chunk_cal_key_lut.base_offset;

		if (tot_cal_size >= (vcpm_info->chunk_subgraph_info.offset + (2 * sizeof(uint32_t))))
		{
        ACDB_MEM_CPY(rsp->buf + vcpm_info->chunk_subgraph_info.offset,
            &data_obj.offset_vcpm_ckv_lut, sizeof(uint32_t));
        vcpm_info->chunk_subgraph_info.offset += sizeof(uint32_t);

        ACDB_MEM_CPY(rsp->buf + vcpm_info->chunk_subgraph_info.offset,
            &data_obj.num_data_offsets, sizeof(uint32_t));
        vcpm_info->chunk_subgraph_info.offset += sizeof(uint32_t);
    }
		else
		{
			return AR_ENEEDMORE;
		}
	}

    status = GetVcpmCalibrationKeyValueTable(vcpm_info,
		offset_vcpm_ckv_lut, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get CKV LUT", status);
        return status;
    }

    for (uint32_t i = 0; i < data_obj.num_data_offsets; i++)
    {
        status = GetVcpmModuleParamPair(
			vcpm_info, &id_pair, &param_info, data_obj.offset_cal_def, rsp, tot_cal_size);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to get <IID, PID> %d/%d",
                status, i, data_obj.num_data_offsets);
            return status;
        }

        *cal_obj_size += sizeof(AcdbVcpmParamInfo);

        status = FileManReadBuffer(
            &offset_data_pool, sizeof(uint32_t), &file_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read CalDataObj's data"
                " pool offset. Offset %d of %d",
                status, i, data_obj.num_data_offsets);
            return status;
        }

        status = GetVcpmParamPayload(
            vcpm_info, &id_pair, &param_info, offset_data_pool, rsp);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to get payload for"
                " <IID, PID> %d/%d",
                status, i, data_obj.num_data_offsets);
            return status;
        }

        //Go to next Global Data Pool Offset within the CalDataObj
        //file_offset += sizeof(uint32_t);
        //Go to next <IID, PID> pair within the Cal Def Table
        data_obj.offset_cal_def += sizeof(AcdbMiidPidPair);

        //Write Param Info to VCPM Subgraph info Chunk
        if (!IsNull(rsp->buf) && vcpm_info->op == ACDB_OP_GET_DATA)
        {
            //param_info
			if (tot_cal_size >= (vcpm_info->chunk_subgraph_info.offset + sizeof(AcdbVcpmParamInfo)))
			{
            ACDB_MEM_CPY(rsp->buf + vcpm_info->chunk_subgraph_info.offset,
                &param_info, sizeof(AcdbVcpmParamInfo));
			}
			else
			{
				return AR_ENEEDMORE;
			}
            vcpm_info->chunk_subgraph_info.offset += sizeof(AcdbVcpmParamInfo);
        }
    }

    *table_offset = file_offset;
    return status;
}

int32_t GetVcpmCalibrationDataTable(AcdbVcpmBlobInfo *vcpm_info,
	uint32_t *data_table_size, uint32_t *table_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t offset_voice_key_table = 0; //file offset
    uint32_t table_blob_offset = 0;
    uint32_t cal_dot_size = 0;
    AcdbVcpmCkvDataTable caldata_table = { 0 };

    if (IsNull(vcpm_info) || IsNull(data_table_size) ||
        IsNull(rsp) || IsNull(table_offset))
    {
        ACDB_ERR("Error[%d]: Input paramert(s) is null");
        return AR_EBADPARAM;
    }

    offset = *table_offset;
    status = FileManReadBuffer(&caldata_table,
        sizeof(AcdbVcpmCkvDataTable), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Module Header.", status);
        return status;
    }

    offset_voice_key_table = caldata_table.offset_voice_key_table;
    caldata_table.offset_voice_key_table = vcpm_info->chunk_cal_key_id.offset;

    //Write Voc Cal Key Table to Voc Cal Key Chunk
    status = GetVcpmCalibrationKeyIDTable(vcpm_info,
		offset_voice_key_table, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Module Header.", status);
        return status;
    }

    //Make room to write header and remember table offset
    if (vcpm_info->op == ACDB_OP_GET_DATA)
    {
        table_blob_offset = vcpm_info->chunk_subgraph_info.offset;
        vcpm_info->chunk_subgraph_info.offset += sizeof(caldata_table);
    }

    //Clear cal data table size read from acdb and recaculate for VCPM blob
    caldata_table.table_size = 0;

    //Read Cal Data Objects
    for (uint32_t i = 0; i < caldata_table.num_caldata_obj; i++)
    {
        status = GetVcpmCalDataObject(vcpm_info, &cal_dot_size,
			&offset, rsp, tot_cal_size);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to get VCPM Cal"
                " Data Object. Skipping..", status);
            continue;
        }
    }

    /* Size of the CKV Data Table is the size of the header:
    * <Voice Cal Key Table Offset, Cal Dot Size, Cal Data Obj Count>
    * + Cal Dot Size */
    caldata_table.table_size = 3 * sizeof(uint32_t)
        + cal_dot_size;
    caldata_table.cal_dot_size = cal_dot_size;
    *data_table_size += sizeof(uint32_t) + caldata_table.table_size;

    //Write CKV Data Table Header info to VCPM Subgraph Info Chunk
    if (!IsNull(rsp->buf) && vcpm_info->op == ACDB_OP_GET_DATA)
    {
        caldata_table.offset_voice_key_table -=
            vcpm_info->chunk_cal_key_id.base_offset;
		if (tot_cal_size >= (table_blob_offset + sizeof(caldata_table)))
		{
        ACDB_MEM_CPY(rsp->buf + table_blob_offset,
            &caldata_table, sizeof(caldata_table));
    }
		else
		{
			return AR_ENEEDMORE;
		}
	}

    *table_offset = offset;
    return status;
}

int32_t GetVcpmSubgraphChunkData(AcdbVcpmBlobInfo *vcpm_info,
    uint32_t table_offset, AcdbVcpmSubgraphCalTable *subgraph_cal_table,
	AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t chunk_size = 0;
    //Number of Subgraphs is always 1 for each VCPM Subgraph Blob
    uint32_t num_subgraphs = 1;
    uint32_t offset_vcpm_master_key_table = 0;
    uint32_t cal_data_table_size = 0;
    uint32_t chunk_offset_master_key =
        vcpm_info->chunk_master_key.base_offset;
    uint32_t chunk_offset_voice_cal_key =
        vcpm_info->chunk_cal_key_id.base_offset;
    uint32_t chunk_offset_voice_cal_lut =
        vcpm_info->chunk_cal_key_lut.base_offset;
    uint32_t chunk_offset_data_pool =
        vcpm_info->chunk_data_pool.base_offset;

    if (IsNull(vcpm_info) || IsNull(subgraph_cal_table) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    vcpm_info->op = ACDB_OP_GET_DATA;

    //Make room to write Chunk Size for each chunk
    vcpm_info->chunk_master_key.base_offset += sizeof(chunk_size);
    vcpm_info->chunk_master_key.offset += sizeof(chunk_size);
    vcpm_info->chunk_cal_key_id.base_offset += sizeof(chunk_size);
    vcpm_info->chunk_cal_key_id.offset += sizeof(chunk_size);
    vcpm_info->chunk_cal_key_lut.base_offset += sizeof(chunk_size);
    vcpm_info->chunk_cal_key_lut.offset += sizeof(chunk_size);
    vcpm_info->chunk_data_pool.base_offset += sizeof(chunk_size);
    vcpm_info->chunk_data_pool.offset += sizeof(chunk_size);

    //Write Subgraph Cal Data Table Header to VCPM Subgraph Info and move blob offset
    offset_vcpm_master_key_table =
        subgraph_cal_table->offset_vcpm_master_key_table;

    subgraph_cal_table->offset_vcpm_master_key_table = 0;
    if (!IsNull(rsp->buf))
    {
		if (tot_cal_size >= (vcpm_info->chunk_subgraph_info.offset + sizeof(uint32_t)))
		{
        ACDB_MEM_CPY((uint8_t*)rsp->buf + vcpm_info->chunk_subgraph_info.offset,
            &num_subgraphs, sizeof(uint32_t));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_info->chunk_subgraph_info.offset += sizeof(uint32_t);

		if (tot_cal_size >= (vcpm_info->chunk_subgraph_info.offset + sizeof(AcdbVcpmSubgraphCalTable)))
		{
        ACDB_MEM_CPY((uint8_t*)rsp->buf + vcpm_info->chunk_subgraph_info.offset,
            subgraph_cal_table, sizeof(AcdbVcpmSubgraphCalTable));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_info->chunk_subgraph_info.offset += sizeof(AcdbVcpmSubgraphCalTable);
    }

    status = GetVcpmMasterKeyTable(vcpm_info,
		offset_vcpm_master_key_table, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        return status;
    }

    for (uint32_t i = 0; i < subgraph_cal_table->num_ckv_data_table; i++)
    {
        status = GetVcpmCalibrationDataTable(vcpm_info, &cal_data_table_size,
			&table_offset, rsp, tot_cal_size);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Subgraph(0x%x) - Failed to get Calibration Data Table %d/%d",
                vcpm_info->subgraph_id, i, subgraph_cal_table->num_ckv_data_table);
            return status;
        }
    }

    /*Write Chunk Sizes for mkt, vckt, vclut, and vcpmdp*/
    if (rsp->buf != NULL)
    {

        chunk_size = vcpm_info->chunk_master_key.size;
		if (tot_cal_size >= (chunk_offset_master_key + sizeof(chunk_size)))
		{
        ACDB_MEM_CPY(rsp->buf + chunk_offset_master_key,
            &chunk_size, sizeof(chunk_size));
		}
		else
		{
			return AR_ENEEDMORE;
		}

        chunk_size = vcpm_info->chunk_cal_key_id.size;
		if (tot_cal_size >= (chunk_offset_voice_cal_key + sizeof(chunk_size)))
		{
        ACDB_MEM_CPY(rsp->buf + chunk_offset_voice_cal_key,
            &chunk_size, sizeof(chunk_size));
		}
		else
		{
			return AR_ENEEDMORE;
		}

        chunk_size = vcpm_info->chunk_cal_key_lut.size;
		if (tot_cal_size >= (chunk_offset_voice_cal_lut + sizeof(chunk_size)))
		{
        ACDB_MEM_CPY(rsp->buf + chunk_offset_voice_cal_lut,
            &chunk_size, sizeof(chunk_size));
		}
		else
		{
			return AR_ENEEDMORE;
		}

        chunk_size = vcpm_info->chunk_data_pool.size;
		if (tot_cal_size >= (chunk_offset_data_pool + sizeof(chunk_size)))
		{
        ACDB_MEM_CPY(rsp->buf + chunk_offset_data_pool,
            &chunk_size, sizeof(chunk_size));
    }
		else
		{
			return AR_ENEEDMORE;
		}
	}

    return status;
}

int32_t GetVcpmSubgraphChunkInfo(AcdbVcpmBlobInfo *vcpm_info,
    uint32_t table_offset, AcdbVcpmSubgraphCalTable *subgraph_cal_table,
	AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t blob_offset = 0;
    //uint32_t cal_data_table_size = 0;

    vcpm_info->op = ACDB_OP_GET_SIZE;

    status = GetVcpmMasterKeyTable(vcpm_info,
		subgraph_cal_table->offset_vcpm_master_key_table, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        return status;
    }

    //Clear table size read from file and recalculated based on blob
    subgraph_cal_table->table_size = 0;

    for (uint32_t i = 0; i < subgraph_cal_table->num_ckv_data_table; i++)
    {
        status = GetVcpmCalibrationDataTable(vcpm_info, &subgraph_cal_table->table_size/*&cal_data_table_size*/,
			&table_offset, rsp, tot_cal_size);
        if (AR_FAILED(status))
        {
            //Try to get the data that we can even if we fail
            continue;
        }

        //subgraph_cal_table->table_size += cal_data_table_size;
    }

    //Size of <Major, Minor, Master Key Off., #Data Tables>
    subgraph_cal_table->table_size += 4 * sizeof(uint32_t);

    vcpm_info->chunk_subgraph_info.size =
        sizeof(uint32_t) + //num_subgraphs
        sizeof(subgraph_cal_table->subgraph_id) +
        sizeof(subgraph_cal_table->table_size) +
        subgraph_cal_table->table_size;

    //Calculate Chunk Offsets to use when building the response
    blob_offset = vcpm_info->offset_vcpm_blob;
    blob_offset += sizeof(AcdbDspModuleHeader);//VCPM Blob Header

    vcpm_info->chunk_subgraph_info.base_offset = blob_offset;
    vcpm_info->chunk_subgraph_info.offset = blob_offset;
    blob_offset += vcpm_info->chunk_subgraph_info.size;

    vcpm_info->chunk_master_key.base_offset = blob_offset;
    vcpm_info->chunk_master_key.offset = blob_offset;
    blob_offset += sizeof(uint32_t) + vcpm_info->chunk_master_key.size;

    vcpm_info->chunk_cal_key_id.base_offset = blob_offset;
    vcpm_info->chunk_cal_key_id.offset = blob_offset;
    blob_offset += sizeof(uint32_t) + vcpm_info->chunk_cal_key_id.size;

    vcpm_info->chunk_cal_key_lut.base_offset = blob_offset;
    vcpm_info->chunk_cal_key_lut.offset = blob_offset;
    blob_offset += sizeof(uint32_t) + vcpm_info->chunk_cal_key_lut.size;

    vcpm_info->chunk_data_pool.base_offset = blob_offset;
    vcpm_info->chunk_data_pool.offset = blob_offset;

    return status;
}

int32_t BuildVcpmBlob(uint32_t subgraph_id, uint32_t blob_offset, AcdbBlob *rsp, uint32_t tot_cal_size)
{
    int32_t status = AR_EOK;
    uint32_t file_offset = 0;
    uint32_t vcpm_blob_offset = 0;
    uint32_t padding = 0;
    uint32_t padded_param_size = 0;
    ChunkInfo ci_vcpm_cal = { 0 };
    AcdbVcpmSubgraphCalHeader subgraph_cal_header = { 0 };
    AcdbDspModuleHeader fwk_module_header = { 0 };
    AcdbVcpmSubgraphCalTable subgraph_cal_table_header = { 0 };
    AcdbVcpmBlobInfo vcpm_info =
    {
        ACDB_OP_NONE, 0, 0,
        { 0 },{ 0 },{ 0 },{ 0 },{ 0 }
    };

    ci_vcpm_cal.chunk_id = ACDB_CHUNKID_VCPM_CAL_DATA;
    status = AcdbGetChunkInfo(ci_vcpm_cal.chunk_id,
        &ci_vcpm_cal.chunk_offset, &ci_vcpm_cal.chunk_size);
    if (AR_FAILED(status)) return status;

    file_offset = ci_vcpm_cal.chunk_offset;
    status = FileManReadBuffer(&subgraph_cal_header,
        sizeof(AcdbVcpmSubgraphCalHeader), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Module Header.", status);
        return status;
    }

    //Search for the matching VCPM Subgraph Cal Data Table
    status = GetVcpmSubgraphCalTable(subgraph_cal_header.num_subgraphs,
        subgraph_id, &file_offset, &subgraph_cal_table_header);
    if (AR_FAILED(status))
    {
        return status;
    }

    /* Set the starting offset of the VCPM blob and calculate chunk sizes
     * and offsets
     */
    vcpm_info.offset_vcpm_blob = blob_offset;
    vcpm_info.subgraph_id = subgraph_id;
    status = GetVcpmSubgraphChunkInfo(
		&vcpm_info, file_offset, &subgraph_cal_table_header, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to calculate vcpm subgraph cal data size");
        return status;
    }

    //Calculate Size of vcpm blob: sizeof(chunk.size) + chunk.size
    fwk_module_header.param_size =
        vcpm_info.chunk_subgraph_info.size                  +
        sizeof(uint32_t) + vcpm_info.chunk_master_key.size  +
        sizeof(uint32_t) + vcpm_info.chunk_cal_key_id.size  +
        sizeof(uint32_t) + vcpm_info.chunk_cal_key_lut.size +
        sizeof(uint32_t) + vcpm_info.chunk_data_pool.size;

    padded_param_size = ACDB_ALIGN_8_BYTE(fwk_module_header.param_size);
    padding = padded_param_size - fwk_module_header.param_size;

    rsp->buf_size += padded_param_size;
    rsp->buf_size += sizeof(AcdbDspModuleHeader);

    if (IsNull(rsp->buf))
        return status;

    status = GetVcpmSubgraphChunkData(&vcpm_info, file_offset,
		&subgraph_cal_table_header, rsp, tot_cal_size);
    if (AR_FAILED(status))
    {
        return status;
    }

    //Write VCPM Framework Module Header
    fwk_module_header.module_iid = subgraph_cal_header.module_iid;
    fwk_module_header.parameter_id = subgraph_cal_header.parameter_id;
    if (!IsNull(rsp->buf))
    {
		if (tot_cal_size >= (vcpm_info.offset_vcpm_blob + sizeof(AcdbDspModuleHeader)))
		{
        ACDB_MEM_CPY(rsp->buf + vcpm_info.offset_vcpm_blob,
            &fwk_module_header, sizeof(AcdbDspModuleHeader));
		}
		else
		{
			return AR_ENEEDMORE;
		}
        vcpm_blob_offset += vcpm_info.offset_vcpm_blob +
            sizeof(AcdbDspModuleHeader) + fwk_module_header.param_size;

        //Padding
        memset((uint8_t*)rsp->buf + vcpm_blob_offset,
            0, padding);
    }

    return status;
}

int32_t AcdbCmdGetSubgraphCalDataPersist(AcdbSgIdCalKeyVector *req, AcdbSgIdPersistCalData *rsp, uint32_t rsp_struct_size)
{
    int32_t status = AR_EOK;
    bool_t found_sg = FALSE;
    bool_t has_persist_cal = FALSE;
    uint32_t offset = 0;
    uint32_t sg_cal_lut_offset = 0;
    uint32_t default_ckv_entry_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t sg_persist_header_offset = 0;
    uint32_t sg_persist_cal_offset = 0;
    uint32_t num_subgraph = 0;
    uint32_t num_subgraph_found = 0;
    ChunkInfo ci_sg_cal_lut = { 0 };
    AcdbSgCalLutHeader sg_cal_lut_header = { 0 };
    AcdbCalKeyTblEntry ckv_entry = { 0 };
    AcdbAudioCalContextInfo info =
    {
        ACDB_OP_NONE,
        ACDB_OP_GET_SUBGRAPH_DATA,
        FALSE, FALSE, FALSE,
        { 0 }, 0, 0,
        NULL, NULL, NULL, NULL,
        ACDB_DATA_UNKNOWN,
        { 0 }
    };
    AcdbBlob vcpm_blob = { 0 };
    AcdbBlob audio_blob = { 0 };
    AcdbSgIdPersistData sg_persist_data = { 0 };

    if (rsp_struct_size < sizeof(AcdbSgIdPersistCalData))
    {
        ACDB_ERR("Error[%d]: The response structure size is less than %d",
            AR_EBADPARAM, sizeof(AcdbSgIdPersistCalData));
        return AR_EBADPARAM;
    }

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " req(%p) rsp(%p)", req, rsp);
        return AR_EBADPARAM;
    }

    ci_sg_cal_lut.chunk_id = ACDB_CHUNKID_CALSGLUT;
    status = ACDB_GET_CHUNK_INFO(&ci_sg_cal_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration LUT "
            "chunk information", status);
        return status;
    }

    info.param_type = ACDB_DATA_PERSISTENT;
    if (rsp->cal_data_size > 0 && !IsNull(rsp->cal_data))
    {
        info.op = ACDB_OP_GET_DATA;
        memset(rsp->cal_data, 0, rsp->cal_data_size);
    }
    else
        info.op = ACDB_OP_GET_SIZE;

    if (info.op == ACDB_OP_GET_SIZE)
    {
        //Log CKV when this API is called for the first time
        ACDB_DBG("Retrieving persistent calibration for "
            "the following cal key vector:");
        ACDB_DBG("CKV[new]:");
        LogKeyVector(&req->cal_key_vector_new, CAL_KEY_VECTOR);
        ACDB_DBG("CKV[old]:");
        LogKeyVector(&req->cal_key_vector_prior, CAL_KEY_VECTOR);
    }

    /* CKV Setup
    * 1. GLB_BUF_2 stores the module CKV that will be used to locate data in
    * the heap(if it exists)
    * 2. The Delta CKV and CKV[New] are allocated on the heap by
    * AcdbComputeDeltaCKV
    *
    * tmp - stores the amount of padding after graph_key_vectornum_keys
    * tmp2 - stores the position of the KeyValuePair list
    *
    * The first info.module_ckv->graph_key_vector assignment has the effect
    * of storing the address of the pointer in glb_buf_2 after
    * graph_key_vector.num_keys
    *
    * The second info.module_ckv->graph_key_vector assignment has the effect
    * of pointer the info.module_ckv->graph_key_vector to the actual list
    * of key value pairs
    *
    * Whats inside global buffer 2 after these operations?
    *
    * The first 'sizeof(AcdbGraphKeyVector)' bytes is AcdbGraphKeyVector
    * followed by 'sizeof(AcdbKeyValuePair) * num_keys' bytes which is
    * the KV list
    */
    info.module_ckv = (AcdbGraphKeyVector*)&glb_buf_2[0];
    uint32_t tmp = (sizeof(AcdbGraphKeyVector) - sizeof(AcdbKeyValuePair *)) / sizeof(uint32_t);
    uint32_t tmp2 = sizeof(AcdbGraphKeyVector) / sizeof(uint32_t);
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp];
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp2];

    status = AcdbComputeDeltaCKV(req, &info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get the delta CKV", status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    offset = ci_sg_cal_lut.chunk_offset;
    status = FileManReadBuffer(&num_subgraph,
        sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of Subgraphs",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    /* Sort the subgraph list so that the linear search for subgraph
    * takes less time. The Subgraph CAL LUT sorts the subgraphs. By
    * sorting the input subgraph list we can locate the subgraphs
    * in the chunk sequentially. Therefore there is no need to reset
    * the table offset */
    status = AcdbSort2(req->num_sg_ids * sizeof(uint32_t),
        req->sg_ids, sizeof(uint32_t), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort subgraph list",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        found_sg = FALSE;
        audio_blob.buf_size = rsp->cal_data_size;
        audio_blob.buf = (void*)rsp->cal_data;
        has_persist_cal = FALSE;
        //offset = sizeof(uint32_t);

        //Get VCPM Subgraph Data
        if (info.is_first_time)
        {
            int32_t vcpm_status = AR_EOK;
            //uint32_t vcpm_blob_offset = rsp->cal_data_size +
            //    sizeof(AcdbSgIdPersistData);
            uint32_t vcpm_blob_offset = 0;

            if (info.op == ACDB_OP_GET_SIZE)
                rsp->cal_data_size += sizeof(AcdbSgIdPersistData);
            else if (info.op == ACDB_OP_GET_DATA)
            {
                vcpm_blob_offset = blob_offset;
                blob_offset += sizeof(AcdbSgIdPersistData);
                sg_persist_cal_offset = blob_offset;
            }

            vcpm_blob.buf_size = 0;
            vcpm_blob.buf = (void*)rsp->cal_data;
            vcpm_status = BuildVcpmBlob(req->sg_ids[i],
				sg_persist_cal_offset, &vcpm_blob, rsp->cal_data_size);
            if (AR_SUCCEEDED(vcpm_status))
            {
                if (info.op == ACDB_OP_GET_SIZE)
                    rsp->cal_data_size += vcpm_blob.buf_size;
                else if (info.op == ACDB_OP_GET_DATA)
                {
					if (rsp->cal_data_size >= (vcpm_blob_offset + sizeof(uint32_t)))
					{
                    ACDB_MEM_CPY(rsp->cal_data + vcpm_blob_offset,
                        &req->sg_ids[i], sizeof(uint32_t));
					}
					else
					{
						return AR_ENEEDMORE;
					}
                    vcpm_blob_offset += sizeof(uint32_t);

                    //Place holder to come back and write size at this offset
                    //persist_data_offset = blob_offset;//todo: figure out what this was for and add it back

					if (rsp->cal_data_size >= (vcpm_blob_offset + sizeof(vcpm_blob.buf_size)))
					{
                    ACDB_MEM_CPY(rsp->cal_data + vcpm_blob_offset,
                        &vcpm_blob.buf_size, sizeof(vcpm_blob.buf_size));
					}
					else
					{
						return AR_ENEEDMORE;
					}
                    vcpm_blob_offset += sizeof(vcpm_blob.buf_size);

                    blob_offset += vcpm_blob.buf_size;
                }

                num_subgraph_found++;
                continue;
            }

            //If there is any other error other than ENOTEXIST return failure
            if (AR_FAILED(vcpm_status) && vcpm_status != AR_ENOTEXIST)
            {
                return vcpm_status;
            }
            else
            {
                if (info.op == ACDB_OP_GET_SIZE)
                    rsp->cal_data_size -= sizeof(AcdbSgIdPersistData);
                else if (info.op == ACDB_OP_GET_DATA)
                    blob_offset -= sizeof(AcdbSgIdPersistData);
            }
        }


        for (uint32_t j = 0; j < num_subgraph; j++)
        {
            status = FileManReadBuffer(&sg_cal_lut_header,
                sizeof(AcdbSgCalLutHeader), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read Subgraph "
                    "Cal LUT header", status);
            }

            if (req->sg_ids[i] == sg_cal_lut_header.subgraph_id)
            {
                found_sg = TRUE;
                break;
            }
            else
            {
                //Go to next Subgraph entry
                offset += (sg_cal_lut_header.num_ckv_entries
                    * sizeof(AcdbCalKeyTblEntry));
            }
        }

        if (!found_sg)
        {
            ACDB_ERR("Error[%d]: Subgraph(%x) does not exist. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            continue;
        }

        if (sg_cal_lut_header.num_ckv_entries == 0)
        {
            ACDB_ERR("Error[%d]: Subgraph(%x) contains zero CKV entries in"
                " the Subgraph Cal LUT. At least one entry should be"
                " present. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            continue;
        }

        /* Save starting offset of a subgraphs persist cal. Move the blob
         * offset to start at the calibration. If there is no calibration
         * move the offset back by subtracting the size of the header and
         * skip the subgraph.
         */
        if (info.op == ACDB_OP_GET_SIZE)
            rsp->cal_data_size += sizeof(AcdbSgIdPersistData);
        else if (info.op == ACDB_OP_GET_DATA)
        {
            sg_persist_header_offset = blob_offset;
            blob_offset += sizeof(AcdbSgIdPersistData);
            sg_persist_cal_offset = blob_offset;
        }

        info.ignore_get_default_data =
            sg_cal_lut_header.num_ckv_entries == 1 ? FALSE : TRUE;

        //The default CKV will be skipped and handeled last
        if (sg_cal_lut_header.num_ckv_entries > 1 && info.is_first_time)
            sg_cal_lut_header.num_ckv_entries++;

        for (uint32_t k = 0; k < sg_cal_lut_header.num_ckv_entries; k++)
        {
            /* The Default CKV is always the first CKV in the list of CKVs to
            * search. It will be skipped and processed last */
            if ((k == sg_cal_lut_header.num_ckv_entries - 1) &&
                info.ignore_get_default_data == TRUE && info.is_first_time)
            {
                sg_cal_lut_offset = default_ckv_entry_offset;
                info.ignore_get_default_data = FALSE;

                status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                    &sg_cal_lut_offset);
            }
            else
            {
                status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry),
                    &offset);
                sg_cal_lut_offset = offset;
            }

            if (AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            info.subgraph_id = sg_cal_lut_header.subgraph_id;

            status = AcdbFindModuleCKV(&ckv_entry, &info);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                continue;
            }
            else if (AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            if (info.is_default_module_ckv)
            {
                default_ckv_entry_offset =
                    sg_cal_lut_offset - sizeof(ckv_entry);
            }

            //Get Calibration
            status = AcdbGetSubgraphCalibration(
                &info, &blob_offset, &audio_blob);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                //No calibration found for subgraph with module ckv_entry
                //has_persist_cal = FALSE;
                continue;
            }
            else if (AR_FAILED(status) && status != AR_ENOTEXIST)
            {
                ACDB_ERR("Error[%d]: An error occured while getting "
                    "calibration data for Subgraph(%x)", status,
                    info.subgraph_id);
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            if (info.is_default_module_ckv && info.ignore_get_default_data)
            {
                continue;
            }

            has_persist_cal = TRUE;
        }

        if (AR_FAILED(status) && !has_persist_cal)
        {
            if (info.op == ACDB_OP_GET_SIZE)
                rsp->cal_data_size -= sizeof(AcdbSgIdPersistData);
            else if (info.op == ACDB_OP_GET_DATA)
                blob_offset -= sizeof(AcdbSgIdPersistData);

            ACDB_ERR("Error[%d]: Unable to retrieve calibration data for "
                "Subgraph(%x). Skipping..", status,
                info.subgraph_id);
            status = AR_EOK;
        }
        else
        {
            status = AR_EOK;
            num_subgraph_found++;

            if (info.op == ACDB_OP_GET_SIZE)
                rsp->cal_data_size += audio_blob.buf_size;
            else if (info.op == ACDB_OP_GET_DATA)
            {
                //Write subgraph persist data header
                sg_persist_data.sg_id = info.subgraph_id;
                //sg_persist_data.persist_data_size = audio_blob.buf_size;
                sg_persist_data.persist_data_size =
                    blob_offset - sg_persist_cal_offset;

				if (rsp->cal_data_size >= (sg_persist_header_offset + sizeof(AcdbSgIdPersistData)))
				{
                ACDB_MEM_CPY_SAFE(rsp->cal_data + sg_persist_header_offset,
                    sizeof(AcdbSgIdPersistData),
                    &sg_persist_data, sizeof(AcdbSgIdPersistData));
            }
				else
				{
					return AR_ENEEDMORE;
				}
			}
        }
    }

    rsp->num_sg_ids = num_subgraph_found;

    if (num_subgraph_found == 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: No calibration found", status);
    }

    //Clean Up Context Info
    AcdbClearAudioCalContextInfo(&info);

    return status;
}

/* TODO: This api needs to be supported when the ACDB file supports global
* persistent data. This API also needs to be refactored and follow the
* approach used in AcdbCmdGetSubgraphCalNonPersist
*/
int32_t AcdbCmdGetSubgraphGlbPersistIds(AcdbSgIdCalKeyVector *pInput, AcdbGlbPsistIdentifierList *pOutput, uint32_t rsp_struct_size)
{
    __UNREFERENCED_PARAM(pInput);
    __UNREFERENCED_PARAM(pOutput);
    __UNREFERENCED_PARAM(rsp_struct_size);
    return AR_ENOTEXIST;
}

/* TODO: This api needs to be supported when the ACDB file supports global
 * persistent data. This API also needs to be refactored and follow the
 * approach used in AcdbCmdGetSubgraphCalNonPersist
 */
//int32_t AcdbCmdGetSubgraphGlbPersistIds(AcdbSgIdCalKeyVector *pInput, AcdbGlbPsistIdentifierList *pOutput, uint32_t rsp_struct_size)
//{
//	int32_t result = AR_EOK;
//	uint32_t entryCnt = 0;
//	uint32_t chkBuf = 0;
//	uint32_t chkLen = 0;
//	uint32_t sgLutChkBuf = 0;
//	uint32_t sgLutChkLen = 0;
//	uint32_t calKeyChkBuf = 0;
//	uint32_t calKeyChkLen = 0;
//	uint32_t calLutChkBuf = 0;
//	uint32_t calLutChkLen = 0;
//	uint32_t calDef2ChkBuf = 0;
//	uint32_t calDef2ChkLen = 0;
//	uint32_t pidMapChkBuf = 0;
//	uint32_t pidMapChkLen = 0;
//	uint32_t offset = 0;
//	uint32_t data_offset = 0;
//	size_t totalsize = 0;
//	uint32_t calKeyTblOffset = 0;
//	uint32_t calDataTblOffset = 0;
//	uint32_t calDataDefOffset = 0;
//	bool_t found = FALSE;
//	bool_t key_found = FALSE;
//	bool_t first_time = FALSE;
//	uint32_t* iid_list = NULL;
//
//	ACDB_PKT_LOG_DATA("NumSGIDs", &pInput->num_sg_ids, sizeof(pInput->num_sg_ids));
//	ACDB_PKT_LOG_DATA("SGIDs", pInput->sg_ids, sizeof(pInput->sg_ids) * pInput->num_sg_ids);
//	ACDB_PKT_LOG_DATA("CKVPrior_NumKeys", &pInput->cal_key_vector_prior.num_keys, sizeof(pInput->cal_key_vector_prior.num_keys));
//	if (pInput->cal_key_vector_prior.num_keys != 0)
//		ACDB_PKT_LOG_DATA("CKVPrior_KV", pInput->cal_key_vector_prior.graph_key_vector, sizeof(pInput->cal_key_vector_prior.graph_key_vector) * pInput->cal_key_vector_prior.num_keys);
//	ACDB_PKT_LOG_DATA("CKVNew_NumKeys", &pInput->cal_key_vector_new.num_keys, sizeof(pInput->cal_key_vector_new.num_keys));
//	if (pInput->cal_key_vector_new.num_keys != 0)
//		ACDB_PKT_LOG_DATA("CKVNew_KV", pInput->cal_key_vector_new.graph_key_vector, sizeof(pInput->cal_key_vector_new.graph_key_vector) * pInput->cal_key_vector_new.num_keys);
//
//	if (rsp_struct_size < 12)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, rsp_struct_size is invalid");
//		return AR_EBADPARAM;
//	}
//
//	if (pInput->cal_key_vector_prior.num_keys == 0)
//		first_time = TRUE;
//
//	AcdbGraphKeyVector* deltaKV = (AcdbGraphKeyVector*)malloc(sizeof(AcdbGraphKeyVector));
//	if (deltaKV == NULL)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, Insufficient memory to allocate deltaKV");
//		return AR_ENOMEMORY;
//	}
//
//	if (pInput->cal_key_vector_prior.num_keys == 0)
//	{
//		first_time = TRUE;
//		deltaKV->num_keys = pInput->cal_key_vector_new.num_keys;
//		deltaKV->graph_key_vector = (AcdbKeyValuePair*)malloc(2 * sizeof(uint32_t) * deltaKV->num_keys);
//		if (IsNull(deltaKV->graph_key_vector))
//		{
//			ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, Insufficient memory to allocate deltaKV->graph_key_vector");
//			return AR_ENOMEMORY;
//		}
//
//		for (uint32_t i = 0; i < deltaKV->num_keys; i++)
//		{
//			deltaKV->graph_key_vector[i].key = pInput->cal_key_vector_new.graph_key_vector[i].key;
//			deltaKV->graph_key_vector[i].value = pInput->cal_key_vector_new.graph_key_vector[i].value;
//		}
//	}
//	else
//	{
//		//compute delta KV (from prior and new) and number of keys
//		if (pInput->cal_key_vector_prior.num_keys != pInput->cal_key_vector_new.num_keys)
//		{
//			ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, calkeyvectorprior - num of keys do not match calkeyvectornew - num of keys");
//			return AR_EFAILED;
//		}
//		uint32_t cnt = 0;
//		for (uint32_t i = 0; i < pInput->cal_key_vector_prior.num_keys; i++)
//		{
//			if (pInput->cal_key_vector_prior.graph_key_vector[i].value != pInput->cal_key_vector_new.graph_key_vector[i].value)
//				cnt++;
//		}
//		deltaKV->num_keys = cnt;
//		deltaKV->graph_key_vector = (AcdbKeyValuePair*)malloc(2 * sizeof(uint32_t) * cnt);
//		if (IsNull(deltaKV->graph_key_vector))
//		{
//			ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, Insufficient memory to allocate deltaKV->graph_key_vector");
//			return AR_ENOMEMORY;
//		}
//
//		cnt = 0;
//		for (uint32_t i = 0; i < pInput->cal_key_vector_prior.num_keys; i++)
//		{
//			if (pInput->cal_key_vector_prior.graph_key_vector[i].value != pInput->cal_key_vector_new.graph_key_vector[i].value)
//			{
//				deltaKV->graph_key_vector[cnt].key = pInput->cal_key_vector_new.graph_key_vector[i].key;
//				deltaKV->graph_key_vector[cnt].value = pInput->cal_key_vector_new.graph_key_vector[i].value;
//				cnt++;
//			}
//		}
//	}
//
//	ACDB_PKT_LOG_DATA("deltaNumKeys", &deltaKV->num_keys, sizeof(deltaKV->num_keys));
//	ACDB_PKT_LOG_DATA("deltaKV", &deltaKV->graph_key_vector, sizeof(deltaKV->graph_key_vector) * deltaKV->num_keys);
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_CALSGLUT, &sgLutChkBuf, &sgLutChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_CALKEYTBL, &calKeyChkBuf, &calKeyChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_CALDATALUT, &calLutChkBuf, &calLutChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_CALDATADEF, &calDef2ChkBuf, &calDef2ChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_PIDPERSIST, &pidMapChkBuf, &pidMapChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_DATAPOOL, &chkBuf, &chkLen);
//	if (result != AR_EOK)
//		return result;
//
//	for (uint32_t i = 0;i < pInput->num_sg_ids;i++)
//	{
//		uint32_t sgID = pInput->sg_ids[i];
//		offset = sizeof(uint32_t); //ignore num of SGID's in ACDB_CHUNKID_CALSGLUT chunk
//		found = FALSE;
//		do
//		{
//			memset(&glb_buf_1, 0, sizeof(glb_buf_1));
//			int ret = file_seek(sgLutChkBuf + offset, AR_FSEEK_BEGIN);
//			if (ret != 0)
//			{
//				ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", sgLutChkBuf + offset);
//				return AR_EFAILED;
//			}
//			ret = file_read(&glb_buf_1, 2 * sizeof(uint32_t));
//			if (ret != 0)
//			{
//				ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, sgID read was unsuccessful");
//				return AR_EFAILED;
//			}
//
//			if (memcmp(&sgID, &glb_buf_1[0], sizeof(sgID)) == 0)
//			{
//				found = TRUE;
//				uint32_t num_cktbl_entries = glb_buf_1[1];
//				offset = offset + (2 * sizeof(uint32_t));
//				for (uint32_t j = 0; j < num_cktbl_entries;j++)
//				{
//					memset(&glb_buf_1, 0, sizeof(glb_buf_1));
//					ret = file_seek(sgLutChkBuf + offset, AR_FSEEK_BEGIN);
//					if (ret != 0)
//					{
//						ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", sgLutChkBuf + offset);
//						return AR_EFAILED;
//					}
//					ret = file_read(&glb_buf_1, 2 * sizeof(uint32_t));
//					if (ret != 0)
//					{
//						ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, calKeyTblOffset read was unsuccessful");
//						return AR_EFAILED;
//					}
//
//					offset = offset + (2 * sizeof(uint32_t));
//					calKeyTblOffset = glb_buf_1[0];
//					calDataTblOffset = glb_buf_1[1];
//
//					ACDB_PKT_LOG_DATA("calKeyTblOffset", &calKeyTblOffset, sizeof(calKeyTblOffset));
//					ACDB_PKT_LOG_DATA("calDataTblOffset", &calDataTblOffset, sizeof(calDataTblOffset));
//
//					if (calKeyTblOffset == 0xffffffff && !first_time)
//						continue;
//					else
//					{
//						if (calKeyTblOffset != 0xffffffff)
//						{
//							uint32_t num_keys = 0;
//							memset(&glb_buf_1, 0, sizeof(glb_buf_1));
//							ret = file_seek(calKeyChkBuf + calKeyTblOffset, AR_FSEEK_BEGIN);
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calKeyChkBuf + calKeyTblOffset);
//								return AR_EFAILED;
//							}
//							ret = file_read(&num_keys, sizeof(uint32_t));
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, num of keys read was unsuccessful");
//								return AR_EFAILED;
//							}
//
//							ret = file_seek(calKeyChkBuf + calKeyTblOffset + sizeof(num_keys), AR_FSEEK_BEGIN);
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calKeyChkBuf + calKeyTblOffset + sizeof(num_keys));
//								return AR_EFAILED;
//							}
//
//							if (num_keys > 0)
//							{
//								ret = file_read(&glb_buf_1, num_keys * sizeof(num_keys));
//								if (ret != 0)
//								{
//									ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, keys read was unsuccessful");
//									return AR_EFAILED;
//								}
//							}
//
//							//perform SG key validation with CKV->keys provided, if SG key doesn't exist in CKV, flag an error, since the input is invalid
//							for (uint32_t k = 0; k < num_keys; k++)
//							{
//								key_found = FALSE;
//								for (uint32_t l = 0; l < deltaKV->num_keys; l++)
//								{
//									if (glb_buf_1[k] == deltaKV->graph_key_vector[l].key)
//									{
//										key_found = TRUE;
//										break;
//									}
//								}
//								if (!key_found)
//								{
//									ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, key doesn't exist %d", glb_buf_1[i]);
//									return AR_EFAILED;
//								}
//
//							}
//						}
//
//						uint32_t num_entries = 0;
//						uint32_t num_values = 0;
//						memset(&glb_buf_2, 0, sizeof(glb_buf_2));
//						ret = file_seek(calLutChkBuf + calDataTblOffset, AR_FSEEK_BEGIN);
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calLutChkBuf + calDataTblOffset);
//							return AR_EFAILED;
//						}
//						ret = file_read(&num_entries, sizeof(uint32_t));
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, num of entries read was unsuccessful");
//							return AR_EFAILED;
//						}
//
//						ret = file_seek(calLutChkBuf + calDataTblOffset + sizeof(num_entries), AR_FSEEK_BEGIN);
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calLutChkBuf + calDataTblOffset + sizeof(num_entries));
//							return AR_EFAILED;
//						}
//						ret = file_read(&num_values, sizeof(uint32_t));
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, num of values read was unsuccessful");
//							return AR_EFAILED;
//						}
//
//						ret = file_seek(calLutChkBuf + calDataTblOffset + sizeof(num_entries) + sizeof(num_values), AR_FSEEK_BEGIN);
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calLutChkBuf + calDataTblOffset + sizeof(num_entries) + sizeof(num_values));
//							return AR_EFAILED;
//						}
//						ret = file_read(&glb_buf_2, num_entries * ((num_values * sizeof(uint32_t)) + (3 * sizeof(uint32_t))));
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, values read was unsuccessful");
//							return AR_EFAILED;
//						}
//
//						ACDB_PKT_LOG_DATA("NumEntries", &num_entries, sizeof(num_entries));
//						ACDB_PKT_LOG_DATA("NumValues", &num_values, sizeof(num_values));
//						ACDB_PKT_LOG_DATA("Values", &glb_buf_2, num_entries * ((num_values * sizeof(uint32_t)) + (3 * sizeof(uint32_t))));
//
//						uint32_t temp_offset = 0;
//						uint32_t marked_offset = 0;
//
//						for (uint32_t k = 0; k < num_entries; k++, temp_offset += 4)
//						{
//							for (uint32_t m = 0;m < num_values; m++, temp_offset++)
//							{
//								key_found = FALSE;
//								for (uint32_t n = 0;n < deltaKV->num_keys;n++)
//								{
//									if (glb_buf_1[m] == deltaKV->graph_key_vector[n].key && glb_buf_2[temp_offset] == deltaKV->graph_key_vector[n].value)
//									{
//										marked_offset = temp_offset;
//										key_found = TRUE;
//									}
//								}
//							}
//						}
//						if (!key_found && !first_time)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, value doesn't exist");
//							return AR_EFAILED; //return error is there is one mis-match
//						}
//
//						calDataDefOffset = glb_buf_2[marked_offset + 3]; //DEF2 offset
//
//						ACDB_PKT_LOG_DATA("calDataDefOffset", &calDataDefOffset, sizeof(calDataDefOffset));
//
//						ret = file_seek(calDef2ChkBuf + calDataDefOffset, AR_FSEEK_BEGIN);
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calDef2ChkBuf + calDataDefOffset);
//							return AR_EFAILED;
//						}
//						ret = file_read(&num_entries, sizeof(uint32_t));
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, DEF2 offset read was unsuccessful");
//							return AR_EFAILED;
//						}
//
//						calDataDefOffset += sizeof(uint32_t); //num of entries
//						ret = file_seek(calDef2ChkBuf + calDataDefOffset, AR_FSEEK_BEGIN);
//						if (ret != 0)
//						{
//							ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calDef2ChkBuf + calDataDefOffset);
//							return AR_EFAILED;
//						}
//
//						do
//						{
//
//							uint32_t pid = 0;
//							uint32_t num_of_iids = 0;
//							uint32_t cal_ID = 0;
//							ret = file_read(&cal_ID, sizeof(uint32_t));
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, calId read was unsuccessful");
//								return AR_EFAILED;
//							}
//
//							calDataDefOffset += sizeof(uint32_t); //calID value;
//
//							ret = file_seek(calDef2ChkBuf + calDataDefOffset, AR_FSEEK_BEGIN);
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calDef2ChkBuf + calDataDefOffset);
//								return AR_EFAILED;
//							}
//							ret = file_read(&pid, sizeof(pid)); // pid
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, pid read was unsuccessful");
//								return AR_EFAILED;
//							}
//
//							calDataDefOffset += sizeof(pid); //pid value;
//
//							ret = file_seek(calDef2ChkBuf + calDataDefOffset, AR_FSEEK_BEGIN);
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calDef2ChkBuf + calDataDefOffset);
//								return AR_EFAILED;
//							}
//							ret = file_read(&num_of_iids, sizeof(num_of_iids)); // num of iid's
//							if (ret != 0)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, num of iid's read was unsuccessful");
//								return AR_EFAILED;
//							}
//
//							iid_list = (uint32_t*)malloc(sizeof(uint32_t) * num_of_iids);
//							if (iid_list == NULL)
//							{
//								ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, cannot allocate memory for iid_list");
//								return AR_ENOMEMORY;
//							}
//
//							calDataDefOffset += sizeof(uint32_t); // num of iid's
//							uint32_t iid_offset = 0;
//							for (uint32_t r = 0; r < num_of_iids; r++)
//							{
//								memset(&glb_buf_3, 0, sizeof(glb_buf_3));
//								ret = file_seek(calDef2ChkBuf + calDataDefOffset, AR_FSEEK_BEGIN);
//								if (ret != 0)
//								{
//									ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, file seek to %d was unsuccessful", calDef2ChkBuf + calDataDefOffset);
//									return AR_EFAILED;
//								}
//								ret = file_read(&glb_buf_3, sizeof(uint32_t)); // iid value;
//								if (ret != 0)
//								{
//									ACDB_ERR("AcdbCmdGetSubgraphGlbPersistIds() failed, iid value read was unsuccessful");
//									return AR_EFAILED;
//								}
//
//								ACDB_MEM_CPY(&iid_list + iid_offset, &glb_buf_3, sizeof(uint32_t));
//								iid_offset += sizeof(uint32_t);
//								calDataDefOffset += sizeof(uint32_t);
//							}
//
//							totalsize += sizeof(cal_ID) + sizeof(num_of_iids);
//							totalsize += sizeof(iid_list);
//
//							if (pOutput->global_persistent_cal_info != NULL)
//							{
//								if (pOutput->size >= (data_offset + (2 * sizeof(uint32_t))))
//								{
//								ACDB_MEM_CPY(pOutput->global_persistent_cal_info + data_offset, &cal_ID, sizeof(cal_ID));
//								data_offset += sizeof(cal_ID);
//								ACDB_MEM_CPY(pOutput->global_persistent_cal_info + data_offset, &num_of_iids, sizeof(num_of_iids));
//								data_offset += sizeof(num_of_iids);
//								}
//								else
//								{
//									return AR_ENEEDMORE;
//								}
//								for (uint32_t r = 0; r < num_of_iids; r++)
//								{
//									if (pOutput->size >= (data_offset + sizeof(uint32_t)))
//									{
//									ACDB_MEM_CPY(pOutput->global_persistent_cal_info + data_offset, &iid_list[r], sizeof(uint32_t));
//									}
//									else
//									{
//										return AR_ENEEDMORE;
//									}
//									data_offset += sizeof(uint32_t);
//								}
//							}
//
//							entryCnt++;
//						} while (entryCnt < num_entries);
//
//					}
//
//				}
//			}
//			else
//			{
//				offset = offset + (2 * sizeof(uint32_t) + (2 * sizeof(uint32_t) * glb_buf_1[1]));
//			}
//		} while (offset < sgLutChkLen && !found);
//	}
//	if (iid_list != NULL)
//		free(iid_list);
//
//	pOutput->num_glb_persist_identifiers = entryCnt;
//	pOutput->size = (uint32_t)totalsize;
//
//	ACDB_PKT_LOG_DATA("RspBufSize", &pOutput->size, sizeof(pOutput->size));
//	ACDB_PKT_LOG_DATA("RspBufNumGlbIds", &pOutput->num_glb_persist_identifiers, sizeof(pOutput->num_glb_persist_identifiers));
//	if (pOutput->global_persistent_cal_info != NULL)
//	{
//		ACDB_PKT_LOG_DATA("RspBuf", pOutput->global_persistent_cal_info, totalsize);
//	}
//
//	return result;
//}

/* TODO: This api needs to be supported when the ACDB file supports global
* persistent data. This API also needs to be refactored and follow the
* approach used in AcdbCmdGetSubgraphCalNonPersist
*/
int32_t AcdbCmdGetSubgraphGlbPersistCalData(AcdbGlbPersistCalDataCmdType *pInput, AcdbBlob *pOutput, uint32_t rsp_struct_size)
{
    __UNREFERENCED_PARAM(pInput);
    __UNREFERENCED_PARAM(pOutput);
    __UNREFERENCED_PARAM(rsp_struct_size);
    return AR_ENOTEXIST;
}

/*TODO: This api needs to be supported when the ACDB file supports global persistent data. This API also needs to be refactored */
//int32_t AcdbCmdGetSubgraphGlbPersistCalData(AcdbGlbPersistCalDataCmdType *pInput, AcdbBlob *pOutput, uint32_t rsp_struct_size)
//{
//	int32_t result = AR_EOK;
//	uint32_t chkBuf = 0;
//	uint32_t chkLen = 0;
//	uint32_t pidMapChkBuf = 0;
//	uint32_t pidMapChkLen = 0;
//	uint32_t pid_offset = 0;
//	uint32_t calBlobSize = 0;
//	size_t totalsize = 0;
//	uint32_t data_offset = 0;
//	bool_t cal_id_found = FALSE;
//	uint32_t pid = 0;
//
//	ACDB_PKT_LOG_DATA("CalID", &pInput->cal_Id, sizeof(pInput->cal_Id));
//
//	if (rsp_struct_size < 8)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, rsp_struct_size is invalid");
//		return AR_EFAILED;
//	}
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_PIDPERSIST, &pidMapChkBuf, &pidMapChkLen);
//	if (result != AR_EOK)
//		return result;
//
//	result = AcdbGetChunkInfo(ACDB_CHUNKID_DATAPOOL, &chkBuf, &chkLen);
//	if (result != AR_EOK)
//		return result;
//
//	uint32_t calID = pInput->cal_Id;
//	pid_offset += (2 * sizeof(uint32_t));//size and num of calId's
//	do
//	{
//		memset(&glb_buf_1, 0, sizeof(glb_buf_1));
//		int ret = file_seek(pidMapChkBuf + pid_offset, AR_FSEEK_BEGIN);
//		if (ret != 0)
//		{
//			ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, file seek to %d was unsuccessful", pidMapChkBuf + pid_offset);
//			return AR_EFAILED;
//		}
//		ret = file_read(&glb_buf_1, 3 * sizeof(uint32_t));
//		if (ret != 0)
//		{
//			ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, calID read was unsuccessful");
//			return AR_EFAILED;
//		}
//
//		if (calID == glb_buf_1[0])
//		{
//			pid = glb_buf_1[1];
//			data_offset = glb_buf_1[2];
//			cal_id_found = TRUE;
//			break;
//		}
//		else
//		{
//			pid_offset += ((4 * sizeof(uint32_t)) + (glb_buf_1[3] * sizeof(uint32_t)));
//		}
//
//	} while (pid_offset < pidMapChkLen && !cal_id_found);
//	if (!cal_id_found)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, cal_ID %d is not found", calID);
//		return AR_EFAILED;
//	}
//
//	ACDB_PKT_LOG_DATA("DataOffset", &data_offset, sizeof(data_offset));
//
//	//read the payload for each cal_identifier
//	uint32_t param_size = 0, new_param_size = 0, errcode = 0, iid_val = 0;
//	uint64_t zero_val_payload = 0;
//	int ret = file_seek(chkBuf + data_offset, AR_FSEEK_BEGIN);
//	if (ret != 0)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, file seek to %d was unsuccessful", chkBuf + data_offset);
//		return AR_EFAILED;
//	}
//	ret = file_read(&param_size, sizeof(param_size));
//	if (ret != 0)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, param_size read was unsuccessful");
//		return AR_EFAILED;
//	}
//	ACDB_PKT_LOG_DATA("ParamSize", &param_size, sizeof(param_size));
//	data_offset += sizeof(param_size);
//
//	memset(&glb_buf_1, 0, sizeof(glb_buf_1));
//	ret = file_seek(chkBuf + data_offset, AR_FSEEK_BEGIN);
//	if (ret != 0)
//	{
//		ACDB_ERR("AcdbCmdGetSubgraphGlbPersistCalData() failed, file seek to %d was unsuccessful", chkBuf + data_offset);
//		return AR_EFAILED;
//	}
//
//	//Add paddig if necessary before sending the response back
//	if ((param_size % 8) != 0)
//	{
//		new_param_size = param_size + (8 - ((param_size % 8)));
//	}
//	else
//	{
//		new_param_size = param_size;
//	}
//
//	totalsize += sizeof(iid_val) + sizeof(pid) + sizeof(errcode) + sizeof(new_param_size);
//	totalsize += new_param_size;
//
//	if (pOutput->buf != NULL)
//	{
//		if (pOutput->buf_size >= (calBlobSize + (4 * sizeof(uint32_t))))
//		{
//		ACDB_MEM_CPY(pOutput->buf + calBlobSize, &iid_val, sizeof(iid_val));
//		calBlobSize += sizeof(iid_val);
//		ACDB_MEM_CPY(pOutput->buf + calBlobSize, &pid, sizeof(pid));
//		calBlobSize += sizeof(pid);
//		ACDB_MEM_CPY(pOutput->buf + calBlobSize, &param_size, sizeof(param_size));
//		calBlobSize += sizeof(param_size);
//		ACDB_MEM_CPY(pOutput->buf + calBlobSize, &errcode, sizeof(errcode));
//		calBlobSize += sizeof(errcode);
//		}
//		else
//		{
//			return AR_ENEEDMORE;
//		}
//
//		if (param_size != 0)
//		{
//			if (param_size < sizeof(glb_buf_1))
//			{
//				ret = file_read(&glb_buf_1, param_size);
//				if (ret != 0)
//				{
//					ACDB_ERR_MSG_1("Failed to read parameter payload from database.", ret);
//					ACDB_ERR("IID: %x PID: %x Size: %d", iid_val, pid, param_size);
//					return ret;
//				}
//				if (pOutput->buf_size >= (calBlobSize + param_size))
//				{
//				ACDB_MEM_CPY(pOutput->buf + calBlobSize, &glb_buf_1, param_size);
//			}
//			else
//			{
//					return AR_ENEEDMORE;
//				}
//			}
//			else
//			{
//				//ret = CopyFromFileToBuf(chkBuf + data_offset, (uint8_t*)(pOutput->buf) + calBlobSize, param_size, sizeof(glb_buf_1));
//				if (pOutput->buf_size >= (calBlobSize + param_size))
//				{
//				ret = CopyFromFileToBuf(chkBuf + data_offset, (uint8_t*)(pOutput->buf) + calBlobSize, param_size, param_size);
//				if (AR_EOK != ret)
//				{
//					ACDB_ERR_MSG_1("Failed to read parameter payload from database.", ret);
//					ACDB_ERR("IID: %x PID: %x Size: %d", iid_val, pid, param_size);
//					return ret;
//				}
//			}
//				else
//				{
//					return AR_ENEEDMORE;
//				}
//			}
//		}
//		calBlobSize += param_size;
//
//		if (pOutput->buf_size >= (calBlobSize + new_param_size - param_size))
//		{
//		ACDB_MEM_CPY(pOutput->buf + calBlobSize, &zero_val_payload, new_param_size - param_size);
//		}
//		else
//		{
//			return AR_ENEEDMORE;
//		}
//		calBlobSize += (new_param_size - param_size);
//
//		ACDB_PKT_LOG_DATA("RspBuf", pOutput->buf, totalsize);
//	}
//	pOutput->buf_size = (uint32_t)totalsize;
//	ACDB_PKT_LOG_DATA("RspBufSize", &pOutput->buf_size, sizeof(pOutput->buf_size));
//	return result;
//}

/**
* \brief
*		Sets parameter calibration to the heap and saves to the delta file if
*       delta support is enabled.
*
* \param[in/out] info: Context info containing the module kv
* \param[in/out] req: Contains calibration to be set
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbSetSubgraphCalibration(AcdbAudioCalContextInfo *info,
    AcdbSetCalibrationDataReq *req)
{
    int32_t status = AR_EOK;
    uint32_t cur_def_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t num_param_cal_found = 0;
    ChunkInfo ci_cal_def = { 0 };
    uint32_t num_id_entries = 0;
    AcdbModuleHeader blob_module_header = { 0 };
    AcdbModuleHeader file_module_header = { 0 };
    AcdbSetCalibrationDataReq set_cal_req = { 0 };

    ci_cal_def.chunk_id = ACDB_CHUNKID_CALDATADEF;
    status = ACDB_GET_CHUNK_INFO(&ci_cal_def);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration "
            "DEF chunk information", status);
        return status;
    }

    cur_def_offset = ci_cal_def.chunk_offset + info->data_offsets.offset_def;
    status = FileManReadBuffer(&num_id_entries, sizeof(uint32_t), &cur_def_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of "
            "<iid, pid> entries", status);
        return status;
    }

    /* Parse the input cal blob, find the matching  <IID,PID> pair and set
     * the calibration to the heap */
    for (uint32_t i = 0; i < num_id_entries; i++)
    {
        status = FileManReadBuffer(&file_module_header, sizeof(AcdbMiidPidPair),
            &cur_def_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read number of "
                "<iid, pid> entries", status);
            return status;
        }

        blob_offset = 0;
        do
        {
            ACDB_MEM_CPY_SAFE(&blob_module_header, sizeof(AcdbModuleHeader),
                (uint8_t*)req->cal_blob + blob_offset,
                sizeof(AcdbModuleHeader));

            if (0 == memcmp(&file_module_header, &blob_module_header,
                sizeof(AcdbMiidPidPair)))
            {
                //set calibration to heap
                set_cal_req.cal_key_vector.num_keys =
                    info->module_ckv->num_keys;
                set_cal_req.cal_key_vector.graph_key_vector =
                    info->module_ckv->graph_key_vector;
                set_cal_req.cal_blob_size = sizeof(AcdbModuleHeader) +
                    blob_module_header.param_size;
                set_cal_req.cal_blob = (uint8_t*)req->cal_blob + blob_offset;
                set_cal_req.num_sg_ids = 1;
                /* Use glb_buf_3 to resolve Warning C4366 caused by:
                 * (uint32_t*)&info->subgraph_id */
                glb_buf_3[0] = info->subgraph_id;
                set_cal_req.sg_ids = glb_buf_3;

                status = DataProcSetCalData(&set_cal_req);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Failed to set calibration for "
                        "Subgraph(0x%x) Module(0x%x) Parameter(0x%x)");
                }

                num_param_cal_found++;
                break;
            }

            blob_offset += sizeof(AcdbModuleHeader) +
                blob_module_header.param_size;
        } while (blob_offset < req->cal_blob_size);
    }

    if (num_param_cal_found == 0) return AR_ENOTEXIST;

    return status;
}

int32_t AcdbCmdSetSubgraphCalData(AcdbSetCalibrationDataReq *req)
{
    int32_t status = AR_EOK;
    bool_t found_sg = FALSE;
    uint32_t offset = 0;
    uint32_t num_subgraph = 0;
    //Number of subgraphs that data was found for
    uint32_t num_subgraph_found = 0;
    ChunkInfo ci_sg_cal_lut = { 0 };
    AcdbSgCalLutHeader sg_cal_lut_header = { 0 };
    AcdbCalKeyTblEntry ckv_entry = { 0 };
    AcdbAudioCalContextInfo info =
    {
        ACDB_OP_NONE,
        ACDB_OP_GET_SUBGRAPH_DATA,
        FALSE, FALSE, FALSE,
        { 0 }, 0, 0,
        NULL, NULL, NULL, NULL,
        ACDB_DATA_UNKNOWN,
        { 0 }
    };
    uint32_t is_persistance_supported = FALSE;

    if (IsNull(req))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " req or rsp");
        return AR_EBADPARAM;
    }

    if (req->cal_blob_size == 0 || IsNull(req->cal_blob))
    {
        ACDB_ERR("Error[%d]: The input blob is empty. There are no parameters to set.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_sg_cal_lut.chunk_id = ACDB_CHUNKID_CALSGLUT;
    status = ACDB_GET_CHUNK_INFO(&ci_sg_cal_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration LUT "
            "chunk information", status);
        return status;
    }

    /* CKV Setup
    * 1. GLB_BUF_2 stores the module CKV that will be used to locate data in
    * the heap(if it exists)
    * 2. The Delta CKV and CKV[New] are allocated on the heap by
    * AcdbComputeDeltaCKV
    *
    * tmp - stores the amount of padding after graph_key_vectornum_keys
    * tmp2 - stores the position of the KeyValuePair list
    *
    * The first info.module_ckv->graph_key_vector assignment has the effect
    * of storing the address of the pointer in glb_buf_2 after
    * graph_key_vector.num_keys
    *
    * The second info.module_ckv->graph_key_vector assignment has the effect
    * of pointer the info.module_ckv->graph_key_vector to the actual list
    * of key value pairs
    *
    * Whats inside global buffer 2 after these operations?
    *
    * The first 'sizeof(AcdbGraphKeyVector)' bytes is AcdbGraphKeyVector
    * followed by 'sizeof(AcdbKeyValuePair) * num_keys' bytes which is
    * the KV list
    */
    info.module_ckv = (AcdbGraphKeyVector*)&glb_buf_2[0];
    uint32_t tmp = (sizeof(AcdbGraphKeyVector) - sizeof(AcdbKeyValuePair *)) / sizeof(uint32_t);
    uint32_t tmp2 = sizeof(AcdbGraphKeyVector) / sizeof(uint32_t);
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp];
    info.module_ckv->graph_key_vector = (AcdbKeyValuePair*)&glb_buf_2[tmp2];

    info.ckv = &req->cal_key_vector;

    /* For Set Data this flag must be set to true since delta CKVs are
     * not applicable */
    info.is_first_time = TRUE;

    offset = ci_sg_cal_lut.chunk_offset;
    status = FileManReadBuffer(&num_subgraph,
        sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of Subgraphs",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    /* Sort the subgraph list so that the linear search for subgraph
    * takes less time. The Subgraph CAL LUT sorts the subgraphs. By
    * sorting the input subgraph list we can locate the subgraphs
    * in the chunk sequentially. Therefore there is no need to reset
    * the table offset */
    status = AcdbSort2(req->num_sg_ids * sizeof(uint32_t),
        req->sg_ids, sizeof(uint32_t), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort subgraph list",
            status);
        AcdbClearAudioCalContextInfo(&info);
        return status;
    }

    for (uint32_t i = 0; i < req->num_sg_ids; i++)
    {
        found_sg = FALSE;
        //offset = sizeof(uint32_t);

        for (uint32_t j = 0; j < num_subgraph; j++)
        {
            status = FileManReadBuffer(&sg_cal_lut_header,
                sizeof(AcdbSgCalLutHeader), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read Subgraph "
                    "Cal LUT header", status);
            }

            if (req->sg_ids[i] == sg_cal_lut_header.subgraph_id)
            {
                found_sg = TRUE;
                break;
            }
            else
            {
                //Go to next Subgraph entry
                offset += (sg_cal_lut_header.num_ckv_entries
                    * sizeof(AcdbCalKeyTblEntry));
            }
        }

        if (!found_sg)
        {
            ACDB_ERR("Error[%d] Subgraph(%x) does not exist. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            offset = ci_sg_cal_lut.chunk_offset + sizeof(uint32_t);
            continue;
        }

        if (sg_cal_lut_header.num_ckv_entries == 0)
        {
            ACDB_ERR("Error[%d]: Subgraph(%x) contains zero CKV entries in"
                " the Subgraph Cal LUT. At least one entry should be"
                " present. Skipping..",
                AR_ENOTEXIST, req->sg_ids[i]);
            continue;
        }

        for (uint32_t k = 0; k < sg_cal_lut_header.num_ckv_entries; k++)
        {
            status = FileManReadBuffer(&ckv_entry, sizeof(ckv_entry), &offset);
            if (AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            info.subgraph_id = sg_cal_lut_header.subgraph_id;

            status = AcdbFindModuleCKV(&ckv_entry, &info);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                continue;
            }
            else if (AR_FAILED(status))
            {
                AcdbClearAudioCalContextInfo(&info);
                return status;
            }

            //Set Calibration to heap and save to delta file
            status = AcdbSetSubgraphCalibration(&info, req);
            if (AR_FAILED(status) && status == AR_ENOTEXIST)
            {
                continue;
            }
            //if (AR_FAILED(status))
            //{
            //    ACDB_ERR("Error[%d]: An error occured while setting "
            //        "calibration data for Subgraph(%x). Skipping..", status,
            //        sg_cal_lut_header.subgraph_id);
            //    continue;
            //}
        }

        if (AR_FAILED(status))
        {
            status = AR_EOK;
            ACDB_ERR("Error[%d]: Unable to set calibration data for "
                "Subgraph(%x). Skipping..", status,
                sg_cal_lut_header.subgraph_id);
        }
        else
        {
            status = AR_EOK;
            num_subgraph_found++;
        }
    }

    if (num_subgraph_found == 0)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: Unable to set calibration for the "
            "provided subgraphs", status);
    }

    //Save Heap data to Delta File if there is data to save
    status = acdbCmdIsPersistenceSupported(&is_persistance_supported);
    if (AR_SUCCEEDED(status) && is_persistance_supported &&
        num_subgraph_found > 0)
    {
        status = acdb_delta_data_ioctl(ACDB_DELTA_DATA_CMD_SAVE, NULL, 0, NULL);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to save delta file.", status);
        }
    }
    else
    {
        ACDB_ERR("Error[%d]: Unable to save delta data. Is delta data"
            " persistance enabled?", status);
    }

    //Clean Up Context Info
    info.ckv = NULL;
    AcdbClearAudioCalContextInfo(&info);

    return status;
}

int32_t SearchAmdbRegDataChunk(uint32_t *amdb_chk_offset, uint32_t *amdb_chk_size)
{
	int32_t status = AR_EOK;
	ChunkInfo amdb_ci = { ACDB_CHUNKID_AMDB_REG_DATA, 0, 0 };

	status = AcdbGetChunkInfo(amdb_ci.chunk_id, &amdb_ci.chunk_offset, &amdb_ci.chunk_size);
	if (status != AR_EOK)
		return status;

	status = file_seek(amdb_ci.chunk_offset, AR_FSEEK_BEGIN);
	if (status != AR_EOK)
		return status;

	*amdb_chk_offset = amdb_ci.chunk_offset;
	*amdb_chk_size = amdb_ci.chunk_size;

	return status;
}

int32_t SearchBootUpModulesChunk(uint32_t *boot_load_chk_offset, uint32_t *boot_load_chk_size)
{
	int32_t status = AR_EOK;
	ChunkInfo boot_load_ci = { ACDB_CHUNKID_BOOTUP_LOAD_MODULES, 0, 0 };

	status = AcdbGetChunkInfo(boot_load_ci.chunk_id, &boot_load_ci.chunk_offset, &boot_load_ci.chunk_size);
	if (status != AR_EOK)
		return status;

	status = file_seek(boot_load_ci.chunk_offset, AR_FSEEK_BEGIN);
	if (status != AR_EOK)
		return status;

	*boot_load_chk_offset = boot_load_ci.chunk_offset;
	*boot_load_chk_size = boot_load_ci.chunk_size;

	return status;
}

int32_t AcdbCmdGetAmdbRegData(AcdbAmdbProcID *req, AcdbBlob *rsp, uint32_t rsp_size)
{
	int32_t status = AR_EOK;
	uint32_t amdb_reg_chk_offset = 0;
	uint32_t amdb_reg_chk_size = 0;
	uint32_t offset = 0;
	uint32_t data_offset = 0;
	uint32_t mod_size = 0;
    uint32_t num_processors = 0;
	bool_t found = FALSE;
	uint32_t expected_size = rsp->buf_size;
	//Header: <Size, ProcID, #Num of MID's, >
    uint32_t sz_amdb_obj_header = 4 * sizeof(uint32_t);
    uint32_t sz_rsp_amdb_obj_header = 3 * sizeof(uint32_t);

	if (rsp_size < sizeof(AcdbBlob))
	{
		ACDB_ERR("Error[%d]: The response size is less than %d bytes", AR_EBADPARAM, sizeof(AcdbBlob));
		return AR_EBADPARAM;
	}

	status = SearchAmdbRegDataChunk(&amdb_reg_chk_offset, &amdb_reg_chk_size);
	if (AR_EOK != status)
	{
		return status;
	}

    offset += amdb_reg_chk_offset;
    status = FileManReadBuffer(&num_processors, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read processor count", status);
        return status;
    }

    offset = amdb_reg_chk_offset + sizeof(uint32_t);

    for (uint32_t p = 0; p < num_processors; p++)
    {
        memset(&glb_buf_1, 0, sizeof(glb_buf_1));

        status = FileManReadBuffer(&glb_buf_1, sz_amdb_obj_header, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read AMDB header", status);
            return status;
        }

        if (glb_buf_1[1] == req->proc_id)
        {
            rsp->buf_size = glb_buf_1[0];

            if (rsp->buf != NULL)
            {
                if (expected_size >= (data_offset + (2 * sizeof(uint32_t))))
                {
                    ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sz_rsp_amdb_obj_header,
                        &glb_buf_1[1], sz_rsp_amdb_obj_header);
                    data_offset += sz_rsp_amdb_obj_header;
                }
                else
                {
                    return AR_ENEEDMORE;
                }
                for (uint32_t i = 0; i < glb_buf_1[2]; i++)
                {
                    mod_size = 0;

                    status = FileManReadBuffer(&mod_size, sizeof(uint32_t), &offset);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Unable to read AMDB header", status);
                        return status;
                    }

                    memset(&glb_buf_2, 0, sizeof(glb_buf_2));

                    status = FileManReadBuffer(&glb_buf_2, mod_size, &offset);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Unable to read AMDB header", status);
                        return status;
                    }

                    if (expected_size >= (data_offset + mod_size))
                    {
                        ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, mod_size, &glb_buf_2, mod_size);
                    }
                    else
                    {
                        return AR_ENEEDMORE;
                    }
                    data_offset += mod_size;
                }
            }

            found = TRUE;
            break;
        }
        else
        {
            offset += glb_buf_1[0];
        }
    }

	if (!found)
	{
		ACDB_ERR("Error[%d]: Processor ID(%d) does not exist", AR_ENOTEXIST, req->proc_id);
		return AR_ENOTEXIST;
	}

	return status;
}

int32_t AcdbCmdGetAmdbDeRegData(AcdbAmdbProcID *req, AcdbBlob *rsp, uint32_t rsp_size)
{
	int32_t status = AR_EOK;
	uint32_t amdb_reg_chk_offset = 0;
	uint32_t amdb_reg_chk_size = 0;
	uint32_t offset = 0;
	uint32_t data_offset = 0;
    uint32_t num_processors = 0;
	bool_t found = FALSE;
	uint32_t expected_size = rsp->buf_size;
	//Header: <Size, ProcID, #Num of MID's >
	uint32_t sz_amdb_obj_header = 4 * sizeof(uint32_t);

	//Rsp Header : <ProcID, #Num of MID's, Struct Version>
    uint32_t sz_rsp_header = 2 * sizeof(uint32_t);

    if (rsp_size < sizeof(AcdbBlob))
    {
        ACDB_ERR("Error[%d]: The response size is less than %d bytes", AR_EBADPARAM, sizeof(AcdbBlob));
        return AR_EBADPARAM;
    }

	status = SearchAmdbRegDataChunk(&amdb_reg_chk_offset, &amdb_reg_chk_size);
	if (AR_EOK != status)
	{
		return status;
	}

    offset += amdb_reg_chk_offset;
    status = FileManReadBuffer(&num_processors, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read processor count", status);
        return status;
    }

    offset = amdb_reg_chk_offset + sizeof(uint32_t);

    for (uint32_t p = 0; p < num_processors; p++)
    {
        memset(&glb_buf_1, 0, sizeof(glb_buf_1));

        status = FileManReadBuffer(&glb_buf_1, sz_amdb_obj_header, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read AMDB header", status);
            return status;
        }

        if (glb_buf_1[1] == req->proc_id)
        {
            rsp->buf_size = (uint32_t)sz_rsp_header + (glb_buf_1[2] * sizeof(uint32_t));

            if (rsp->buf != NULL)
            {
                if (expected_size >= (data_offset + (2 * sizeof(uint32_t))))
                {
                    ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sz_rsp_header,
                        &glb_buf_1[1], sz_rsp_header);
                    data_offset += sz_rsp_header;
                }
                else
                {
                    return AR_ENEEDMORE;
                }

                for (uint32_t i = 0; i < glb_buf_1[2]; i++)
                {
                    memset(&glb_buf_2, 0, sizeof(glb_buf_2));

                    status = FileManReadBuffer(&glb_buf_2, 4 * sizeof(uint32_t), &offset);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Unable to read size and module ID", status);
                        return status;
                    }

                    if (expected_size >= (data_offset + sizeof(uint32_t)))
                    {
                        ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t), &glb_buf_2[3], sizeof(uint32_t)); //copy MID
                    }
                    else
                    {
                        return AR_ENEEDMORE;
                    }
                    data_offset += sizeof(uint32_t);
                    offset += glb_buf_2[0] + sizeof(uint32_t) - (4 * sizeof(uint32_t));
                }
            }

            found = TRUE;
            break;
        }
        else
        {
            offset += glb_buf_1[0];
        }
    }

	if (!found)
	{
        ACDB_ERR("Error[%d]: Processor ID(%d) does not exist", req->proc_id, AR_ENOTEXIST);
        return AR_ENOTEXIST;
	}

	return status;
}

int32_t AcdbCmdGetSubgraphProcIds(AcdbCmdGetSubgraphProcIdsReq *req, AcdbCmdGetSubgraphProcIdsRsp *rsp, uint32_t rsp_size)
{
	int32_t status = AR_EOK;
	uint32_t num_sgIds = 0;
	uint32_t total_sz = 0;
	uint32_t data_offset = 0;
	uint32_t proc_offset = 0;
	uint32_t num_modules_proc = 0;
	SubgraphModuleIIDMap *sg_mod_iid_map = NULL;

	if (rsp_size < sizeof(AcdbCmdGetSubgraphProcIdsRsp))
	{
		ACDB_ERR("AcdbCmdGetSubgraphProcIds() failed, rsp_size is invalid");
		return AR_EBADPARAM;
	}

	for (uint32_t i = 0; i < req->num_sg_ids; i++)
	{
		sg_mod_iid_map = NULL;
		proc_offset = 0;
		status = GetSubgraphIIDMap(req->sg_ids[i], &sg_mod_iid_map);
		if (AR_EOK != status)
		{
			//Return even if it can't find data for one of the given subgraphs or memory allocation failed
			return status;
		}
		if (sg_mod_iid_map != NULL)
		{
			num_sgIds++;
            total_sz += sizeof(AcdbSgProcIdsMap)
                + sg_mod_iid_map->proc_count * sizeof(uint32_t); //sg_mod_iid_map->size;
			if (rsp->sg_proc_ids != NULL)
			{
				if (rsp->size >= (data_offset + (2 * sizeof(uint32_t))))
				{
				    ACDB_MEM_CPY(
                        rsp->sg_proc_ids + data_offset, &sg_mod_iid_map->subgraph_id,
                        sizeof(sg_mod_iid_map->subgraph_id));
				    data_offset += sizeof(sg_mod_iid_map->subgraph_id);

				    ACDB_MEM_CPY(rsp->sg_proc_ids + data_offset, &sg_mod_iid_map->proc_count,
                        sizeof(sg_mod_iid_map->proc_count));
				    data_offset += sizeof(sg_mod_iid_map->proc_count);
				}
				else
				{
					return AR_ENEEDMORE;
				}

				for (uint32_t j = 0; j < sg_mod_iid_map->proc_count; j++)
				{
					if (rsp->size >= (data_offset + sizeof(uint32_t)))
					{
					    ACDB_MEM_CPY(rsp->sg_proc_ids + data_offset,
                            sg_mod_iid_map->proc_info + proc_offset, sizeof(uint32_t));
					    data_offset += sizeof(uint32_t); //size of each procID
					}
					else
					{
						return AR_ENEEDMORE;
					}

					proc_offset += sizeof(uint32_t);
					ACDB_MEM_CPY(&num_modules_proc,
                        sg_mod_iid_map->proc_info + proc_offset, sizeof(uint32_t));
					proc_offset += (sizeof(num_modules_proc)
                        + (num_modules_proc * sizeof(AcdbModuleInstance)));

				}
			}
		}
		FreeSubgraphIIDMap(sg_mod_iid_map);
	}

    sg_mod_iid_map = NULL;
	rsp->num_sg_ids = num_sgIds;
	rsp->size = total_sz;
	return status;
}

int32_t AcdbCmdGetAmdbBootupLoadModules(AcdbAmdbProcID *req, AcdbBlob *rsp, uint32_t rsp_size)
{
	int32_t status = AR_EOK;
	uint32_t boot_load_chk_offset = 0;
	uint32_t boot_load_chk_size = 0;
	uint32_t offset = 0;
	uint32_t data_offset = 0;
	uint32_t error_code = 0;
	uint32_t mod_offset = 0;
	uint32_t lsw_handle = 0;
	uint32_t msw_handle = 0;
    bool_t found = FALSE;
	uint32_t expected_size = rsp->buf_size;
	//Header: <#Num of Proc ID's>
	size_t sz_boot_load_header = sizeof(uint32_t);
	//Proc Header: <ProcID, #Num of MID's>
	size_t sz_proc_header = 2 * sizeof(uint32_t);

	if (rsp_size < sizeof(AcdbBlob))
	{
		ACDB_ERR("AcdbCmdGetAmdbBootupLoadModules() failed, rsp_size is invalid");
		return AR_EBADPARAM;
	}

	status = SearchBootUpModulesChunk(&boot_load_chk_offset, &boot_load_chk_size);
	if (AR_EOK != status)
	{
		return status;
	}

	offset = boot_load_chk_offset + (uint32_t)sz_boot_load_header;
	do
	{
		memset(&glb_buf_2, 0, sizeof(glb_buf_2));

        //read processorID and num of MID's
        status = FileManReadBuffer(&glb_buf_2, sz_proc_header, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read processor header", status);
            return status;
        }

		if (glb_buf_2[0] == req->proc_id)
		{
            //3 is moduleID, lsw_handle, msw_handle
			rsp->buf_size = (uint32_t)sz_proc_header
                + (uint32_t)sizeof(error_code)
                + (glb_buf_2[1] * 3UL * (uint32_t)sizeof(uint32_t));

			if (rsp->buf != NULL)
			{
				if (expected_size >= (data_offset + (3 * sizeof(uint32_t))))
				{
					ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t),
                        &glb_buf_2[0], sizeof(uint32_t));
				    data_offset += sizeof(uint32_t);

					ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(error_code),
                        &error_code, sizeof(error_code));
				    data_offset += sizeof(error_code);

					ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t),
                        &glb_buf_2[1], sizeof(uint32_t));
				    data_offset += sizeof(uint32_t);
				}
				else
				{
					return AR_ENEEDMORE;
				}

                status = FileManReadBuffer(&glb_buf_1, glb_buf_2[1] * sizeof(uint32_t), &offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Unable to read module ID list", status);
                    return status;
                }

                //loop num_modules to copy each moduleID, then add lsw_handle, msw_handle
				for (uint32_t i = 0; i < glb_buf_2[1]; i++)
				{
				    if (expected_size >= (data_offset + (3 * sizeof(uint32_t))))
				    {
					    ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t),
                            &glb_buf_1 + mod_offset, sizeof(uint32_t)); //copy MID
					    data_offset += sizeof(uint32_t);

					    ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t),
                            &lsw_handle, sizeof(uint32_t)); //copy lsw_handle
					    data_offset += sizeof(uint32_t);

					    ACDB_MEM_CPY_SAFE(rsp->buf + data_offset, sizeof(uint32_t),
                            &msw_handle, sizeof(uint32_t)); //copy msw_handle
					    data_offset += sizeof(uint32_t);
				    }
				    else
				    {
					    return AR_ENEEDMORE;
				    }

					mod_offset += sizeof(uint32_t);
				}
			}

			found = TRUE;
			break;
		}
		else
		{
			offset += (glb_buf_2[1] * sizeof(uint32_t));
		}
	} while (offset < boot_load_chk_size && !found);

	if (!found)
	{
		ACDB_INFO("Error[%d]: Processor ID = %d does not exist", req->proc_id);
		return AR_ENOTEXIST;
	}

	return status;
}

/**
* \brief SearchTaggedModuleMapLutForAnyTag
*		Search the Tagged Module Map LUT for SubgraphTagEntries that match the input subgraph with any tag.
* \param[int] sg_id: Subgraph ID to search for
* \param[int] sg_lut_buf: List of Tagged Module Map entries
* \param[int] buf_size: size of Tagged Module Map buffer
* \param[out] tag_id: The Tag ID associated with the Subgraph
* \param[out] def_offset: Offset of the Module(s) associated with the tag
* \param[out] entry_offset: Offset of the SubgraphTagLutEntry in the sg_lut_buf
* \return AR_EOK on success, and AR_ENOTEXIST otherwise as well as tag id and def offset
*/
int32_t SearchTaggedModuleMapLutForAnyTag(uint32_t sg_id, uint32_t* sg_lut_buf, uint32_t buf_size, uint32_t *tag_id, uint32_t* def_offset, uint32_t *entry_offset)
{
	uint32_t entry_offset_tmp = 0;
	uint32_t search_index_count = 1;
	//Search for any tag
	SubgraphTagLutEntry sgTagEntry;

	sgTagEntry.sg_id = sg_id;
	sgTagEntry.tag_id = 0;
	sgTagEntry.offset = 0;

	if (SEARCH_ERROR == AcdbDataBinarySearch2((void*)sg_lut_buf, buf_size,
		&sgTagEntry, search_index_count,
		(int32_t)(sizeof(SubgraphTagLutEntry) / sizeof(uint32_t)),
		&entry_offset_tmp))
	{
		return AR_ENOTEXIST;
	}

	ACDB_MEM_CPY(&sgTagEntry, sg_lut_buf + entry_offset_tmp * sizeof(uint32_t), sizeof(SubgraphTagLutEntry));

	if (sg_id == sgTagEntry.sg_id)
	{
		*tag_id = sgTagEntry.tag_id;
		*def_offset = sgTagEntry.offset;
		*entry_offset = entry_offset_tmp;
	}

	return AR_EOK;
}

int32_t BuildGetTagsFromGkvRsp(uint32_t sg_list_offset, AcdbCmdGetTagsFromGkvRsp *rsp)
{
	int32_t status = AR_EOK;
	ChunkInfo tmlut_ci = { ACDB_CHUNKID_TAGGED_MODULE_LUT, 0, 0 };
	ChunkInfo tmdef_ci = { ACDB_CHUNKID_TAGGED_MODULE_DEF, 0, 0 };
	ChunkInfo dp_ci = { ACDB_CHUNKID_DATAPOOL, 0, 0 };
	uint32_t num_subgraphs = 0;
	uint32_t sz_subgraph_list = 0;
	uint32_t cur_sg_id = 0;
	uint32_t cur_num_dst = 0;
	uint32_t sg_id_index = 0;
	uint32_t offset = 0;
	uint32_t rsp_offset = 0;
	uint32_t num_sg_tag_entries = 0;
	uint32_t lut_size = 0;

	uint32_t tag_id = 0;
    uint32_t def_offset = 0;
    uint32_t def_offset_2 = 0;
	uint32_t entry_offset = 0;
	uint32_t num_modules = 0;
	uint32_t sz_tag_module_obj = 0;
	uint32_t sz_module_list = 0;
	bool_t new_tag_found = TRUE;

	uint32_t expected_size = rsp->list_size;

    status = ACDB_GET_CHUNK_INFO(&tmlut_ci, &tmdef_ci, &dp_ci);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get one or more chunks", status);
        return status;
    }

    offset = dp_ci.chunk_offset + sg_list_offset;
    status = FileManReadBuffer(&glb_buf_3, 2 * sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read subgraph list header<size, #subgraphs>", status);
        return status;
    }

	sz_subgraph_list = glb_buf_3[0];
	num_subgraphs = glb_buf_3[1];

	ACDB_CLEAR_BUFFER(glb_buf_3);

	//GLB BUFFER 2 stores subgraph list data structure

    //GLB BUFFER 2 stores subgraph list data structure
    status = FileManReadBuffer(&glb_buf_2, sz_subgraph_list, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of subgraph-tag entries", status);
        return status;
    }

    offset = tmlut_ci.chunk_offset;
    status = FileManReadBuffer(&num_sg_tag_entries, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of subgraph-tag entries", status);
        return status;
    }

	lut_size = num_sg_tag_entries * sizeof(SubgraphTagLutEntry);
    status = FileManReadBuffer(&glb_buf_1, lut_size, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read size of subgraph-tag lookup table", status);
        return status;
    }

	//Use the remaining portion of GLB BUF 1 to store found TAG<Tag IDs, Def Offset>
	uint32_t sz_glb_buf_1_reamining = sizeof(glb_buf_1) - lut_size;
    uint32_t glb_buf_1_pos = (uint32_t)(lut_size / sizeof(uint32_t));
	uint32_t* tag_defofst_list = &glb_buf_1[glb_buf_1_pos];
	uint32_t tag_defost_index = 0;
	uint32_t tag_defofst_max_index = (uint32_t)(sz_glb_buf_1_reamining / sizeof(uint32_t));

	uint32_t* found_tags_list = &glb_buf_3[0];
	uint32_t tag_index = 0;

	rsp->num_tags = 0;
	rsp->list_size = 0;

	/*
	* GLB BUF 1 stores Taged Module Map lookup table entries
	* GLB BUF 1 also stores found tag IDs in the left over memory
	* GLB BUF 2 stores Subgraph List structure
	* GLB BUF 3 stores Module<ID, Instance ID>
	* Get Subgraph ID from GLB BUF 2 and search for all associated Tags in GLB BUF 1
	*/
	for (uint32_t i = 0; i < num_subgraphs; i++)
	{
		cur_sg_id = glb_buf_2[sg_id_index];
		cur_num_dst = glb_buf_2[sg_id_index + 1];
		sg_id_index +=
				(
					sizeof(cur_sg_id) +
					sizeof(cur_num_dst) +
					(cur_num_dst * sizeof(uint32_t))
				)/sizeof(uint32_t);

		//Search Tag LUT for all Tags associated with SGID
		//During the binary search remove any entry found from the entry from GLB_BUF_1 by overriting the entry with 0's
		int32_t search_status = AR_EOK;
		while (search_status != AR_ENOTEXIST)
		{
			tag_id = 0;
			def_offset = 0;
			entry_offset = 0;
			num_modules = 0;
			sz_tag_module_obj = 0;
			sz_module_list = 0;
			new_tag_found = TRUE;
			offset = 0;

			//Using Index 0 for SubgraphID as sort key
			status = AcdbSort2(
				lut_size,
				&glb_buf_1[0],
				sizeof(SubgraphTagLutEntry), 0);

			//for each Subgraph Tag entry, see if the current subgraph ID
			//matches then save tag id def offset mapping
			search_status = SearchTaggedModuleMapLutForAnyTag(cur_sg_id, &glb_buf_1[0], lut_size,
				&tag_id, &def_offset, &entry_offset);

			if (AR_FAILED(search_status)) break;

			//Remove found SubgraphTagLutEntry from GLB BUF 1
			memset((void*)&glb_buf_1[entry_offset], 0, sizeof(SubgraphTagLutEntry));

			if (tag_defost_index + 1 > tag_defofst_max_index)
			{
				ACDB_ERR_MSG_1("Tag definition offset list is full", status);
				status = AR_EFAILED;
				break;
			}
			else
			{
				tag_defofst_list[tag_defost_index] = tag_id;
				tag_defofst_list[tag_defost_index + 1] = def_offset;
				tag_defost_index += 2;
			}

			//Start building response here

			//Get <Module ID, Instance ID> list from Def offset
            def_offset_2 = tmdef_ci.chunk_offset + def_offset;

            status = FileManReadBuffer(&num_modules, sizeof(uint32_t), &def_offset_2);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read number of modules", status);
                return status;
            }

			offset += sizeof(num_modules);
			sz_module_list = num_modules * sizeof(AcdbModule);

            if (sz_module_list == 0)
            {
                ACDB_ERR("Ignoring unused tag Tag(0x%x). It is not associated with any module.", tag_id);
                continue;
            }

			for (uint32_t j = 0; j < tag_index; j++)
			{
				if (tag_id == found_tags_list[j])
				{
					new_tag_found = FALSE;
					break;
				}
			}

			if (!new_tag_found)
			{
				//found tag id in found_tags_list
				sz_tag_module_obj = sz_module_list;
			}
			else
			{
				//A new tag is being added
				found_tags_list[tag_index] = tag_id;
				tag_index++;
				sz_tag_module_obj = sizeof(tag_id) + sizeof(num_modules) + sz_module_list;
				rsp->num_tags += 1;
			}

			rsp->list_size += sz_tag_module_obj;
		}
	}

	if (!IsNull(rsp->tag_module_list))
	{
		uint32_t prev_tag_id = 0;
		uint32_t total_num_modules = 0;
		uint32_t rsp_offset_num_modules = 0;

		uint32_t sz_tag_defofst_list = tag_defost_index * sizeof(uint32_t);
		status = AcdbSort2(
			sz_tag_defofst_list,
			tag_defofst_list,
			2 * sizeof(uint32_t), 0);//Using index 0 for tag id

		if (AR_FAILED(status))
		{
			ACDB_ERR_MSG_1("Failed to sort Tag IDs and offsets", status);
			return status;
		}

		prev_tag_id = 0;

		for (uint32_t i = 0; i + 1< tag_defost_index; i+=2)
		{
			tag_id = tag_defofst_list[i];
			def_offset = tag_defofst_list[i + 1];
			num_modules = 0;
			sz_module_list = 0;

            def_offset_2 = tmdef_ci.chunk_offset + def_offset;
            status = FileManReadBuffer(&num_modules, sizeof(uint32_t), &def_offset_2);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Unable to read number of modules", status);
                return status;
            }

			sz_module_list = num_modules * sizeof(AcdbModule);

            if (sz_module_list == 0)
            {
                continue;
            }

			if (prev_tag_id != tag_id)
			{
				if (expected_size >= (rsp_offset + sizeof(tag_id)))
				{
				ACDB_MEM_CPY_SAFE(rsp->tag_module_list + rsp_offset,
					sizeof(tag_id), &tag_id, sizeof(tag_id));
				}
				else
				{
					return AR_ENEEDMORE;
				}

				rsp_offset += sizeof(tag_id);

				if (expected_size >= (rsp_offset + sizeof(num_modules)))
				{
				ACDB_MEM_CPY_SAFE(rsp->tag_module_list + rsp_offset,
					sizeof(num_modules), &num_modules, sizeof(num_modules));
				}
				else
				{
					return AR_ENEEDMORE;
				}

				rsp_offset_num_modules = rsp_offset;
				rsp_offset += sizeof(num_modules);
				total_num_modules = num_modules;

                if (num_modules != 0)
                {
					if (expected_size >= (rsp_offset + sz_module_list))
					{
                    status = CopyFromFileToBuf(def_offset_2,
                        (uint8_t*)rsp->tag_module_list + rsp_offset, sz_module_list, sz_module_list);
					}
					else
					{
						return AR_ENEEDMORE;
					}
                    rsp_offset += sz_module_list;
                    def_offset_2 += sz_module_list;
                }
			}
			else
			{
				total_num_modules += num_modules;
				//update num_modules and write new modules
				if (expected_size >= (rsp_offset_num_modules + sizeof(total_num_modules)))
				{
				ACDB_MEM_CPY_SAFE(rsp->tag_module_list + rsp_offset_num_modules,
					sizeof(total_num_modules), &total_num_modules, sizeof(total_num_modules));
				}
				else
				{
					return AR_ENEEDMORE;
				}

                if (num_modules != 0)
                {
					if (expected_size >= (rsp_offset + sz_module_list))
					{
                    status = CopyFromFileToBuf(def_offset_2,
                        (uint8_t*)rsp->tag_module_list + rsp_offset, sz_module_list, sz_module_list);
					}
					else
					{
						return AR_ENEEDMORE;
					}
                    rsp_offset += sz_module_list;
                    def_offset_2 += sz_module_list;
                }
			}

			if (AR_FAILED(status))
			{
				ACDB_ERR_MSG_1("Failed to read Tag module list from file", status);
				return status;
			}

			prev_tag_id = tag_id;
		}
	}

	ACDB_CLEAR_BUFFER(glb_buf_1);
	ACDB_CLEAR_BUFFER(glb_buf_2);
	ACDB_CLEAR_BUFFER(glb_buf_3);

	if (rsp->num_tags == 0)
	{
		ACDB_ERR_MSG_1("No tags were found", status);
		status = AR_ENOTEXIST;
	}

	return status;
}

int32_t AcdbCmdGetTagsFromGkv(AcdbCmdGetTagsFromGkvReq *req, AcdbCmdGetTagsFromGkvRsp *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    uint32_t gkvLutOff = 0;
    uint32_t sgListOffset = 0;
    uint32_t sgDataOffset = 0;
    AcdbGraphKeyVector graph_kv = { 0 };

    if (req->graph_key_vector->num_keys == 0)
    {
        ACDB_ERR("Error[%d]: No data exist for the empty GKV",
            AR_ENOTEXIST);
        return AR_ENOTEXIST;
    }
    else
    {
        ACDB_DBG("Retrieving Tag list for the following graph key vector:");
        LogKeyVector(req->graph_key_vector, GRAPH_KEY_VECTOR);
    }

    if (rsp_size < sizeof(AcdbCmdGetTagsFromGkvRsp))
    {
        ACDB_ERR("Error[%d]: The response structure size is less than %d",
            AR_EBADPARAM, sizeof(AcdbCmdGetTagsFromGkvRsp));
        return AR_EBADPARAM;
    }

    //GLB BUF 3 is used to store the sorted GKV
    graph_kv.num_keys = req->graph_key_vector->num_keys;
    graph_kv.graph_key_vector = (AcdbKeyValuePair*)&glb_buf_3[0];
    ACDB_MEM_CPY_SAFE(&glb_buf_3[0],
        graph_kv.num_keys * sizeof(AcdbKeyValuePair),
        req->graph_key_vector->graph_key_vector,
        graph_kv.num_keys * sizeof(AcdbKeyValuePair));

    //Sort Key Vector by Key IDs(position 0 of AcdbKeyValuePair)
    status = AcdbSort2(graph_kv.num_keys * sizeof(AcdbKeyValuePair),
        graph_kv.graph_key_vector, sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort the graph key vector.", status);
        return status;
    }

    status = SearchGkvKeyTable(&graph_kv, &gkvLutOff);

    if (AR_FAILED(status)) return status;

    status = SearchGkvLut(&graph_kv, gkvLutOff,
        &sgListOffset, &sgDataOffset);

    if (AR_FAILED(status)) return status;

    status = BuildGetTagsFromGkvRsp(sgListOffset, rsp);

    if (AR_FAILED(status)) return status;

    return status;
}

/**
* \brief
*		Interleaves two lists. If List 1 is [1 2 3 4] and List 2 is [5 6 7 8]
*       The resulting list is List[1 5 2 6 3 7 4 8]
*
* Note: An inplace shuffle can be used zip within the same array
*
* \param[in] num_elem_1: Number of elements in list 1
* \param[in] list_1: List to be interleaved with list 2
* \param[in] num_elem_2: Number of elements in list 2
* \param[in] list_2: List to be interleaved with list 1
* \param[in] num_elem_3: Number of elements in list 3
* \param[in/out] list_3: The interleaved list
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbZip1(uint32_t num_elem_1, uint32_t *list_1,
    uint32_t num_elem_2, uint32_t *list_2,
    uint32_t num_elem_3, uint32_t *list_3)
{
    if (num_elem_1 != num_elem_2)
        return AR_EBADPARAM;

    if(IsNull(list_1) || IsNull(list_2))
        return AR_EBADPARAM;

    if(num_elem_3 != num_elem_1 + num_elem_2)
        return AR_EBADPARAM;

    uint32_t index_1 = 0, index_2 = 0;
    for (uint32_t i = 0; i < num_elem_3 ; i++)
    {
        if (i % 2 == 0)
        {
            list_3[i] = list_1[index_1];
            index_1++;
        }
        else
        {
            list_3[i] = list_2[index_2];
            index_2++;
        }
    }

    return AR_EOK;
}

/**
* \brief
*		Checks for the existance of a CKV within the lookup table
*
* \param[in] kv_list: contains the key id and value lists that are
*            used to compare against an entry in the lookup table
* \param[in] lookup: a look up table that contains
*            <key tbl off/value tbl off> pairs
* \param[in/out] status: AR status
*
* \return FALSE(0) or TRUE(1)
*/
bool_t LookupContainsCKV(AcdbKeyValueList *kv_list,
    AcdbKeyVectorLut* lookup, int32_t *status)
{
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    uint32_t *key_list = NULL;
    uint32_t *value_list = NULL;
    uint32_t *tmp_buf = NULL;
    bool_t contains_ckv = FALSE;
    if (IsNull(kv_list) || IsNull(lookup) || IsNull(status))
        return AR_EBADPARAM;

    *status = AR_EOK;
    AcdbKeyVectorLutEntry entry = { 0 };
    for (uint32_t i = 0; i < lookup->num_entries; i++)
    {
        entry = lookup->lookup_table[i];
        offset = entry.offset_key_tbl;

        *status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
        if (AR_FAILED(*status))
        {
            ACDB_ERR("Error[%d]: Failed to read number of keys.", *status);
            return contains_ckv;
        }

        if (num_keys != kv_list->num_keys)
            continue;

        if (num_keys == 0)
            return TRUE;

        if (IsNull(tmp_buf))
        {
            tmp_buf = ACDB_MALLOC(uint32_t, 2 * num_keys);
            if (IsNull(tmp_buf))
            {
                *status = AR_ENOMEMORY;
                return contains_ckv;
            }
            key_list = &tmp_buf[0];
            value_list = &tmp_buf[num_keys];
        }

        *status = FileManReadBuffer(key_list,
            num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(*status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key ID List", *status);
            return contains_ckv;
        }

        if (0 != ACDB_MEM_CMP(kv_list->key_list, key_list,
            num_keys * sizeof(uint32_t)))
        {
            continue;
        }

        offset = entry.offset_value_list;
        *status = FileManReadBuffer(value_list,
            num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(*status))
        {
            ACDB_ERR("Error[%d]: Failed to read Value List", *status);
            return contains_ckv;
        }

        if (0 == ACDB_MEM_CMP(kv_list->value_list, value_list,
            num_keys * sizeof(uint32_t)))
        {
            contains_ckv = TRUE;
            break;
        }
    }

    if (!IsNull(tmp_buf))
    {
        ACDB_FREE(tmp_buf);
        tmp_buf = NULL;
        key_list = NULL;
        value_list = NULL;
    }

    return contains_ckv;
}

/**
* \brief
*		Adds a <key tbl off/value tbl off> pair to the lookup table. The
*       value tbl off. points to an entry with in the Value LUT, i.e the
*       actual list of values.
*       Ex. value tbl off -> [v1, v2, v3, def, dot] (one entry in the
*       Value LUT)
*
* \param[in] entry: a <key tbl off/value tbl off> pair
*            used to compare against an entry in the lookup table
* \param[in] lookup: a look up table that contains
*            <key tbl off/value tbl off> pairs
*
* \return 0 on success, and non-zero on failure
*/
int32_t AddCkvEntryToLookup(AcdbKeyVectorLutEntry *entry,
    AcdbKeyVectorLut* lookup)
{
    int32_t status = AR_EOK;
    ChunkInfo ci_calkeytbl = { 0 };
    ChunkInfo ci_callut = { 0 };

    ci_calkeytbl.chunk_id = ACDB_CHUNKID_CALKEYTBL;
    ci_callut.chunk_id = ACDB_CHUNKID_CALDATALUT;

    status = ACDB_GET_CHUNK_INFO(&ci_calkeytbl, &ci_callut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get chunk info for either "
            "Cal Key or Cal Lookup table.", AR_EBADPARAM);
        return status;
    }

    if (IsNull(entry) || IsNull(lookup))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " key vector entry or lookup.", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (lookup->size == 0)
    {
        ACDB_ERR("Error[%d]: The lookup table size is zero", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (lookup->num_entries * sizeof(entry) >= lookup->size)
    {
        ACDB_ERR("Error[%d]: The lookup table is full", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ACDB_MEM_CPY(&lookup->lookup_table[lookup->num_entries],
        entry, sizeof(*entry));
    lookup->num_entries++;

    return AR_EOK;
}

/**
* \brief
*		Retrieves all the audio cal key vector combinations for a set of keys given
*       the offset of the Key ID and Key Value tables
*
* \depends
*       Makes use of GLB_BUF_3 to store key vector data
*
* \param[in/out] lookup: Keeps track of key vectors already found
*            Key ID and Key Value tables
* \param[in] cal_key_table_entry: An entry that holds the offsets to the
*            Key ID and Key Value tables
* \param[in/out] blob_offset: current offset in the rsp
* \param[in] expected_size: the expected size of the rsp.list_size
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t GetAllAudioCKVVariations(AcdbKeyVectorLut *lookup,
    AcdbCalKeyTblEntry *cal_key_table_entry,
    uint32_t *blob_offset, uint32_t expected_size, AcdbKeyVectorList *rsp)
{
    int32_t status = AR_EOK;
    //bool_t found = FALSE;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    //uint32_t num_lut_entries = 0;
    ChunkInfo ci_calkeytbl = { 0 };
    ChunkInfo ci_callut = { 0 };
    AcdbOp op = ACDB_OP_NONE;
    KeyTableHeader key_value_table_header = { 0 };
    uint32_t *key_id_list = NULL;
    uint32_t *key_value_list = NULL;
    uint32_t *ckv = NULL;
    /* Contains offsets to key table and offset of a particualr value list
     * in the value lut. The offsets include the respective chunk offset */
    AcdbKeyVectorLutEntry ckv_entry = { 0 };
    AcdbKeyValueList kv_list = { 0 };

    if (IsNull(lookup) || IsNull(cal_key_table_entry)
        || IsNull(blob_offset) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    op = IsNull(rsp->key_vector_list) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    ci_calkeytbl.chunk_id = ACDB_CHUNKID_CALKEYTBL;
    ci_callut.chunk_id = ACDB_CHUNKID_CALDATALUT;

    status = ACDB_GET_CHUNK_INFO(&ci_calkeytbl, &ci_callut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get chunk info for either "
            "Cal Key or Cal Lookup table.", AR_EBADPARAM);
        return status;
    }

    offset = ci_calkeytbl.chunk_offset +
        cal_key_table_entry->offset_cal_key_tbl;
    ckv_entry.offset_key_tbl = offset;

    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of keys.", status);
        return status;
    }

    if (num_keys != 0)
    {
        status = FileManReadBuffer(&glb_buf_3[0],
            num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key ID List", status);
            return status;
        }
    }

    offset = ci_callut.chunk_offset +
        cal_key_table_entry->offset_cal_lut;
    status = FileManReadBuffer(&key_value_table_header,
        sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of values "
            "and lut entries.", status);
        return status;
    }

    if (num_keys != key_value_table_header.num_keys)
    {
        ACDB_ERR("Error[%d]: Number of keys and values are"
            " different!", AR_EFAILED);
        return AR_EFAILED;
    }
    //GLB_BUF_3 stores the key vector IDs followed by the Values
    //Ex. [K1 K2 K2 V1 V2 V3]
    key_id_list = &glb_buf_3[0];
    key_value_list = &glb_buf_3[num_keys];
    ckv = &glb_buf_3[num_keys * 2];

    kv_list.num_keys = num_keys;
    kv_list.key_list = key_id_list;
    kv_list.value_list = key_value_list;

    for (uint32_t i = 0; i < key_value_table_header.num_entries; i++)
    {
        ckv_entry.offset_value_list = offset;

        if (num_keys != 0)
        {
            status = FileManReadBuffer(key_value_list,
                num_keys * sizeof(uint32_t), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
                return status;
            }
        }

        offset += sizeof(AcdbCkvLutEntryOffsets);

        /* Check to see if we already accounted for the CKV. If so, skip
         * the CKV. Otherwise add the CKV to the lookup */
        if (LookupContainsCKV(&kv_list, lookup, &status)
            && AR_SUCCEEDED(status))
        {
            continue;
        }

        status = AddCkvEntryToLookup(&ckv_entry, lookup);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to add ckv entry to lookup.", status);
        }

        rsp->num_key_vectors ++;
        rsp->list_size += (sizeof(num_keys)
            + num_keys * sizeof(AcdbKeyValuePair));

        if (op == ACDB_OP_GET_SIZE) continue;

        //Write Key Vector
        status = AcdbZip1(num_keys, key_id_list,
            num_keys, key_value_list,
            num_keys * 2, ckv);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to zip key id and"
                " key value list.", status);
            return status;
        }

        //Write num keys
        if (*blob_offset
            + sizeof(uint32_t) > expected_size)
        {
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
            &num_keys, num_keys * sizeof(AcdbKeyValuePair));
        *blob_offset += sizeof(uint32_t);

        if (num_keys == 0) continue;

        //Write Key Vector
        if (*blob_offset
            + num_keys * sizeof(AcdbKeyValuePair) > expected_size)
        {
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
            ckv, num_keys * sizeof(AcdbKeyValuePair));
        *blob_offset += num_keys * sizeof(AcdbKeyValuePair);

        //<CKV.Values, DEF, DOT, DOT2>
        //offset += sizeof(AcdbCkvLutEntryOffsets);
    }

    ACDB_CLEAR_BUFFER(glb_buf_3);
    return status;
}

/**
* \brief
*		Retrieves all the voice cal key vector combinations
*       for one voice subgraph
*
* \depends
*       Makes use of GLB_BUF_3 to store key vector data
*
* \param[in/out] lookup: Keeps track of key vectors already found
*            Key ID and Key Value tables
* \param[in] file_offset: absolute file offset of a
*            subgraphs vcpm cal data
* \param[in/out] blob_offset: current offset in the rsp
* \param[in] expected_size: the expected size of the rsp.list_size
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t GetAllVoiceCKVVariations(AcdbKeyVectorLut *lookup,
    uint32_t *file_offset, AcdbVcpmSubgraphCalTable *sg_cal_data_tbl_header,
    uint32_t *blob_offset, uint32_t expected_size, AcdbKeyVectorList *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t kv_tbl_offset = 0;
    uint32_t num_keys = 0;
    uint32_t *ckv = NULL;
    uint32_t *key_id_list = NULL;
    uint32_t *key_value_list = NULL;
    AcdbKeyVectorLutEntry ckv_entry = { 0 };
    AcdbKeyValueList kv_list = { 0 };
    ChunkInfo ci_vcpm_key_tbl = { ACDB_CHUNKID_VCPM_CALKEY_TABLE, 0, 0 };
    ChunkInfo ci_vcpm_value_tbl = { ACDB_CHUNKID_VCPM_CALDATA_LUT, 0, 0 };
    AcdbVcpmCkvDataTable ckv_data_tbl_header = { 0 };
    AcdbVcpmCalDataObj cal_data_obj = { 0 };
    AcdbOp op = ACDB_OP_NONE;

    if (IsNull(lookup) || IsNull(file_offset)
        || IsNull(sg_cal_data_tbl_header)
        || IsNull(blob_offset) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    op = IsNull(rsp->key_vector_list) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    status = ACDB_GET_CHUNK_INFO(&ci_vcpm_key_tbl, &ci_vcpm_value_tbl);
    if (AR_FAILED(status))
    {
        if (ci_vcpm_key_tbl.chunk_size == 0)
            ACDB_ERR("Error[%d]: Failed to get chunk info for the"
                " VCPM Calibration Key Table", status);
        if (ci_vcpm_value_tbl.chunk_size == 0)
            ACDB_ERR("Error[%d]: Failed to get chunk info for the"
                " VCPM Calibration Data Table", status);
    }

    offset = *file_offset;

    for (uint32_t i = 0; i < sg_cal_data_tbl_header->num_ckv_data_table; i++)
    {
        //Get Voice Key Table offset and combine key id list with value list
        status = FileManReadBuffer(&ckv_data_tbl_header,
            sizeof(AcdbVcpmCkvDataTable), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read VCPM CKV Data Table header.",
                status);
            return status;
        }

        for (uint32_t j = 0; j < ckv_data_tbl_header.num_caldata_obj; j++)
        {
            status = FileManReadBuffer(&cal_data_obj,
                sizeof(AcdbVcpmCalDataObj), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read VCPM Cal Data Obj" ,
                    status);
                return status;
            }

            offset += cal_data_obj.num_data_offsets * sizeof(uint32_t);

            ckv_entry.offset_key_tbl = ci_vcpm_key_tbl.chunk_offset
                + ckv_data_tbl_header.offset_voice_key_table;
            ckv_entry.offset_value_list = ci_vcpm_value_tbl.chunk_offset
                + cal_data_obj.offset_vcpm_ckv_lut
                + 2 * sizeof(uint32_t);

            kv_tbl_offset = ckv_entry.offset_key_tbl;

            //read number of keys from key table
            status = FileManReadBuffer(&num_keys,
                sizeof(uint32_t), &kv_tbl_offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read VCPM Cal Data Obj"
                    , status);
                return status;
            }

            //GLB_BUF_3 stores the key vector IDs followed by the Values
            //Ex. [K1 K2 K2 V1 V2 V3]
            key_id_list = &glb_buf_3[0];
            key_value_list = &glb_buf_3[num_keys];
            ckv = &glb_buf_3[num_keys * 2];

            kv_list.num_keys = num_keys;
            kv_list.key_list = key_id_list;
            kv_list.value_list = key_value_list;

            if (num_keys != 0)
            {
                status = FileManReadBuffer(&glb_buf_3[0],
                    num_keys * sizeof(uint32_t), &kv_tbl_offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Failed to read Key ID List", status);
                    return status;
                }

                /* Skip #keys and #entries(#entries is always 1 for
                 * each voice value table */
                kv_tbl_offset = ckv_entry.offset_value_list;
                status = FileManReadBuffer(key_value_list,
                    num_keys * sizeof(uint32_t), &kv_tbl_offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
                    return status;
                }
            }

            /* Check to see if we already accounted for the CKV. If so, skip
            * the CKV. Otherwise add the CKV to the lookup */
            if (LookupContainsCKV(&kv_list, lookup, &status)
                && AR_SUCCEEDED(status))
            {
                continue;
            }

            status = AddCkvEntryToLookup(&ckv_entry, lookup);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to add ckv entry to lookup.", status);
            }

            rsp->num_key_vectors++;
            rsp->list_size += (sizeof(num_keys)
                + num_keys * sizeof(AcdbKeyValuePair));

            if (op == ACDB_OP_GET_SIZE) continue;

            //Write Key Vector
            status = AcdbZip1(num_keys, key_id_list,
                num_keys, key_value_list,
                num_keys * 2, ckv);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to zip key id and"
                    " key value list.", status);
                return status;
            }

            //Write num keys
            if (*blob_offset
                + sizeof(uint32_t) > expected_size)
            {
                return AR_ENEEDMORE;
            }

            ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
                &num_keys, sizeof(uint32_t));
            *blob_offset += sizeof(uint32_t);

            if (num_keys == 0) continue;

            //Write Key Vector
            if (*blob_offset
                + num_keys * sizeof(AcdbKeyValuePair) > expected_size)
            {
                return AR_ENEEDMORE;
            }

            ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
                ckv, num_keys * sizeof(AcdbKeyValuePair));
            *blob_offset += num_keys * sizeof(AcdbKeyValuePair);

        }
    }

    return status;
}

/**
* \brief
*		Builds a list of cal key vectors using the subgraphs defined in a graph
*
* \param[in] sg_list_offset: Offset of the subgraph list
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t BuildCalKeyVectorList(
    AcdbKeyVectorList *rsp, uint32_t sg_list_offset)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t sg_vcpm_data_offset = 0;
    uint32_t data_pool_offset = 0;
    uint32_t ckv_list_offset = 0;
    uint32_t blob_offset = 0;
    //The expected size of rps.key_vector_list set by client
    uint32_t expected_size = 0;
    ChunkInfo ci_data_pool = { 0 };
    ChunkInfo ci_vcpm_cal = { 0 };
    SgListHeader sg_list_header = { 0 };
    SgListObjHeader sg_obj = { 0 };
    AcdbSgCalLutHeader sg_entry_header = { 0 };
    AcdbCalKeyTblEntry e = { 0 };
    /* Keeps track of key vectors that have already been found */
    AcdbKeyVectorLut lookup = { 0 };
    AcdbVcpmSubgraphCalHeader sg_vcpm_caldata_header = { 0 };
    AcdbVcpmSubgraphCalTable sg_cal_tbl_header = { 0 };
    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The input parameter rsp is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (!IsNull(rsp->key_vector_list))
    {
        expected_size = rsp->list_size;
        rsp->list_size = 0;
        rsp->num_key_vectors = 0;
    }

    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    ci_vcpm_cal.chunk_id = ACDB_CHUNKID_VCPM_CAL_DATA;
    status = ACDB_GET_CHUNK_INFO(&ci_data_pool, &ci_vcpm_cal);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Data Pool chunk info.",
            status);
        return status;
    }

    //The KV Lookup Table uses GLB BUF 1
    ACDB_CLEAR_BUFFER(glb_buf_1);
    lookup.size = sizeof(glb_buf_1);
    lookup.lookup_table = (AcdbKeyVectorLutEntry*)&glb_buf_1;

    data_pool_offset = ci_data_pool.chunk_offset + sg_list_offset;
    status = FileManReadBuffer(&sg_list_header,
        sizeof(SgListHeader), &data_pool_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read the subgraph"
            " list header", status);
        return status;
    }

    offset = ci_vcpm_cal.chunk_offset;
    status = FileManReadBuffer(&sg_vcpm_caldata_header,
        sizeof(AcdbVcpmSubgraphCalHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: VCPM Subgraph Caldata header.", status);
        return status;
    }

    // Loop through each subgraph and combine all the key vectors into a list
    for (uint32_t i = 0; i < sg_list_header.num_subgraphs; i++)
    {

        status = FileManReadBuffer(&sg_obj, sizeof(SgListObjHeader),
            &data_pool_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read the subgraph"
                " object header", status);
            return status;
        }

        //Skip over subgraph destinations
        data_pool_offset += sg_obj.num_subgraphs * sizeof(uint32_t);

        //Get Voice CKVs for a voice subgraph
        sg_vcpm_data_offset = offset;
        status = GetVcpmSubgraphCalTable(sg_vcpm_caldata_header.num_subgraphs,
            sg_obj.subgraph_id, &sg_vcpm_data_offset, &sg_cal_tbl_header);
        if (AR_SUCCEEDED(status))
        {
            status = GetAllVoiceCKVVariations(&lookup, &sg_vcpm_data_offset,
                &sg_cal_tbl_header, &blob_offset, expected_size, rsp);
            if (AR_FAILED(status) && status != AR_ENOTEXIST)
            {
                ACDB_ERR("Error[%d]: Failed to get Voice CKVs for "
                    "Subgraph(0x%x)", status);
                return status;
            }

            continue;
        }

        //Get Audio CKVs for a subgraph
        sg_entry_header.subgraph_id = sg_obj.subgraph_id;
        status = SearchSubgraphCalLut2(&sg_entry_header, &ckv_list_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to search for CKV table "
                "offset list for Subgraph(0x%x)", status, sg_obj.subgraph_id);
            return status;
        }

        //Search List of Offsets for matching CKV
        for (uint32_t j = 0; j < sg_entry_header.num_ckv_entries; j++)
        {
            status = FileManReadBuffer(&e, sizeof(AcdbCalKeyTblEntry),
                &ckv_list_offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: CKV key table and"
                    " value table offsets", status);
                return status;
            }

            status = GetAllAudioCKVVariations(
                &lookup, &e, &blob_offset, expected_size, rsp);
            if (AR_FAILED(status) && status != AR_ENOTEXIST)
            {
                ACDB_ERR("Error[%d]: CKV key table and"
                    " value table offsets", status);
                return status;
            }
        }
    }

    return status;
}

/**
* \brief
*       Search for the first occurance of a <Subgraph ID, Tag ID, Def Offset>
*       tuple using the Subgraph ID as a search key and returns the file
*       offset of the first occurrance.
*
* \param[in/out] sg_tag_entry: Tag Key Data Table entry in the
*           format of <Subgraph ID, Tag ID, Def Offset>
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t TagKeyTableFindFirstOfSubgraphID(SubgraphTagLutEntry *sg_tag_entry, uint32_t* file_offset)
{
    int32_t status = AR_EOK;
    //uint32_t offset = 0;
    ChunkInfo ci_tag_key_tbl =
    { ACDB_CHUNKID_MODULE_TAGDATA_KEYTABLE, 0, 0 };

    //Binary Search based on Subgraph id
    uint32_t search_index_count = 1;
    uint32_t sz_gsl_cal_lut = 0;
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    if (IsNull(sg_tag_entry) || IsNull(file_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameters is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&ci_tag_key_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get Module Tag Key Table Chunk",
            status);
        return status;
    }

    sz_gsl_cal_lut = ci_tag_key_tbl.chunk_size - sizeof(uint32_t);

    //Setup Table Information
    table_info.table_offset = ci_tag_key_tbl.chunk_offset + sizeof(uint32_t);
    table_info.table_size = sz_gsl_cal_lut;
    table_info.table_entry_size = sizeof(SubgraphTagLutEntry);

    //Setup Search Information
    search_info.num_search_keys = search_index_count;
    search_info.num_structure_elements =
        (sizeof(SubgraphTagLutEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = sg_tag_entry;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to find Subgraph ID(0x%x)", sg_tag_entry->sg_id);
        return status;
    }

    *file_offset = ci_tag_key_tbl.chunk_offset
        + sizeof(uint32_t) //Number of <SubgraphID, Tag> entries
        + search_info.entry_offset;

    ACDB_CLEAR_BUFFER(glb_buf_1);

    return status;
}

/**
* \brief
*       Get the data pool offset of the Tag Key List associated with a Module
*       Tag
*
* \param[in] tag_id: The module tag to get the key ID list for
* \param[in/out] dpo: The absolute data pool offset that points to the tag
*                key table in the data pool
*
* \return AR_EOK on success, non-zero otherwise
*/
int32_t FindTagKeyList(uint32_t tag_id, uint32_t* dpo)
{
    int32_t status = AR_EOK;
    ChunkInfo ci_tag_key_list_lut = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };
    TagKeyListEntry tag_key_list_entry = { 0 };

    if (IsNull(dpo))
    {
        ACDB_ERR("Error[%d]: One or more input parameters is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_tag_key_list_lut.chunk_id = ACDB_CHUNKID_MODULE_TAG_KEY_LIST_LUT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    status = ACDB_GET_CHUNK_INFO(&ci_data_pool, &ci_tag_key_list_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get Module Tag Key Table Chunk",
            status);
        return status;
    }

    tag_key_list_entry.tag_id = tag_id;

    //Setup Table Information
    table_info.table_offset =
        ci_tag_key_list_lut.chunk_offset + sizeof(uint32_t);
    table_info.table_size = ci_tag_key_list_lut.chunk_size
        - sizeof(uint32_t);
    table_info.table_entry_size = sizeof(TagKeyListEntry);

    //Setup Search Information
    search_info.num_search_keys = 1;//Binary Search based on Tag id
    search_info.num_structure_elements =
        (sizeof(TagKeyListEntry) / sizeof(uint32_t));
    search_info.table_entry_struct = &tag_key_list_entry;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to find Tag ID(0x%x)", tag_id);
        return status;
    }

    //Key Table format: [Table Size, Num Keys, KeyID+]
    *dpo = ci_data_pool.chunk_offset + tag_key_list_entry.key_list_offset
        + sizeof(uint32_t);//Skip size
    ACDB_CLEAR_BUFFER(glb_buf_1);

    return status;
}

/**
* \brief
*		Retrieves all the audio cal key vector combinations for a set of keys given
*       the offset of the Key ID and Key Value tables
*
* \depends
*       Makes use of GLB_BUF_3 to store key vector data
*
* \param[in/out] lookup: Keeps track of key vectors already found
*            Key ID and Key Value tables
* \param[in] cal_key_table_entry: An entry that holds the offsets to the
*            Key ID and Key Value tables
* \param[in/out] blob_offset: current offset in the rsp
* \param[in] expected_size: the expected size of the rsp.list_size
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t GetAllTKVVariations(SubgraphTagLutEntry *sg_tag_entry,
    uint32_t *blob_offset, uint32_t expected_size, AcdbTagKeyVectorList *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    ChunkInfo ci_tag_data_lut_tbl = { 0 };
    AcdbOp op = ACDB_OP_NONE;
    KeyTableHeader key_value_table_header = { 0 };
    uint32_t *key_id_list = NULL;
    uint32_t *key_value_list = NULL;
    uint32_t *tkv = NULL;

    op = IsNull(rsp->key_vector_list) ? ACDB_OP_GET_SIZE : ACDB_OP_GET_DATA;

    ci_tag_data_lut_tbl.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_LUT;

    status = ACDB_GET_CHUNK_INFO(&ci_tag_data_lut_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to get chunk info for: "
            "Tag Key List/Data tables or Datapool.", AR_EBADPARAM);
        return status;
    }

    status = FindTagKeyList(sg_tag_entry->tag_id, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to find Tag(0x%x)",
            AR_EBADPARAM, sg_tag_entry->tag_id);
        return status;
    }

    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of keys.", status);
        return status;
    }

    if (num_keys != 0)
    {
        status = FileManReadBuffer(&glb_buf_3[0],
            num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key ID List", status);
            return status;
        }
    }

    offset = ci_tag_data_lut_tbl.chunk_offset +
        sg_tag_entry->offset;
    status = FileManReadBuffer(&key_value_table_header,
        sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of values "
            "and lut entries.", status);
        return status;
    }

    if (num_keys != key_value_table_header.num_keys)
    {
        ACDB_ERR("Error[%d]: Number of keys and values are"
            " different!", AR_EFAILED);
        return AR_EFAILED;
    }
    //GLB_BUF_3 stores the key vector IDs followed by the Values
    //Ex. [K1 K2 K2 V1 V2 V3]
    key_id_list = &glb_buf_3[0];
    key_value_list = &glb_buf_3[num_keys];
    tkv = &glb_buf_3[num_keys * 2];

    for (uint32_t i = 0; i < key_value_table_header.num_entries; i++)
    {
        if (num_keys != 0)
        {
            status = FileManReadBuffer(key_value_list,
                num_keys * sizeof(uint32_t), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
                return status;
            }
        }

        //Skip Def and Dot offsets
        offset += 2 * sizeof(uint32_t);

        rsp->num_key_vectors++;
        rsp->list_size += (sizeof(sg_tag_entry->tag_id) + sizeof(num_keys)
            + num_keys * sizeof(AcdbKeyValuePair));

        if (op == ACDB_OP_GET_SIZE) continue;

        //Write Key Vector
        status = AcdbZip1(num_keys, key_id_list,
            num_keys, key_value_list,
            num_keys * 2, tkv);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to zip key id and"
                " key value list.", status);
            return status;
        }

        //Write Tag ID
        if (*blob_offset
            + sizeof(uint32_t) > expected_size)
        {
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
            &sg_tag_entry->tag_id, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        //Write num keys
        if (*blob_offset
            + sizeof(uint32_t) > expected_size)
        {
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
            &num_keys, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        if (num_keys == 0) continue;

        //Write Key Vector
        if (*blob_offset
            + num_keys * sizeof(AcdbKeyValuePair) > expected_size)
        {
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY((uint8_t*)rsp->key_vector_list + *blob_offset,
            tkv, num_keys * sizeof(AcdbKeyValuePair));
        *blob_offset += num_keys * sizeof(AcdbKeyValuePair);
    }

    ACDB_CLEAR_BUFFER(glb_buf_3);
    return status;
}

/**
* \brief
*		Builds a list of tag key vectors using the subgraphs defined in a graph
*
* \param[in] sg_list_offset: Offset of the subgraph list
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t BuildTagKeyVectorList(
    AcdbTagKeyVectorList *rsp, uint32_t sg_list_offset)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t data_pool_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t end_offset = 0;
    //The expected size of rps.key_vector_list set by client
    uint32_t expected_size = 0;
    ChunkInfo ci_data_pool = { 0 };
    ChunkInfo ci_tag_data_lut = { 0 };
    ChunkInfo ci_tag_key_tbl = { 0 };
    SgListHeader sg_list_header = { 0 };
    SgListObjHeader sg_obj = { 0 };
    SubgraphTagLutEntry sg_tag_entry = { 0 };

    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The input parameter rsp is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (!IsNull(rsp->key_vector_list))
    {
        expected_size = rsp->list_size;
        rsp->list_size = 0;
        rsp->num_key_vectors = 0;
    }

    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;
    ci_tag_data_lut.chunk_id = ACDB_CHUNKID_MODULE_TAG_KEY_LIST_LUT;
    ci_tag_key_tbl.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_KEYTABLE;
    status = ACDB_GET_CHUNK_INFO(&ci_data_pool,
        &ci_tag_data_lut, &ci_tag_key_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve chunk info for: "
            "data pool or tag data.", status);
        return status;
    }

    data_pool_offset = ci_data_pool.chunk_offset + sg_list_offset;
    status = FileManReadBuffer(&sg_list_header,
        sizeof(SgListHeader), &data_pool_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read the subgraph"
            " list header", status);
        return status;
    }

    // Loop through each subgraph and combine all the key vectors into a list
    for (uint32_t i = 0; i < sg_list_header.num_subgraphs; i++)
    {
        status = FileManReadBuffer(&sg_obj, sizeof(SgListObjHeader),
            &data_pool_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read the subgraph"
                " object header", status);
            return status;
        }

        sg_tag_entry.sg_id = sg_obj.subgraph_id;

        //Skip over subgraph destinations
        data_pool_offset += sg_obj.num_subgraphs * sizeof(uint32_t);

        status = TagKeyTableFindFirstOfSubgraphID(&sg_tag_entry, &offset);
        if (AR_FAILED(status) && status == AR_ENOTEXIST)
        {
            ACDB_DBG("Error[%d]: No module tags found in Subgraph(0x%x)."
                " Skipping..", status, sg_tag_entry.sg_id);
            status = AR_EOK;
            continue;
        }
        else if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Subgraph(0x%x) Tag entry"
                " in Module Tag Key Table.", status, sg_tag_entry.sg_id);
            return status;
        }

        end_offset = ci_tag_key_tbl.chunk_offset + ci_tag_key_tbl.chunk_size;
        while (offset < end_offset)
        {
            status = FileManReadBuffer(&sg_tag_entry,
                sizeof(SubgraphTagLutEntry), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read Subgraph Tag entry"
                    " in Module Tag Key Table.", status);
                return status;
            }

            if (sg_tag_entry.sg_id != sg_obj.subgraph_id)
                break;

            status = GetAllTKVVariations(&sg_tag_entry,
                &blob_offset, expected_size, rsp);
            if (AR_FAILED(status) && status != AR_ENOTEXIST)
            {
                ACDB_ERR("Error[%d]: Failed to get TKVs for "
                    "Subgraph(0x%x) & Tag(0x%x)", status);
                return status;
            }
        }
    }

    return status;
}

/**
* \brief
*		Retrieves a list of Tag or Calibration Key Vectors given a GKV
*
* \param[in] kv_type: the type of key vector to get data for
* \param[in] AcdbGraphKeyVector: the graph key vector to get keys vectors from
* \param[out] rsp: the Key Vector List to be populated
* \param[out] rsp_size: the size of the kv_list_rsp structure
*
* \return 0 on success, and non-zero on failure
*/
int32_t GetUsecaseKeyVectorList(KeyVectorType kv_type, AcdbGraphKeyVector *gkv,
    void *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    uint32_t gkv_lut_offset = 0;
    uint32_t sg_list_offset = 0;
    uint32_t sg_data_offset = 0;
    void *kv_list = NULL;
    AcdbGraphKeyVector graph_kv = { 0 };

    if (IsNull(gkv) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null:"
            " GKV or Response", status);
        return AR_EBADPARAM;
    }

    switch (kv_type)
    {
    case CAL_KEY_VECTOR:
        if (rsp_size < sizeof(AcdbKeyVectorList))
        {
            ACDB_ERR("Error[%d]: The size of the response structure is less "
                "than %d bytes", status, sizeof(AcdbKeyVectorList));
            return AR_EBADPARAM;
        }

        kv_list = ((AcdbKeyVectorList*)rsp)->key_vector_list;
        break;
    case TAG_KEY_VECTOR:
        if (rsp_size < sizeof(AcdbTagKeyVectorList))
        {
            ACDB_ERR("Error[%d]: The size of the response structure is less "
                "than %d bytes", status, sizeof(AcdbTagKeyVectorList));
            return AR_EBADPARAM;
        }

        kv_list = ((AcdbTagKeyVectorList*)rsp)->key_vector_list;
        break;
    default:
        break;
    }

    if (IsNull(kv_list))
    {
        /* Print only during the size querry,i.e the first time this
         * api is called */
        ACDB_INFO("Retrieving subgraph list for the following"
            " graph key vector:");
        LogKeyVector(gkv, GRAPH_KEY_VECTOR);
    }


    graph_kv.num_keys = gkv->num_keys;
    graph_kv.graph_key_vector = (AcdbKeyValuePair*)&glb_buf_3[0];
    ACDB_MEM_CPY_SAFE(&glb_buf_3[0], gkv->num_keys * sizeof(AcdbKeyValuePair),
        gkv->graph_key_vector, gkv->num_keys * sizeof(AcdbKeyValuePair));

    //Sort Key Vector by Key IDs(position 0 of AcdbKeyValuePair)
    status = AcdbSort2(gkv->num_keys * sizeof(AcdbKeyValuePair),
        graph_kv.graph_key_vector, sizeof(AcdbKeyValuePair), 0);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to sort graph key vector.", status);
        return status;
    }

    status = SearchGkvKeyTable(&graph_kv, &gkv_lut_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to find the graph key vector", status);
        return status;
    }

    status = SearchGkvLut(&graph_kv, gkv_lut_offset,
        &sg_list_offset, &sg_data_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to find the graph key vector", status);
        return status;
    }

    switch (kv_type)
    {
    case CAL_KEY_VECTOR:
        status = BuildCalKeyVectorList(
            (AcdbKeyVectorList*)rsp, sg_list_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to build calibration key vector list",
                status);
            return status;
        }
        break;
    case TAG_KEY_VECTOR:
        status = BuildTagKeyVectorList(
            (AcdbTagKeyVectorList*)rsp, sg_list_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to build tag key vector list",
                status);
            return status;
        }
        break;
    default:
        break;
    }

    return status;
}

int32_t AcdbCmdGetGraphCalKeyVectors(AcdbGraphKeyVector *gkv,
    AcdbKeyVectorList *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;

    status = GetUsecaseKeyVectorList(CAL_KEY_VECTOR, gkv, rsp, rsp_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: An error occured.", status);
        return status;
    }

    return status;
}

int32_t AcdbCmdGetGraphTagKeyVectors(AcdbGraphKeyVector *gkv,
    AcdbTagKeyVectorList *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;

    status = GetUsecaseKeyVectorList(TAG_KEY_VECTOR, gkv, rsp, rsp_size);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: An error occured.", status);
        return status;
    }

    return status;
}

/**
* \brief
*		Retrieves all the graph key vector combinations for a set of keys given
*       the offset of the Graph Key Value table
*
* \depends
*       GLB_BUF_3 is used to store gkv data
*
* \param[in] graph_keys: A list of graph Key IDs
* \param[in] gkv_lut_offset: An entry that holds the offset to the
*            Graph Key Value look up table
* \param[in/out] blob_offset: offset within the rsp.key_vector_list
* \param[in] expected_size: The expected size of rsp.key_vector_list
* \param[out] rsp: The Key Vector List to be populated
* \return 0 on success, and non-zero on failure
*/
int32_t GetAllGraphKvVariations(
    AcdbUintList *graph_keys, uint32_t gkv_lut_offset,
    uint32_t *blob_offset, uint32_t expected_size, AcdbKeyVectorList *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t value_list_size = 0;
    KeyTableHeader key_table_header = { 0 };
    ChunkInfo ci_key_value_table = { 0 };
    //AcdbOp op = ACDB_OP_NONE;
    uint32_t *key_value_list = NULL;
    uint32_t *gkv = NULL;

    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The input response parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_key_value_table.chunk_id = ACDB_CHUNKID_GKVLUTTBL;
    status = ACDB_GET_CHUNK_INFO(&ci_key_value_table);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Graph Key LUT Chunk.",
            status);
        return status;
    }

    offset = ci_key_value_table.chunk_offset + gkv_lut_offset;
    status = FileManReadBuffer(&key_table_header,
        sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read key table header.",
            status);
        return status;
    }

    if (graph_keys->count != key_table_header.num_keys)
    {
        ACDB_ERR("Error[%d]: There is a mismatch between the length "
            "of the input key id list and the number of keys in the GKV LUT.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    //Only includes size of value list
    value_list_size = key_table_header.num_keys
        * sizeof(uint32_t);
    /* Recalculate list_size when getting data
    num entries x (#keys + sizeof(KeyValuePairList)) */
    rsp->list_size += key_table_header.num_entries
        * ((uint32_t)sizeof(uint32_t) + 2UL * value_list_size);
    rsp->num_key_vectors += key_table_header.num_entries;

    if (IsNull(rsp->key_vector_list)) return status;

    key_value_list = &glb_buf_3[0];
    gkv = &glb_buf_3[key_table_header.num_keys];

    for (uint32_t i = 0; i < key_table_header.num_entries; i++)
    {
        //read value list
        status = FileManReadBuffer(key_value_list,
            key_table_header.num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
            return status;
        }

        //Skip SG List and Data offsets
        offset += 2 * sizeof(uint32_t);

        status = AcdbZip1(key_table_header.num_keys, graph_keys->list,
            key_table_header.num_keys, key_value_list,
            key_table_header.num_keys * 2, gkv);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to zip key id and"
                " key value list.", status);
            return status;
        }

        //Write key vector to buffer

        // Write num keys
        if (*blob_offset + sizeof(uint32_t) > expected_size)
        {
            ACDB_ERR("Error[%d]: Unable to write number of keys. "
                "The actual key vector list size is greater than"
                " the expected size %d bytes.",
                AR_ENEEDMORE, expected_size);
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY_SAFE(rsp->key_vector_list + *blob_offset, sizeof(uint32_t),
            &graph_keys->count, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        // Write key vector
        if (*blob_offset + 2 * value_list_size > expected_size)
        {
            ACDB_ERR("Error[%d]: Unable to write key vector. "
                "The actual key vector list size is greater than"
                " the expected size %d bytes.",
                AR_ENEEDMORE, expected_size);
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY_SAFE(
            rsp->key_vector_list + *blob_offset, 2UL * (size_t)value_list_size,
            gkv                                , 2UL * (size_t)value_list_size);

        *blob_offset += 2 * value_list_size;
    }

    return status;
}

int32_t AcdbCmdGetSupportedGraphKeyVectors(AcdbUintList *key_id_list,
    AcdbKeyVectorList *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t num_key_tables = 0;
    uint32_t key_table_size = 0;
    uint32_t key_table_entry_size = 0;
    uint32_t num_gkvs_found = 0;
    uint32_t num_keys_found = 0;
    uint32_t entry_index = 0;
    uint32_t blob_offset = 0;
    uint32_t expected_size = 0;//Expected size of rsp.key_vector_list
    ChunkInfo ci_key_id_table = { 0 };
    KeyTableHeader key_table_header = { 0 };
    AcdbUintList graph_keys = { 0 };

    if (IsNull(key_id_list) || IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The gkv input parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (rsp_size < sizeof(AcdbKeyVectorList))
    {
        ACDB_ERR("Error[%d]: The response structure size is less than %d",
            AR_EBADPARAM, sizeof(AcdbKeyVectorList));
        return AR_EBADPARAM;
    }

    if (rsp->list_size == 0 && !IsNull(rsp->key_vector_list))
    {
        ACDB_ERR("Error[%d]: The key vector list size is zero, "
            "but the key vector list is not null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (!IsNull(rsp->key_vector_list))
    {
        //Recalculate list size and #kvs when gettig data
        expected_size = rsp->list_size;
        rsp->list_size = 0;
        rsp->num_key_vectors = 0;
    }

    /* GLB_BUF_2 stores a graph key table entry [key1,key2,... lutOffset]
     * graph_keys will point to the key id list
     */
    graph_keys.list = &glb_buf_2[0];

    ci_key_id_table.chunk_id = ACDB_CHUNKID_GKVKEYTBL;
    status = ACDB_GET_CHUNK_INFO(&ci_key_id_table);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to retrieve Graph Key ID Chunk.",
            status);
        return status;
    }

    offset = ci_key_id_table.chunk_offset;
    status = FileManReadBuffer(&num_key_tables, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read number of GKV Key ID tables.",
            status);
        return status;
    }

    for (uint32_t i = 0; i < num_key_tables; i++)
    {
        status = FileManReadBuffer(&key_table_header,
            sizeof(KeyTableHeader), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to read key table header.",
                status);
            return status;
        }

        key_table_entry_size = key_table_header.num_keys
            * sizeof(uint32_t)
            + sizeof(uint32_t);
        key_table_size = key_table_header.num_entries
            * key_table_entry_size;
        graph_keys.count = key_table_header.num_keys;

        /* If there are less keys than the provided keys, the GKV
         * does not support the capability(ies) that are needed */
        if (key_table_header.num_keys < key_id_list->count)
        {
            offset += key_table_size;
            continue;
        }
        else
        {
            //Read key vector and see if it contains the provided keys

            for (uint32_t k = 0; k < key_table_header.num_entries; k++)
            {
                num_keys_found = 0;

                //Read Graph Keys
                status = FileManReadBuffer(&glb_buf_2, key_table_entry_size, &offset);
                if (AR_FAILED(status))
                {
                    ACDB_ERR("Error[%d]: Unable to read number of GKV Key ID tables.",
                        status);
                    return status;
                }

                //Contains input Key(s)?
                for (uint32_t j = 0; j < key_id_list->count; j++)
                {
                    if (AR_SUCCEEDED(AcdbDataBinarySearch2(
                        graph_keys.list,
                        key_table_header.num_keys * sizeof(uint32_t),
                        &key_id_list[j], 1, 1, &entry_index)))
                    {
                        num_keys_found++;
                    }
                }

                if (num_keys_found == key_id_list->count)
                {
                    //Go to LUT and collect all the Key Combinations
                    status = GetAllGraphKvVariations(
                        &graph_keys, glb_buf_2[graph_keys.count],
                        &blob_offset, expected_size, rsp);
                    if (AR_FAILED(status))
                    {
                        ACDB_ERR("Error[%d]: Failed to get all GKV "
                            "variations for:",
                            status);

                        LogKeyIDs(&graph_keys, GRAPH_KEY_VECTOR);
                        return status;
                    }

                    num_gkvs_found++;
                }
            }
        }
    }

    if (!num_gkvs_found)
    {
        ACDB_ERR("Error[%d]: Failed to find data. No graph key vector supports"
            " all the provided keys:",AR_ENOTEXIST);

        for (uint32_t i = 0; i < key_id_list->count; i++)
        {
            ACDB_ERR("Graph Key: 0x%x", key_id_list->list[i]);
        }
        return AR_ENOTEXIST;
    }

    return status;
}

/**
* \brief
*		Retrieves all the driver key vector combinations for a set of
*       keys given the offset of the Key and Value tables
*
* \depends
*       GLB_BUF_3 is used to store kv data
*
* \param[in] cal_key_table_entry: An entry that holds the offsets to the
*            Key ID and Key Value tables
* \param[in/out] blob_offset: current offset in the rsp
* \param[in] expected_size: the expected size of the rsp.list_size
* \param[out] rsp: The Key Vector List to be populated
*
* \return 0 on success, and non-zero on failure
*/
int32_t GetAllDriverKvVariations(
    GslCalLutEntry *cal_lut_entry, uint32_t *blob_offset,
    uint32_t expected_size, AcdbKeyVectorList *rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t value_list_size = 0;
    uint32_t num_keys = 0;
    KeyTableHeader key_table_header = { 0 };
    //AcdbOp op = ACDB_OP_NONE;
    uint32_t *key_list = NULL;
    uint32_t *value_list = NULL;
    uint32_t *driver_kv = NULL;
    ChunkInfo gsl_cal_key_tbl = { ACDB_CHUNKID_GSL_CALKEY_TBL, 0, 0 };
    ChunkInfo gsl_cal_lut_tbl = { ACDB_CHUNKID_GSL_CALDATA_TBL, 0, 0 };

    status = ACDB_GET_CHUNK_INFO(&gsl_cal_key_tbl, &gsl_cal_lut_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get GSL Key Table or"
            " GSL Value Table Chunk", status);
        return status;
    }

    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The input response parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    offset = gsl_cal_key_tbl.chunk_offset
        + cal_lut_entry->offset_calkey_table;

    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of keys.", status);
        return status;
    }

    if (num_keys != 0)
    {
        status = FileManReadBuffer(&glb_buf_3[0],
            num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key ID List", status);
            return status;
        }
    }

    offset = gsl_cal_lut_tbl.chunk_offset
        + cal_lut_entry->offset_caldata_table;
    status = FileManReadBuffer(&key_table_header,
        sizeof(KeyTableHeader), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to read key table header.",
            status);
        return status;
    }

    if (num_keys != key_table_header.num_keys)
    {
        ACDB_ERR("Error[%d]: There is a mismatch between the length "
            "of the key id list and the number of keys in the "
            "Driver Key LUT.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    //Only includes size of value list
    value_list_size = key_table_header.num_keys
        * sizeof(uint32_t);
    /* Recalculate list_size when getting data
    num entries x (#keys + sizeof(KeyValuePairList)) */
    rsp->list_size += key_table_header.num_entries
        * ((uint32_t)sizeof(uint32_t) + 2UL * value_list_size);
    rsp->num_key_vectors += key_table_header.num_entries;

    if (IsNull(rsp->key_vector_list)) return status;

    key_list = &glb_buf_3[0];
    value_list = &glb_buf_3[key_table_header.num_keys];
    driver_kv = &glb_buf_3[2 * key_table_header.num_keys];

    for (uint32_t i = 0; i < key_table_header.num_entries; i++)
    {
        //Read value list
        status = FileManReadBuffer(value_list,
            key_table_header.num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
            return status;
        }

        //Skip SG List and Data offsets
        offset += 2 * sizeof(uint32_t);

        status = AcdbZip1(key_table_header.num_keys, key_list,
            key_table_header.num_keys, value_list,
            key_table_header.num_keys * 2, driver_kv);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to zip key id and"
                " key value list.", status);
            return status;
        }

        //Write key vector to buffer

        // Write num keys
        if (*blob_offset + sizeof(uint32_t) > expected_size)
        {
            ACDB_ERR("Error[%d]: Unable to write number of keys. "
                "The actual key vector list size is greater than"
                " the expected size %d bytes.",
                AR_ENEEDMORE, expected_size);
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY_SAFE(rsp->key_vector_list + *blob_offset, sizeof(uint32_t),
            &num_keys, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        // Write key vector
        if (*blob_offset + 2 * value_list_size > expected_size)
        {
            ACDB_ERR("Error[%d]: Unable to write key vector. "
                "The actual key vector list size is greater than"
                " the expected size %d bytes.",
                AR_ENEEDMORE, expected_size);
            return AR_ENEEDMORE;
        }

        ACDB_MEM_CPY_SAFE(
            rsp->key_vector_list + *blob_offset, 2UL * (size_t)value_list_size,
            driver_kv                          , 2UL * (size_t)value_list_size);
        *blob_offset += 2 * value_list_size;
    }

    return status;
}

int32_t AcdbCmdGetDriverModuleKeyVectors(
    uint32_t module_id, AcdbKeyVectorList *rsp, uint32_t rsp_size)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t blob_offset = 0;
    uint32_t expected_size = 0;
    uint32_t end_gsl_cal_lut_offset = 0;
    GslCalLutEntry cal_lut_entry = { 0 };
    ChunkInfo gsl_cal_lut = { ACDB_CHUNKID_GSL_CAL_LUT, 0, 0 };

    if (IsNull(rsp))
    {
        ACDB_ERR("Error[%d]: The input response parameter is null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    if (rsp_size < sizeof(AcdbKeyVectorList))
    {
        ACDB_ERR("Error[%d]: The response structure size is less than %d",
            AR_EBADPARAM, sizeof(AcdbKeyVectorList));
        return AR_EBADPARAM;
    }

    if (rsp->list_size == 0 && !IsNull(rsp->key_vector_list))
    {
        ACDB_ERR("Error[%d]: The key vector list size is zero, "
            "but the key vector list is not null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&gsl_cal_lut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get GSL Cal LUT Chunk", status);
        return status;
    }

    if (!IsNull(rsp->key_vector_list))
    {
        //Recalculate list size and #kvs when gettig data
        expected_size = rsp->list_size;
        rsp->list_size = 0;
        rsp->num_key_vectors = 0;
    }

    /* Algorithm
    * 1. Find the first occurance of the Module ID in the GSL Cal LUT
    * 2. Use the offset of that entry to find remaining
    *   <Module ID, Key Table, Value table> entries
    * 4. zip key id list and value list and write to reponse buffer
    */
    cal_lut_entry.mid = module_id;

    status = DriverDataFindFirstOfModuleID(&cal_lut_entry, &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: No key vectors found for Driver Module(0x%x)"
            , status, module_id);
        return status;
    }

    end_gsl_cal_lut_offset = gsl_cal_lut.chunk_offset
        + gsl_cal_lut.chunk_size;

    while(offset < end_gsl_cal_lut_offset)
    {
        status = FileManReadBuffer(&cal_lut_entry, sizeof(GslCalLutEntry), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read GSL Cal LUT entry"
                " of entries.", status);
            return status;
        }

        if (cal_lut_entry.mid != module_id) break;

        status = GetAllDriverKvVariations(
            &cal_lut_entry, &blob_offset, expected_size, rsp);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to get all key vector variations."
                , status);
            return status;
        }
    }

    return status;
}

int32_t SearchSubgraphCalLut(
    uint32_t req_subgraph_id, uint32_t sz_ckv_entry_list,
    AcdbCalKeyTblEntry *ckv_entry_list, uint32_t *num_entries)
{
    int32_t status = AR_EOK;
    bool_t sg_found = FALSE;
    uint32_t num_subgraphs = 0;
    uint32_t num_ckv_tbl_entries = 0;
    uint32_t subgraph_id = 0;
    uint32_t offset = 0;
    ChunkInfo ci_calsglut = { 0 };

    if (IsNull(ckv_entry_list))
    {
        ACDB_ERR("Error[%d]: The provided input parameter(s) are null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    //Search for Persist/Non-persist/Global-persist caldata in ACDB files

    ci_calsglut.chunk_id = ACDB_CHUNKID_CALSGLUT;

    status = AcdbGetChunkInfo(ci_calsglut.chunk_id,
        &ci_calsglut.chunk_offset, &ci_calsglut.chunk_size);
    if (AR_FAILED(status)) return status;

    offset = ci_calsglut.chunk_offset;
    status = FileManReadBuffer(&num_subgraphs, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of subgraphs.", status);
        return status;
    }

    for (uint32_t i = 0; i < num_subgraphs; i++)
    {
        status = FileManReadBuffer(&glb_buf_3[0], 2 * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read subgraph and number of entries.", status);
            return status;
        }

        subgraph_id = glb_buf_3[0];
        num_ckv_tbl_entries = glb_buf_3[1];

        if (subgraph_id == req_subgraph_id)
        {
            sg_found = TRUE;
            //Read number of <Cal Key ID List Offset, Cal Key Value List Offset> Entries
            *num_entries = num_ckv_tbl_entries;

            if (num_ckv_tbl_entries == 0)
            {
                ACDB_CLEAR_BUFFER(glb_buf_3);
                ACDB_ERR("Error[%d]: Subgraph(%x) does not have CKV table entries.",
                    AR_ENOTEXIST, subgraph_id);
                return AR_ENOTEXIST;
            }

            if (sz_ckv_entry_list < num_ckv_tbl_entries * sizeof(uint32_t))
            {
                return AR_ENEEDMORE;
            }

            status = FileManReadBuffer(ckv_entry_list,
                num_ckv_tbl_entries * sizeof(AcdbCalKeyTblEntry), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read CKV table entries for Subgraph(%x)",
                    status, subgraph_id);
                return status;
            }

            break;
        }
        else
        {
            //List<CalKeyTable Off., LUT Off.>
            offset += 2 * sizeof(uint32_t) * num_ckv_tbl_entries;
        }
    }

    if (!sg_found)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: Could not find calibration data for Subgraph(%x)",
            status, req_subgraph_id);
    }

    ACDB_CLEAR_BUFFER(glb_buf_3);

    return status;
}

int32_t SearchSubgraphCalLut2(AcdbSgCalLutHeader *sg_lut_entry_header,
    uint32_t *ckv_entry_list_offset)
{
    int32_t status = AR_EOK;
    bool_t sg_found = FALSE;
    uint32_t num_subgraphs = 0;
    uint32_t offset = 0;
    ChunkInfo ci_calsglut = { 0 };
    AcdbSgCalLutHeader entry_header = { 0 };

    if (IsNull(sg_lut_entry_header) || IsNull(ckv_entry_list_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameter(s) are null.",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_calsglut.chunk_id = ACDB_CHUNKID_CALSGLUT;
    status = ACDB_GET_CHUNK_INFO(&ci_calsglut);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Unable to get Subgraph Calibration"
            " Lookup table chunk info.");
        return status;
    }

    offset = ci_calsglut.chunk_offset;
    status = FileManReadBuffer(&num_subgraphs, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of subgraphs.", status);
        return status;
    }

    for (uint32_t i = 0; i < num_subgraphs; i++)
    {
        status = FileManReadBuffer(&entry_header,
            sizeof(AcdbSgCalLutHeader), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read subgraph lookup"
                " table entry header.", status);
            return status;
        }

        if (entry_header.subgraph_id == sg_lut_entry_header->subgraph_id)
        {
            sg_found = TRUE;

            if (entry_header.num_ckv_entries == 0)
            {
                ACDB_CLEAR_BUFFER(glb_buf_3);
                ACDB_ERR("Error[%d]: Subgraph(0x%x) does not"
                    " have CKV table entries.",
                    AR_ENOTEXIST, entry_header.subgraph_id);
                return AR_ENOTEXIST;
            }
            sg_lut_entry_header->num_ckv_entries =
                entry_header.num_ckv_entries;
            *ckv_entry_list_offset = offset;
            break;
        }
        else
        {
            //List<CalKeyTable Off., LUT Off.>
            offset += 2 * sizeof(uint32_t)
                * entry_header.num_ckv_entries;
        }
    }

    if (!sg_found)
    {
        status = AR_ENOTEXIST;
        ACDB_ERR("Error[%d]: Unable to find calibration data for Subgraph(0x%x)",
            status, sg_lut_entry_header->subgraph_id);
    }

    return status;
}

/**
* \brief
*		Matches the requeset CKV with the Key ID and Key Value Tables. Returns
*       the offsets to the DEF, DOT(used for non-persistent/persistent and
*       DOT2(used for global-persistent) tables
* \param[in] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in] offset_key_table: Offset of Key ID table
* \param[in] offset_cal_lut: Offset of Key Value LUT
* \param[out] ckv_lut_entry: Offsets to DEF, DOT, and DOT2 for the input request
* \return 0 on success, and non-zero on failure
*/
int32_t SearchForMatchingCalKeyVector(AcdbParameterCalData *req,
    uint32_t offset_key_table, uint32_t offset_cal_lut,
    AcdbCkvLutEntryOffsets* ckv_lut_entry)
{
    int32_t status = AR_EOK;
    bool_t found = FALSE;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    uint32_t num_values = 0;
    uint32_t num_lut_entries = 0;
    ChunkInfo ci_calkeytbl = { 0 };
    ChunkInfo ci_callut = { 0 };

    if (IsNull(req) || IsNull(ckv_lut_entry))
    {
        ACDB_ERR("The input request, DEF offset, or DOT offset are null");
        return AR_EBADPARAM;
    }

    ci_calkeytbl.chunk_id = ACDB_CHUNKID_CALKEYTBL;
    ci_callut.chunk_id = ACDB_CHUNKID_CALDATALUT;

    status = AcdbGetChunkInfo(ci_calkeytbl.chunk_id,
        &ci_calkeytbl.chunk_offset, &ci_calkeytbl.chunk_size);
    if (AR_FAILED(status)) return status;

    status = AcdbGetChunkInfo(ci_callut.chunk_id,
        &ci_callut.chunk_offset, &ci_callut.chunk_size);
    if (AR_FAILED(status)) return status;

    //Get Key List
    offset = ci_calkeytbl.chunk_offset + offset_key_table;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of keys.", status);
        return status;
    }

    if (req->cal_key_vector.num_keys != num_keys)
    {
        return AR_ENOTEXIST;
    }

    if (num_keys != 0)
    {
        status = FileManReadBuffer(&glb_buf_3[0], num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key ID List", status);
            return status;
        }

        //Sort Key Vector by Key IDs(position 0 of AcdbKeyValuePair)
        status = AcdbSort2(num_keys * sizeof(AcdbKeyValuePair),
            req->cal_key_vector.graph_key_vector, sizeof(AcdbKeyValuePair), 0);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to sort input request key vector.", status);
            return status;
        }

        if (!CompareKeyVectorIds(req->cal_key_vector, glb_buf_2, glb_buf_3, num_keys))
        {
            return AR_ENOTEXIST;
        }
    }

    offset = ci_callut.chunk_offset + offset_cal_lut;
    status = FileManReadBuffer(&glb_buf_3[0], 2 * sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of values and lut entries.", status);
        return status;
    }

    num_values = glb_buf_3[0];
    num_lut_entries = glb_buf_3[1];

    if (num_keys != num_values)
    {
        ACDB_ERR("Error[%d]: Number of keys and values are different!", AR_EFAILED);
        return AR_EFAILED;
    }

    //Compare CKV Values and get
    for (uint32_t i = 0; i < num_lut_entries; i++)
    {
        if (num_values == 0)
        {
            found = TRUE;
            status = FileManReadBuffer(ckv_lut_entry, sizeof(AcdbCkvLutEntryOffsets), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read ckv lut entry.", status);
                return status;
            }

            break;
        }

        status = FileManReadBuffer(&glb_buf_3[0], num_values * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read Key Value List.", status);
            return status;
        }

        if (CompareKeyVectorValues(req->cal_key_vector, glb_buf_2, glb_buf_3, num_keys))
        {
            found = TRUE;
            status = FileManReadBuffer(ckv_lut_entry, sizeof(AcdbCkvLutEntryOffsets), &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read ckv lut entry offsets.", status);
                return status;
            }

            break;
        }
        else
        {
            //<CKV.Values, DEF, DOT, DOT2>
            offset += num_values * sizeof(uint32_t) + sizeof(AcdbCkvLutEntryOffsets);
        }
    }

    if (!found)
    {
        ACDB_ERR("Error[%d]: No matching calibration key vector was found", AR_ENOTEXIST);
        status = AR_ENOTEXIST;
    }

    ACDB_CLEAR_BUFFER(glb_buf_2);
    ACDB_CLEAR_BUFFER(glb_buf_3);
    return status;
}

/**
* \brief
*		Retrieve calibration data for a parameter from the data pool
* \param[int] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in/out] rsp: A data blob containg calibration data
* \param[in] offset_pair: calibration data def and dot offsets
* \return 0 on success, and non-zero on failure
*/
int32_t BuildParameterCalDataBlob(AcdbParameterCalData *req, AcdbBlob *rsp,
    AcdbDefDotPair *offset_pair)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t param_size = 0;
    uint32_t error_code = 0;
    uint32_t param_dot_offset = 0;
    uint32_t data_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t num_iid_pid_entries = 0;
    uint32_t entry_index = 0;
    ChunkInfo ci_caldef = { 0 };
    ChunkInfo ci_caldot = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbModIIDParamIDPair iid_pid_pair = { 0 };

    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("The input request, or response are null");
        return AR_EBADPARAM;
    }

    ci_caldef.chunk_id = ACDB_CHUNKID_CALDATADEF;
    ci_caldot.chunk_id = ACDB_CHUNKID_CALDATADOT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;

    status = AcdbGetChunkInfo(ci_caldef.chunk_id,
        &ci_caldef.chunk_offset, &ci_caldef.chunk_size);
    if (AR_FAILED(status)) return status;

    status = AcdbGetChunkInfo(ci_caldot.chunk_id,
        &ci_caldot.chunk_offset, &ci_caldot.chunk_size);
    if (AR_FAILED(status)) return status;

    status = AcdbGetChunkInfo(ci_data_pool.chunk_id,
        &ci_data_pool.chunk_offset, &ci_data_pool.chunk_size);
    if (AR_FAILED(status)) return status;

    offset = ci_caldef.chunk_offset + offset_pair->offset_def;

    status = FileManReadBuffer(&num_iid_pid_entries, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of <MID, PID> entries.", status);
        return status;
    }

    status = FileManReadBuffer(&glb_buf_3[0], num_iid_pid_entries * sizeof(AcdbModIIDParamIDPair), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read List of <MID, PID>.", status);
        return status;
    }

    iid_pid_pair.module_iid = req->module_iid;
    iid_pid_pair.parameter_id = req->parameter_id;

    if (SEARCH_ERROR == AcdbDataBinarySearch2((void*)&glb_buf_3,
        num_iid_pid_entries * sizeof(AcdbModIIDParamIDPair),
        &iid_pid_pair, 2,
        (int32_t)(sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)),
        &entry_index))
    {
        //MID PID not found
        return AR_ENOTEXIST;
    }
    else
    {
        offset = ci_caldot.chunk_offset + offset_pair->offset_dot;

        /* Calculation for DOT offset of <MID, PID> pair
        *
        * Offset of  +  Number of  +  Offset of the desired DOT entry based on
        *    DOT       DOT entries           index of <MID,PID> in DEF
        */
        param_dot_offset = offset + sizeof(uint32_t) + sizeof(uint32_t) * (uint32_t)(
            entry_index / (sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)));

        status = FileManReadBuffer(&data_offset, sizeof(uint32_t), &param_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read data pool offset from DOT.", status);
            return status;
        }

        offset = ci_data_pool.chunk_offset + data_offset;

        status = FileManReadBuffer(&param_size, sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read param size from data pool.", status);
            return status;
        }

        rsp->buf_size = sizeof(AcdbModIIDParamIDPair) + 2 * sizeof(uint32_t) + param_size;

        //Write Module IID, and Parameter ID
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(AcdbModIIDParamIDPair),
            &iid_pid_pair, sizeof(AcdbModIIDParamIDPair));
        blob_offset += sizeof(AcdbModIIDParamIDPair);

        //Write Parameter Size
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
            &param_size, sizeof(uint32_t));
        blob_offset += sizeof(uint32_t);

        //Write Error code
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
            &error_code, sizeof(uint32_t));
        blob_offset += sizeof(uint32_t);

        //Write Payload
        if (param_size != 0)
        {
            status = FileManReadBuffer((uint8_t*)rsp->buf + blob_offset,
                param_size, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read param payload of %d bytes.",
                    status, param_size);
                return status;
            }
        }
    }

    return status;
}

int32_t SearchForMatchingVoiceCalKeyValues(AcdbParameterCalData *req,
    AcdbVcpmCalDataObj *cal_data_obj, uint32_t *table_offset)
{
    int32_t status = AR_EOK;
    uint32_t file_offset = *table_offset;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    ChunkInfo ci_vcpm_cal_lut = { 0 };

    if (IsNull(req) || IsNull(cal_data_obj) || IsNull(table_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_vcpm_cal_lut.chunk_id = ACDB_CHUNKID_VCPM_CALDATA_LUT;

    status = ACDB_GET_CHUNK_INFO(&ci_vcpm_cal_lut);
    if (AR_FAILED(status)) return status;

    status = FileManReadBuffer(cal_data_obj,
        sizeof(AcdbVcpmCalDataObj), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Cal "
            "Data Object Header.", status);
        return status;
    }

    offset = ci_vcpm_cal_lut.chunk_offset + cal_data_obj->offset_vcpm_ckv_lut;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of voice keys.", status);
        return status;
    }

    if (num_keys != req->cal_key_vector.num_keys)
    {
        //Go to Next Cal Data Obj
        *table_offset = file_offset
            + (cal_data_obj->num_data_offsets * sizeof(uint32_t));
        return AR_ENOTEXIST;
    }

    if (num_keys != 0)
    {
        offset += sizeof(uint32_t);//num_entries is always 1
        status = FileManReadBuffer(&glb_buf_1, num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to Voice Key IDs.", status);
            return status;
        }

        if (FALSE == CompareKeyVectorValues(
            req->cal_key_vector, glb_buf_3, glb_buf_1, num_keys))
        {
            //Go to Next Cal Data Obj
            *table_offset = file_offset
                + (cal_data_obj->num_data_offsets * sizeof(uint32_t));
            return AR_ENOTEXIST;
        }
    }

    *table_offset = file_offset;

    return status;
}

int32_t SearchForMatchingVoiceCalKeyIDs(AcdbParameterCalData *req,
    AcdbVcpmCkvDataTable *cal_data_table, uint32_t *table_offset)
{
    int32_t status = AR_EOK;
    uint32_t file_offset = *table_offset;
    uint32_t offset = 0;
    uint32_t num_keys = 0;
    ChunkInfo ci_vcpm_cal_key = { 0 };

    if (IsNull(req) || IsNull(cal_data_table) || IsNull(table_offset))
    {
        ACDB_ERR("Error[%d]: One or more input parameters are null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_vcpm_cal_key.chunk_id = ACDB_CHUNKID_VCPM_CALKEY_TABLE;

    status = ACDB_GET_CHUNK_INFO(&ci_vcpm_cal_key);
    if (AR_FAILED(status)) return status;

    status = FileManReadBuffer(cal_data_table,
        sizeof(AcdbVcpmCkvDataTable), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read CKV "
            "Data Table Header.", status);
        return status;
    }

    offset = ci_vcpm_cal_key.chunk_offset +
        cal_data_table->offset_voice_key_table;
    status = FileManReadBuffer(&num_keys, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of voice keys.", status);
        return status;
    }

    if (num_keys != req->cal_key_vector.num_keys)
    {
        //Go to Next CKV Data Table
        *table_offset = file_offset
            + sizeof(uint32_t) //table size
            + cal_data_table->table_size
            - sizeof(AcdbVcpmCkvDataTable);
        return AR_ENOTEXIST;
    }

    if (num_keys != 0)
    {
        status = FileManReadBuffer(&glb_buf_1, num_keys * sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to Voice Key IDs.", status);
            return status;
        }

        if (FALSE == CompareKeyVectorIds(
            req->cal_key_vector, glb_buf_3, glb_buf_1, num_keys))
        {
            //Go to Next CKV Data Table
            *table_offset = file_offset
                + sizeof(uint32_t) //table size
                + cal_data_table->table_size
                - sizeof(AcdbVcpmCkvDataTable);
            return AR_ENOTEXIST;
        }
    }

    *table_offset = file_offset;

    return status;
}

int32_t GetVoiceParameterCalData(AcdbParameterCalData *req, AcdbBlob* rsp)
{
    int32_t status = AR_EOK;
    uint32_t i = 0;
    uint32_t file_offset = 0;
    uint32_t blob_offset = 0;
    uint32_t param_size = 0;
    uint32_t error_code = 0;
    uint32_t id_pair_table_size = 0;
    uint32_t data_pool_offset = 0;
    uint32_t data_pool_offset_index = 0;
    ChunkInfo ci_vcpm_cal = { 0 };
    ChunkInfo ci_vcpm_cal_key = { 0 };
    ChunkInfo ci_vcpm_cal_lut = { 0 };
    ChunkInfo ci_vcpm_cal_def = { 0 };
    ChunkInfo ci_data_pool = { 0 };
    AcdbVcpmSubgraphCalHeader subgraph_cal_header = { 0 };
    AcdbVcpmSubgraphCalTable subgraph_cal_table_header = { 0 };
    AcdbVcpmCkvDataTable cal_data_table = { 0 };
    AcdbVcpmCalDataObj cal_data_obj = { 0 };
    AcdbMiidPidPair id_pair = { 0 };
    AcdbTableInfo table_info = { 0 };
    AcdbPartitionSearchInfo search_info = { 0 };

    ci_vcpm_cal.chunk_id = ACDB_CHUNKID_VCPM_CAL_DATA;
    ci_vcpm_cal_key.chunk_id = ACDB_CHUNKID_VCPM_CALKEY_TABLE;
    ci_vcpm_cal_lut.chunk_id = ACDB_CHUNKID_VCPM_CALDATA_LUT;
    ci_vcpm_cal_def.chunk_id = ACDB_CHUNKID_VCPM_CALDATA_DEF;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;

    status = ACDB_GET_CHUNK_INFO(
        &ci_vcpm_cal,       &ci_vcpm_cal_lut,
        &ci_vcpm_cal_key,   &ci_vcpm_cal_def,
        &ci_data_pool);
    if (AR_FAILED(status)) return status;

    file_offset = ci_vcpm_cal.chunk_offset;
    status = FileManReadBuffer(&subgraph_cal_header,
        sizeof(AcdbVcpmSubgraphCalHeader), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read Module Header.", status);
        return status;
    }

    //Search for the matching VCPM Subgraph Cal Data Table
    status = GetVcpmSubgraphCalTable(subgraph_cal_header.num_subgraphs,
        req->subgraph_id, &file_offset, &subgraph_cal_table_header);
    if (AR_FAILED(status))
    {
        return status;
    }

    //Search for matching key vector then find iid,pid and get voice cal data
    for (i = 0; i < subgraph_cal_table_header.num_ckv_data_table; i++)
    {
        status = SearchForMatchingVoiceCalKeyIDs(
            req, &cal_data_table, &file_offset);

        if (AR_FAILED(status) && status != AR_ENOTEXIST)
            return status;
        else if (AR_SUCCEEDED(status)) break;
    }

    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to find matching key vector.", status);
        return status;
    }

    for (i = 0; i < cal_data_table.num_caldata_obj; i++)
    {
        status = SearchForMatchingVoiceCalKeyValues(
            req, &cal_data_obj, &file_offset);

        if (AR_FAILED(status) && status != AR_ENOTEXIST)
            return status;
        else if (AR_SUCCEEDED(status)) break;
    }

    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to find matching key vector.", status);
        return status;
    }

    /* Binary Search through <IID PID> to find index of <IID PID>. Use the
     * index to calculate the Cal Data Object data pool offset get to the
     * data pool offset */
    id_pair_table_size =
        cal_data_obj.num_data_offsets * sizeof(AcdbMiidPidPair);

    id_pair.module_iid = req->module_iid;
    id_pair.parameter_id = req->parameter_id;

    //Setup Table Information
    table_info.table_offset = ci_vcpm_cal_def.chunk_offset +
        cal_data_obj.offset_cal_def + sizeof(uint32_t);//Skip num id pairs
    table_info.table_size = id_pair_table_size;
    table_info.table_entry_size = sizeof(AcdbMiidPidPair);

    //Setup Search Information
    search_info.num_search_keys = 2;//IID, PID
    search_info.num_structure_elements =
        (sizeof(AcdbMiidPidPair) / sizeof(uint32_t));
    search_info.table_entry_struct = &id_pair;
    search_info.scratch_space.buf = &glb_buf_1;
    search_info.scratch_space.buf_size = sizeof(glb_buf_1);

    status = AcdbPartitionBinarySearch(&table_info, &search_info);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to find Module Instance ID(0x%x) "
            "Parameter ID(0x%x)", id_pair.module_iid, id_pair.parameter_id);
        return status;
    }

    data_pool_offset_index = search_info.entry_index;
    file_offset += data_pool_offset_index * sizeof(uint32_t);

    //Get parameter data from the data pool
    status = FileManReadBuffer(&data_pool_offset,
        sizeof(uint32_t), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read global "
            "data pool offset.", status);
        return status;
    }

    file_offset = ci_data_pool.chunk_offset + data_pool_offset;
    status = FileManReadBuffer(&param_size, sizeof(uint32_t), &file_offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read parameter size.", status);
        return status;
    }

    rsp->buf_size = sizeof(AcdbMiidPidPair) +
        2 * sizeof(uint32_t) + param_size;

    //Write Module IID, and Parameter ID
    ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset,
        sizeof(AcdbMiidPidPair), &id_pair, sizeof(AcdbMiidPidPair));
    blob_offset += sizeof(AcdbMiidPidPair);

    //Write Parameter Size
    ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
        &param_size, sizeof(uint32_t));
    blob_offset += sizeof(uint32_t);

    //Write Error code
    ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
        &error_code, sizeof(uint32_t));
    blob_offset += sizeof(uint32_t);

    if (param_size != 0)
    {
        status = FileManReadBuffer((uint8_t*)rsp->buf + blob_offset,
            param_size, &file_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read parameter payload.", status);
            return status;
        }
        blob_offset += param_size;
    }

    return status;
}

/**
* \brief
*		Get calibration data for one parameter
* \param[int] req: Request containing the subgraph, module instance, parameter, and CKV
* \param[in/out] rsp: A data blob containg calibration data
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbGetParameterCalData(AcdbParameterCalData *req, AcdbBlob* rsp)
{
    int32_t status = AR_EOK;
    AcdbCalKeyTblEntry *cal_key_entry_list = NULL;
    AcdbCkvLutEntryOffsets ckv_lut_entry = { 0 };
    uint32_t num_entries = 0;
    if (IsNull(req) || IsNull(rsp))
    {
        ACDB_ERR("The input request and/or response is null");
        return AR_EBADPARAM;
    }
    //Search for Voice Subgraph Calibration Data
    status = GetVoiceParameterCalData(req, rsp);
    if (AR_FAILED(status) && status != AR_ENOTEXIST)
    {
        return status;
    }
    else if (AR_SUCCEEDED(status)) return status;

    //Find Subgraph and Possible CKV offsets
    cal_key_entry_list = (AcdbCalKeyTblEntry*)&glb_buf_1[0];

    status = SearchSubgraphCalLut(req->subgraph_id, sizeof(glb_buf_1),
        cal_key_entry_list, &num_entries);
    if (AR_FAILED(status))
    {
        return status;
    }

    //Search List of Offsets for matching CKV
    for (uint32_t i = 0; i < num_entries; i++)
    {
        AcdbCalKeyTblEntry e = cal_key_entry_list[i];
        status = SearchForMatchingCalKeyVector(req,
            e.offset_cal_key_tbl, e.offset_cal_lut, &ckv_lut_entry);
        if (AR_SUCCEEDED(status)) break;
    }

    if (AR_FAILED(status))
    {
        //Either CKV not found or another error occured
        ACDB_ERR("Error[%d]: Could not find a matcing CKV.", status);
        return status;
    }

    //Use offset from CKV LUT to find Module Calibration
    status = BuildParameterCalDataBlob(req, rsp,
        (AcdbDefDotPair*)(void*)&ckv_lut_entry);

    return status;
}

/**
* \brief
*		Retrieve tag data for a parameter from the data pool
* \param[int] req: Request containing the subgraph, module instance, parameter, and module tag
* \param[in/out] rsp: A data blob containg tag data
* \param[in] offset_pair: Tag data def and dot offsets
* \return 0 on success, and non-zero on failure
*/
int32_t BuildParameterTagDataBlob(AcdbParameterTagData *req, AcdbBlob *rsp, AcdbDefDotPair *offset_pair)
{
    int32_t status = AR_EOK;
    uint32_t offset = 0;
    uint32_t blob_offset = 0;
    uint32_t data_offset = 0;
    uint32_t param_size = 0;
    uint32_t error_code = 0;
    uint32_t param_dot_offset = 0;
    uint32_t entry_index = 0;
    uint32_t num_iid_pid_entries = 0;
    AcdbModIIDParamIDPair iid_pid_pair = { 0 };
    ChunkInfo ci_tag_data_def = { 0 };
    ChunkInfo ci_tag_data_dot = { 0 };
    ChunkInfo ci_data_pool = { 0 };

    if (IsNull(req) || IsNull(offset_pair))
    {
        ACDB_ERR("Error[%d]: The input request, response, or offset pair is null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    ci_tag_data_def.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_DEF;
    ci_tag_data_dot.chunk_id = ACDB_CHUNKID_MODULE_TAGDATA_DOT;
    ci_data_pool.chunk_id = ACDB_CHUNKID_DATAPOOL;

    status = ACDB_GET_CHUNK_INFO(
        &ci_tag_data_def, &ci_tag_data_dot, &ci_data_pool);

    offset = ci_tag_data_def.chunk_offset + offset_pair->offset_def;
    status = FileManReadBuffer(&num_iid_pid_entries, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read number of tagged <MID, PID> entries.", status);
        return status;
    }

    status = FileManReadBuffer(&glb_buf_3[0], num_iid_pid_entries * sizeof(AcdbModIIDParamIDPair), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read List of <MID, PID>.", status);
        return status;
    }

    iid_pid_pair.module_iid = req->module_iid;
    iid_pid_pair.parameter_id = req->parameter_id;

    if (SEARCH_ERROR == AcdbDataBinarySearch2((void*)&glb_buf_3,
        num_iid_pid_entries * sizeof(AcdbModIIDParamIDPair),
        &iid_pid_pair, 2,
        (int32_t)(sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)),
        &entry_index))
    {
        //MID PID not found
        return AR_ENOTEXIST;
    }
    else
    {
        offset = ci_tag_data_dot.chunk_offset + offset_pair->offset_dot;

        /* Calculation for DOT offset of <MID, PID> pair
        *
        * Offset of  +  Number of  +  Offset of the desired DOT entry based on
        *    DOT       DOT entries           index of <MID,PID> in DEF
        */
        param_dot_offset = offset + sizeof(uint32_t) + sizeof(uint32_t) * (uint32_t)(
            entry_index / (sizeof(AcdbModIIDParamIDPair) / sizeof(uint32_t)));

        status = FileManReadBuffer(&data_offset, sizeof(uint32_t), &param_dot_offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read data pool offset from DOT.", status);
            return status;
        }

        offset = ci_data_pool.chunk_offset + data_offset;

        status = FileManReadBuffer(&param_size, sizeof(uint32_t), &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read param size from data pool.", status);
            return status;
        }

        rsp->buf_size = sizeof(AcdbModIIDParamIDPair) + 2 * sizeof(uint32_t) + param_size;

        //Write Module IID, and Parameter ID
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(AcdbModIIDParamIDPair),
            &iid_pid_pair, sizeof(AcdbModIIDParamIDPair));
        blob_offset += sizeof(AcdbModIIDParamIDPair);

        //Write Parameter Size
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
            &param_size, sizeof(uint32_t));
        blob_offset += sizeof(uint32_t);

        //Write Error code
        ACDB_MEM_CPY_SAFE((uint8_t*)rsp->buf + blob_offset, sizeof(uint32_t),
            &error_code, sizeof(uint32_t));
        blob_offset += sizeof(uint32_t);

        //Write Payload
        status = FileManReadBuffer((uint8_t*)rsp->buf + blob_offset,
            param_size, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read param payload of %d bytes.",
                status, param_size);
            return status;
        }
    }

    return status;
}

/**
* \brief
*		Get Tag data for one parameter
* \param[int] req: Request containing the subgraph, module instance, parameter, and module tag
* \param[in/out] rsp: A data blob containg tag data
* \return 0 on success, and non-zero on failure
*/
int32_t AcdbGetParameterTagData(AcdbParameterTagData *req, AcdbBlob* rsp)
{
    int32_t status = AR_EOK;
    uint32_t offset_tag_data_tbl = 0;
    AcdbSgIdModuleTag sg_module_tag = { 0 };
    AcdbDefDotPair offset_pair = { 0 };

    status = AcdbSort2(
        req->module_tag.tag_key_vector.num_keys * sizeof(AcdbKeyValuePair),
        req->module_tag.tag_key_vector.graph_key_vector,
        sizeof(AcdbKeyValuePair), 0);

    status = TagDataSearchKeyTable(req->subgraph_id, req->module_tag.tag_id,
        &offset_tag_data_tbl);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to search for Subgraph(%x) Tag(%x)",
            status, req->subgraph_id, req->module_tag.tag_id);
        return status;
    }

    sg_module_tag.module_tag = req->module_tag;
    sg_module_tag.num_sg_ids = 1;
    sg_module_tag.sg_ids = &req->subgraph_id;

    status = TagDataSearchLut(&sg_module_tag.module_tag.tag_key_vector,
        offset_tag_data_tbl, &offset_pair);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to search tag data LUT for Subgraph(%x) Tag(%x)",
            status, req->subgraph_id, req->module_tag.tag_id);
        return status;
    }

    status = BuildParameterTagDataBlob(req, rsp, &offset_pair);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to build tag data blob for Subgraph(%x) Tag(%x)",
            status, req->subgraph_id, req->module_tag.tag_id);
    }

    return status;
}

