/**
*=============================================================================
* \file acdb_delta_parser.c
*
* \brief
*		Contains the implementation of the Acdb Delta Parser/Generator.
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
#include "ar_osal_error.h"
#include "ar_osal_file_io.h"

#include "acdb_delta_parser.h"
#include "acdb_parser.h"
#include "acdb_common.h"

 /* ---------------------------------------------------------------------------
  * Preprocessor Definitions and Constants
  *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Type Declarations
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Global Data Definitions
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Static Variable Definitions
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 *--------------------------------------------------------------------------- */

int32_t file_seek_read(ar_fhandle fhandle, void* p_buf, size_t read_size, uint32_t *offset)
{
	int32_t status = AR_EOK;
	size_t bytes_read = 0;

	status = ar_fseek(fhandle, *offset, AR_FSEEK_BEGIN);

	if (AR_EOK != status) return status;

	status = ar_fread(fhandle, p_buf, read_size, &bytes_read);

	*offset += (uint32_t)bytes_read;

	return status;
}

int32_t ReadHeader(ar_fhandle fhandle, AcdbDeltaFileHeader *file_header)
{
	uint32_t offset = 0;

	if (AR_EOK != file_seek_read(fhandle, file_header, sizeof(AcdbDeltaFileHeader), &offset))
	{
		//ACDB_ERR(__FUNCTION__, NULL, 0);
		return AR_EFAILED;
	}

	return AR_EOK;
}

int32_t CheckFileVersion(AcdbDeltaFileVersion *file_version)
{
	int32_t status = ACDB_PARSE_SUCCESS;//ACDB_PARSE_INVALID_FILE

	//File version must not be higher than the Software File Version

	if ((file_version->major == 1 && file_version->minor == 0) // ||
		/*(file_version->major == ?? && file_version->minor == ??)*/)
	{
		status =  ACDB_PARSE_SUCCESS;
	}
	else
	{
		ACDB_ERR(__FUNCTION__, " Unsupported delta file version %d.%d.%d",
			file_version->major, file_version->minor, file_version->revision);
		status = ACDB_PARSE_INVALID_FILE;
	}

	return status;
}

int32_t IsAcdbDeltaFileValid(ar_fhandle fhandle, uint32_t file_size)
{
	int32_t status = ACDB_PARSE_SUCCESS;
	AcdbDeltaFileHeader file_header;

	if (fhandle == NULL || file_size == 0)
	{
		ACDB_ERR("IsAcdbDeltaFileValid() failed, fp is null %d", ACDB_PARSE_INVALID_FILE);
		return ACDB_PARSE_INVALID_FILE;
	}

	if (file_size < sizeof(AcdbDeltaFileHeader))
	{
		ACDB_ERR("IsAcdbDeltaFileValid() failed, delta acdb file size = %d is less than AcdbDeltaFileHeader", file_size);
		return ACDB_PARSE_INVALID_FILE;
	}

	if (AR_EOK != ReadHeader(fhandle, &file_header))
	{
		ACDB_ERR(__FUNCTION__, " failed with error code %d. Failed to read delta file header.");
		return AR_EFAILED;
	}

	if (ACDB_PARSE_SUCCESS != CheckFileVersion(&file_header.file_version))
	{
		return ACDB_PARSE_INVALID_FILE;
	}

	if (file_size != file_header.file_data_size + (sizeof(AcdbDeltaFileHeader) - sizeof(uint32_t)))
	{
		ACDB_ERR(__FUNCTION__, " failed with error code %d. Data size is incorrect.");
		return ACDB_PARSE_INVALID_FILE;
	}

	return status;
}

int32_t AcdbDeltaParserGetSwVersion(ar_fhandle fhandle, uint32_t file_size, AcdbDeltaFileInfo* delta_finfo)
{
	AcdbDeltaFileHeader file_header;

	if (file_size < (sizeof(AcdbDeltaFileHeader)))
	{
		ACDB_ERR("AcdbDeltaFileGetSWVersion() failed, delta acdb file size = %d is less than AcdbDeltaFileHeader", file_size);
		return ACDB_PARSE_FAILURE;
	}

	if (AR_EOK != ReadHeader(fhandle, &file_header))
	{
		ACDB_ERR(__FUNCTION__, " failed with error code %d. Failed to read delta file header.")
		return AR_EFAILED;
	}

	delta_finfo->major = file_header.acdb_major;
	delta_finfo->minor = file_header.acdb_minor;
	delta_finfo->revision = file_header.acdb_revision;
	delta_finfo->cplInfo = file_header.acdb_cpl_info;
	
	return AR_EOK;
}

/*
*	\brief AcdbDeltaParserGetMapChunk
*		Get the Map Chunk based on the provided CKV
*/
//int32_t AcdbDeltaParserGetMapChunk(ar_fhandle fhandle, const uint32_t file_size,
//	const AcdbGraphKeyVector ckv, AcdbDeltaMapChunkInfo *map_info)
//{
//	uint32_t key_count = 0;
//
//	uint32_t map_size = 0;
//
//	size_t offset = 0;
//
//	size_t bytes_read = 0;
//
//	size_t sz_cal_key_vector = ckv.num_keys * sizeof(uint32_t);
//
//	//The Calilbration Key Vector acts like a Chunk ID
//	AcdbGraphKeyVector tmpCkv;
//
//	AcdbDeltaFileHeader file_header;
//
//	if (map_info == NULL)
//	{
//		//ACDB_PKT_LOG_DATA(__FUNCTION__, NULL, 0);
//		return AR_EBADPARAM;
//	}
//
//	//Read file header to get map_count
//	if (AR_EOK != ar_fread(fhandle, &file_header, sizeof(AcdbDeltaFileHeader), &bytes_read))
//	{
//		//ACDB_PKT_LOG_DATA(__FUNCTION__, NULL, 0);
//		return AR_EFAILED;
//	}
//
//	offset += sizeof(AcdbDeltaFileHeader);
//
//	ar_fseek(fhandle, offset, AR_FSEEK_BEGIN);
//
//	//Compare Delta File CKV to input ckv to find the matching map
//	for (uint32_t i = 0; i < file_header.map_count; i++)
//	{
//		if (offset > file_size)
//		{
//			return AR_EFAILED;
//		}
//
//		//Read Number of Keys
//		ar_fread(fhandle, &key_count, sizeof(uint32_t), &bytes_read);
//
//		offset += bytes_read;
//
//		//If Maps Key Count doesnt match the input CKV key count, skip this chunk
//		if (ckv.num_keys != key_count)
//		{
//			//Skip over CKV
//			offset += key_count * sizeof(uint32_t);
//
//			//read map size in order to skip to the next chunk
//			ar_fseek(fhandle, offset, AR_FSEEK_BEGIN);
//
//			ar_fread(fhandle, &map_size, sizeof(uint32_t), &bytes_read);
//
//			offset += bytes_read + map_size;
//
//			continue;
//		}
//		else
//		{
//			//Compare CKVs, if the CKVs dont match, continue
//
//			ar_fread(fhandle, &tmpCkv, sz_cal_key_vector, &bytes_read);
//
//			offset += bytes_read;
//
//			ar_fseek(fhandle, offset, AR_FSEEK_BEGIN);
//
//			ar_fread(fhandle, &map_size, sizeof(uint32_t), &bytes_read);
//
//			//offset += bytes_read;
//
//			if (tmpCkv.num_keys == ckv.num_keys)
//			{
//				if (AR_EOK == ACDB_MEM_CMP(ckv.graph_key_vector, &tmpCkv, sz_cal_key_vector))
//				{
//					//The chunk is found so re
//					map_info->map_offset = (uint32_t)offset;
//
//					map_info->map_size = map_size;
//
//					return AR_EOK;
//				}
//				else
//				{
//					offset += bytes_read + map_size;
//				}
//			}
//		}
//	}
//
//	return AR_EFAILED;
//}

int32_t AcdbDeltaParserGetMapChunkInfo(ar_fhandle fhandle,
	uint32_t *offset, AcdbDeltaMapChunkInfo *map_info)
{
	int32_t status = AR_EOK;

	uint32_t kv_pair_count = 0;

	uint32_t map_size = 0;

	size_t bytes_read = 0;

	if (map_info == NULL)
	{
		//ACDB_PKT_LOG_DATA(__FUNCTION__, NULL, 0);
		return AR_EBADPARAM;
	}

	ar_fseek(fhandle, (size_t)offset, AR_FSEEK_BEGIN);

	//while (offset < file_size)
	
	map_info->map_offset = *offset;

	//Read Number of Keys
	status = ar_fread(fhandle, &kv_pair_count, sizeof(uint32_t), &bytes_read);

	if(AR_EOK != status) return status;

	*offset += (uint32_t)bytes_read + (kv_pair_count * 2UL * (uint32_t)sizeof(uint32_t));

	status = ar_fseek(fhandle, (size_t)offset, AR_FSEEK_BEGIN);

	if (AR_EOK != status) return status;

	status = ar_fread(fhandle, &map_size, sizeof(uint32_t), &bytes_read);

	if (AR_EOK != status) return status;

	*offset += (uint32_t)bytes_read;

	map_info->map_size = map_size;

	return AR_EFAILED;
}

int32_t ReadPersistanceData(ar_fhandle fhandle, AcdbDeltaPersistanceData *persistance_data, uint32_t *offset)
{
	int32_t status = AR_EOK;
	uint32_t bytes_read = *offset;
	uint32_t persistance_data_size = 0;

	status = file_seek_read(fhandle, &persistance_data->data_size, sizeof(uint32_t), &bytes_read);
	if (AR_EOK != status) return status;

	ACDB_CLEAR_BUFFER(persistance_data->cal_data_list);

	while (persistance_data_size < persistance_data->data_size)
	{
		AcdbDeltaModuleCalData *caldata = ACDB_MALLOC(AcdbDeltaModuleCalData, 1);
		if (IsNull(caldata))
		{
			status = AR_ENOMEMORY;
			break;
		}
		status = file_seek_read(fhandle, caldata, 3 * sizeof(uint32_t), &bytes_read);//Module IID, Param ID, Param Size
		if (AR_EOK != status) break;

		caldata->param_payload = ACDB_MALLOC(uint8_t, caldata->param_size);

		status = file_seek_read(fhandle, caldata->param_payload, caldata->param_size, &bytes_read);// Payload
		if (AR_EOK != status) break;

		LinkedListNode *node = AcdbListCreateNode(caldata);

		AcdbListAppend(&persistance_data->cal_data_list, node);

		persistance_data_size += 3 * sizeof(uint32_t) + caldata->param_size;

		caldata = NULL;
	}

	*offset = bytes_read;

	return status;
}

int32_t ReadMapChunk(ar_fhandle fhandle, uint32_t *offset,
	AcdbDeltaKVToSubgraphDataMap *map)
{
	int32_t status = AR_EOK;
	uint32_t kv_pair_count = 0;
	uint32_t bytes_read = *offset;
    AcdbModuleTag* module_tag = NULL;
    AcdbGraphKeyVector* key_vector = NULL;

	if (map == NULL)
	{
		return AR_EBADPARAM;
	}

    status = file_seek_read(fhandle, &map->key_vector_type, sizeof(uint32_t), &bytes_read);
    if (AR_EOK != status)
    {
        ACDB_ERR_MSG_1("Unable to read key vector type", status);
        return status;
    }

    //Read Module Tag or Graph Key Vector depending on key vector type
    switch (map->key_vector_type)
    {
    case TAG_KEY_VECTOR:
        module_tag = ACDB_MALLOC(AcdbModuleTag, 1);
        if (IsNull(module_tag))
        {
            status = AR_ENOMEMORY;
            ACDB_ERR_MSG_1("Unable to allocate memory for module tag", status);
            return status;
        }

        map->key_vector_data = (void*)module_tag;

        status = file_seek_read(fhandle, &module_tag->tag_id, sizeof(uint32_t), &bytes_read);
        if (AR_EOK != status)
        {
            ACDB_FREE(module_tag);
            ACDB_ERR_MSG_1("Unable to read module tag", status);
            return status;
        }

        key_vector = &module_tag->tag_key_vector;
        if (IsNull(key_vector))
        {
            status = AR_EBADPARAM;
            ACDB_FREE(module_tag);
            ACDB_ERR_MSG_1("Tag Key Vector is null", status);
            return status;
        }

        break;
    case CAL_KEY_VECTOR:
        key_vector = ACDB_MALLOC(AcdbGraphKeyVector, 1);
        if (IsNull(key_vector))
        {
            status = AR_ENOMEMORY;
            ACDB_ERR_MSG_1("Unable to allocate memory for Calibration Key Vector", status);
            return status;
        }
        
        map->key_vector_data = (void*)key_vector;
        break;
    default:
        break;
    }

    if (IsNull(key_vector))
    {
        ACDB_ERR("The Map Key vector is null");
        return AR_EBADPARAM;
    }

    ACDB_CLEAR_BUFFER(*key_vector);
	status = file_seek_read(fhandle, &kv_pair_count, sizeof(uint32_t), &bytes_read);

	if (AR_EOK != status) return status;

    key_vector->num_keys = kv_pair_count;

    if (kv_pair_count > 0)
    {
        key_vector->graph_key_vector = ACDB_MALLOC(AcdbKeyValuePair, kv_pair_count * sizeof(AcdbKeyValuePair));

        status = file_seek_read(fhandle, key_vector->graph_key_vector, kv_pair_count * sizeof(AcdbKeyValuePair), &bytes_read);
        if (AR_EOK != status)
        {
            if (!IsNull(key_vector->graph_key_vector))
            {
                ACDB_FREE(key_vector->graph_key_vector);
            }

            return status;
        }
    }

    module_tag = NULL;
    key_vector = NULL;

	status = file_seek_read(fhandle, &map->map_size, sizeof(uint32_t), &bytes_read);
	if (AR_EOK != status) return status;

	status = file_seek_read(fhandle, &map->num_subgraphs, sizeof(uint32_t), &bytes_read);
	if (AR_EOK != status) return status;

	LinkedListNode *sg_data_node = NULL;
	AcdbDeltaSubgraphData *sg_data = NULL;

	ACDB_CLEAR_BUFFER(map->subgraph_data_list);

	for (uint32_t i = 0; i < map->num_subgraphs; i++)
	{
		sg_data = ACDB_MALLOC(AcdbDeltaSubgraphData, 1);
		if (IsNull(sg_data))
		{
			status = AR_ENOMEMORY;
			break;
		}

		status = file_seek_read(fhandle, sg_data, 2 * sizeof(uint32_t), &bytes_read);
		if (AR_EOK != status) break;

		status = ReadPersistanceData(fhandle, &sg_data->non_global_data, &bytes_read);
		if (AR_EOK != status) break;

		status = ReadPersistanceData(fhandle, &sg_data->global_data, &bytes_read);
		if (AR_EOK != status) break;

		sg_data_node = AcdbListCreateNode(sg_data);
		AcdbListAppend(&map->subgraph_data_list, sg_data_node);
	}

	if (AR_EOK != status)
	{
		AcdbListClear(&map->subgraph_data_list);
		return status;
	}

	*offset = bytes_read;

	sg_data_node = NULL;
	sg_data = NULL;
	return status;
}

int32_t WriteDeltaFileHeader(ar_fhandle fhandle, AcdbDeltaFileInfo* delta_finfo, uint32_t fdata_size, uint32_t map_count)
{
	size_t bytes_written = 0;
	AcdbDeltaFileHeader file_header;
	file_header.file_version.major = ACDB_DELTA_FILE_VERSION_MAJOR;
	file_header.file_version.minor = ACDB_DELTA_FILE_VERSION_MINOR;
	file_header.file_version.revision = ACDB_DELTA_FILE_REVISION;

	file_header.acdb_major = delta_finfo->major;
	file_header.acdb_minor = delta_finfo->minor;
	file_header.acdb_revision = delta_finfo->revision;
	file_header.acdb_cpl_info = delta_finfo->cplInfo;
	file_header.file_data_size = fdata_size;
	file_header.map_count = map_count;

	int32_t status = ar_fwrite(fhandle, &file_header, sizeof(AcdbDeltaFileHeader), &bytes_written);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to write the *.acdbdelta file header", status);
		return AR_EFAILED;
	}

	return AR_EOK;
}

int32_t WritePersistanceData(ar_fhandle fhandle, AcdbDeltaPersistanceData *persist_data)
{
	int32_t status = AR_EOK;
	size_t bytes_written = 0;
	AcdbDeltaModuleCalData cal_data;

	status = ar_fwrite(fhandle, &persist_data->data_size, sizeof(uint32_t), &bytes_written);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to write global/non-global data size.", status);
		return AR_EFAILED;
	}

	if (persist_data->data_size == 0)
	{
		return AR_EOK;
	}

	LinkedListNode *cur_node = persist_data->cal_data_list.p_head;
	while (!IsNull(cur_node))
	{
		cal_data = *((AcdbDeltaModuleCalData*)cur_node->p_struct);

		status = ar_fwrite(fhandle, &cal_data.module_iid, sizeof(uint32_t), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write Module Instance ID.", status);
			return AR_EFAILED;
		}

		status = ar_fwrite(fhandle, &cal_data.param_id, sizeof(uint32_t), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write Parameter ID.", status);
			return AR_EFAILED;
		}

		status = ar_fwrite(fhandle, &cal_data.param_size, sizeof(uint32_t), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write Parameter Size.", status);
			return AR_EFAILED;
		}

		status = ar_fwrite(fhandle, (uint8_t*)cal_data.param_payload, (size_t)cal_data.param_size, &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write Parameter Payload.", status);
			return AR_EFAILED;
		}

		cur_node = cur_node->p_next;
	}

	return AR_EOK;
}

int32_t WriteMapChunk(ar_fhandle fhandle, AcdbDeltaKVToSubgraphDataMap *map)
{
	int32_t status = AR_EOK;
    AcdbModuleTag* module_tag = NULL;
    AcdbGraphKeyVector* key_vector = NULL;
	if (IsNull(map)) return AR_EBADPARAM;

	uint32_t i = 0;
	size_t bytes_written = 0;

    status = ar_fwrite(fhandle, &map->key_vector_type, sizeof(uint32_t), &bytes_written);
    if (AR_EOK != status)
    {
        ACDB_ERR_MSG_1("Unable to write key vector type", status);
        return AR_EFAILED;
    }

    //Write Module Tag or Graph Key Vector depending on KV type
    switch (map->key_vector_type)
    {
    case TAG_KEY_VECTOR:
        module_tag = ((AcdbModuleTag*)map->key_vector_data);
        status = ar_fwrite(fhandle, &module_tag->tag_id, sizeof(uint32_t), &bytes_written);
        if (AR_EOK != status)
        {
            ACDB_ERR_MSG_1("Unable to write module tag", status);
            return AR_EFAILED;
        }

        key_vector = &module_tag->tag_key_vector;

        if (IsNull(key_vector))
        {
            ACDB_ERR("Map Tag Key Vector is null");
            return AR_EBADPARAM;
        }
        break;
    case CAL_KEY_VECTOR:
        key_vector = ((AcdbGraphKeyVector*)map->key_vector_data);

        if (IsNull(key_vector))
        {
            ACDB_ERR("Map Calibration Key Vector is null");
            return AR_EBADPARAM;
        }
        break;
    default:
        break;
    }

    if (IsNull(key_vector))
    {
        ACDB_ERR("The Map Key Vector is null");
        return AR_EBADPARAM;
    }

	status = ar_fwrite(fhandle, &key_vector->num_keys, sizeof(uint32_t), &bytes_written);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to write number of cal key vector key pairs", status);
		return AR_EFAILED;
	}

	for (i = 0; i < key_vector->num_keys; i++)
	{
		status = ar_fwrite(fhandle, &key_vector->graph_key_vector[i], sizeof(AcdbKeyValuePair), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write the calibration key vector", status);
			return AR_EFAILED;
		}
	}

	status = ar_fwrite(fhandle, &map->map_size, sizeof(uint32_t), &bytes_written);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to write map size", status);
		return AR_EFAILED;
	}

	status = ar_fwrite(fhandle, &map->num_subgraphs, sizeof(uint32_t), &bytes_written);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to write number of subgraph", status);
		return AR_EFAILED;
	}

	LinkedListNode *sg_data_node = map->subgraph_data_list.p_head;
	AcdbDeltaSubgraphData *sg_data = NULL;

	do
	{
		sg_data = (AcdbDeltaSubgraphData*)sg_data_node->p_struct;

		status = ar_fwrite(fhandle, &sg_data->subgraph_id, sizeof(uint32_t), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write the subgraph ID", status);
			return AR_EFAILED;
		}

		status = ar_fwrite(fhandle, &sg_data->subgraph_data_size, sizeof(uint32_t), &bytes_written);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write the subgraph data size", status);
			return AR_EFAILED;
		}

		status = WritePersistanceData(fhandle, &sg_data->non_global_data);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write non-global persistance data", status);
			return AR_EFAILED;
		}

		status = WritePersistanceData(fhandle, &sg_data->global_data);
		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Unable to write global persistance data", status);
			return AR_EFAILED;
		}

	} while (TRUE != AcdbListSetNext(&sg_data_node));

	return AR_EOK;
}
