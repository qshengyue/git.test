/**
*=============================================================================
* \file acdb_delta_file_mgr.c
*
* \brief
*		Contains the implementation of the delta acdb file manager
*		interface.
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

#include "acdb_delta_file_mgr.h"
#include "acdb_init_utility.h"
#include "acdb_delta_parser.h"
#include "ar_osal_error.h"
#include "ar_osal_file_io.h"
#include "acdb_common.h"
#include "acdb_heap.h"

/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */
#define INF 4294967295U

/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */
typedef struct _AcdbDeltaDataFileInfo AcdbDeltaDataFileInfo;
#include "acdb_begin_pack.h"
struct _AcdbDeltaDataFileInfo
{
	uint32_t file_count;
	AcdbCmdDeltaFileInfo file_info[MAX_ACDB_FILE_COUNT];
}
#include "acdb_end_pack.h"
;

typedef struct _subgraph_module_iid_t SubgraphModuleIIDMap;
#include "acdb_begin_pack.h"
struct _subgraph_module_iid_t
{
	uint32_t subgraph_id;
	uint32_t proc_count;
	uint32_t *proc_list;
	uint32_t module_count;
	uint32_t *module_list;
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

/* ---------------------------------------------------------------------------
* Global Data Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Variable Definitions
*--------------------------------------------------------------------------- */
static AcdbDeltaDataFileInfo gDeltaDataFileInfo;

/* ---------------------------------------------------------------------------
* Function Declarations and Definitions
*--------------------------------------------------------------------------- */

int32_t AcdbDeltaDataCmdInit(AcdbCmdDeltaFileInfo *cmdInfo)
{
	int32_t result = AR_EOK;
	uint32_t idx;

	if (cmdInfo == NULL)
	{
		ACDB_ERR("AcdbDeltaDataCmdInit() failed, cmdInfo is null");
		return AR_EFAILED;
	}

	if (gDeltaDataFileInfo.file_count == MAX_ACDB_FILE_COUNT)
	{
		ACDB_ERR("AcdbDeltaDataCmdInit() failed, num of delta acdb files is maximum %d", gDeltaDataFileInfo.file_count);
		return AR_EFAILED;
	}

	idx = gDeltaDataFileInfo.file_count++;
	gDeltaDataFileInfo.file_info[idx].file_index = cmdInfo->file_index;
	gDeltaDataFileInfo.file_info[idx].file_info = cmdInfo->file_info;
	gDeltaDataFileInfo.file_info[idx].is_updated = FALSE;
	gDeltaDataFileInfo.file_info[idx].exists = cmdInfo->exists;
	gDeltaDataFileInfo.file_info[idx].file_size = cmdInfo->file_size;
	gDeltaDataFileInfo.file_info[idx].file_handle = cmdInfo->file_handle;
	gDeltaDataFileInfo.file_info[idx].delta_file_path.fileNameLen = cmdInfo->delta_file_path.fileNameLen;
	result = ACDB_STR_CPY_SAFE(
		gDeltaDataFileInfo.file_info[idx].delta_file_path.fileName,
		MAX_FILENAME_LENGTH,
		cmdInfo->delta_file_path.fileName,
		cmdInfo->delta_file_path.fileNameLen
	);

	if (AR_EOK != result)
	{
		ACDB_ERR_MSG_1("Failed to copy delta file path", result);
	}

	return result;
}

int32_t AcdbDeltaDataClearFileInfo()
{
	int32_t result = AR_EOK;
	uint32_t idx = 0;

	for (idx = 0; idx < gDeltaDataFileInfo.file_count; idx++)
	{
		if (gDeltaDataFileInfo.file_info[idx].file_handle != NULL)
		{
			int ret = ar_fclose(gDeltaDataFileInfo.file_info[idx].file_handle);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataClearFileInfo() failed, delta file close was unsuccessful %d", ret);
				return AR_EFAILED;
			}
			gDeltaDataFileInfo.file_info[idx].file_handle = NULL;
			gDeltaDataFileInfo.file_info[idx].file_size = 0;
		}
	}

	return result;
}

int32_t AcdbDeltaDataCmdGetDeltaVersion(uint32_t file_index, uint32_t *delta_major, uint32_t *delta_minor)
{
	int32_t result = AR_EOK;
	uint32_t idx = file_index;
	uint32_t offset = 0;
	size_t bytes_read = 0;

	if (gDeltaDataFileInfo.file_info[idx].exists == TRUE && gDeltaDataFileInfo.file_info[idx].file_handle != NULL)
	{
		offset = 0;
		// parse file and call DM_IOCTL.
		//if (gDeltaDataFileInfo.fInfo[idx].file_size > sizeof(AcdbDeltaFileHeader) + sizeof(AcdbDeltaFileHeaderInfoV1) + sizeof(uint32_t))
		if (gDeltaDataFileInfo.file_info[idx].file_size > sizeof(AcdbDeltaFileHeader) + sizeof(uint32_t))
		{
			int ret = ar_fseek(gDeltaDataFileInfo.file_info[idx].file_handle, offset, AR_FSEEK_BEGIN);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataCmdGetDeltaVersion() failed, file seek to %d was unsuccessful", offset);
				return AR_EFAILED;
			}
			ret = ar_fread(gDeltaDataFileInfo.file_info[idx].file_handle, &delta_major, sizeof(uint32_t), &bytes_read);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataCmdGetDeltaVersion() failed, delta_major read was unsuccessful");
				return AR_EFAILED;
			}
			offset += sizeof(uint32_t); // curDeltafilemajor
			ret = ar_fseek(gDeltaDataFileInfo.file_info[idx].file_handle, offset, AR_FSEEK_BEGIN);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataCmdGetDeltaVersion() failed, file seek to %d was unsuccessful", offset);
				return AR_EFAILED;
			}
			ret = ar_fread(gDeltaDataFileInfo.file_info[idx].file_handle, &delta_minor, sizeof(uint32_t), &bytes_read);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataCmdGetDeltaVersion() failed, delta_minor read was unsuccessful");
				return AR_EFAILED;
			}
			offset += sizeof(uint32_t); // curDeltafileminor
		}
	}

	return result;
}

int32_t AcdbDeltaDataCmdSave(int32_t findex)
{
	int32_t status = AR_EOK;
	ar_fhandle *fhandle = &gDeltaDataFileInfo.file_info[findex].file_handle;
	uint32_t fsize = 0;
	uint32_t fdata_size = 0; ;
	LinkedList *p_map_list = NULL;
	LinkedList map_list = { 0 };
	LinkedListNode *cur_node = NULL;
	AcdbFileManPathInfo file_name_info = { 0 };
	AcdbDeltaKVToSubgraphDataMap *map = NULL;

	memset(&file_name_info.path[0], 0, MAX_FILENAME_LENGTH);
	file_name_info.path_len = 0;
	status = acdb_file_man_ioctl(ACDB_FILE_MAN_GET_FILE_NAME, 
		(uint8_t *)&findex, sizeof(uint32_t),
		(uint8_t *)&file_name_info, sizeof(file_name_info));

	//Close and delete old delta file
	status = ar_fclose(*fhandle);
	if (AR_EOK != status) return status;

	status = AcdbInitUtilDeleteDeltaFileData(
		file_name_info.path, file_name_info.path_len,
		&gDeltaDataFileInfo.file_info[findex].delta_file_path);

	if (AR_EOK != status) return status;

	//Create new delta file and set ar_fhandle to gDeltaDataFileInfo at findex
	//Check to see if file exists otherwise create it
	status = AcdbInitUtilOpenDeltaFile(
		file_name_info.path, file_name_info.path_len, 
		&gDeltaDataFileInfo.file_info[findex].delta_file_path, &fsize);
	if (AR_EOK != status)
	{
		ACDB_ERR_MSG_1("Unable to create a new delta acdb file", status);
		return status;
	}

	status = AcdbInitUtilGetDeltaFileData(
		file_name_info.path, file_name_info.path_len,
		fhandle,
		&gDeltaDataFileInfo.file_info[findex].delta_file_path);

	if (AR_EOK != status) return status;

	gDeltaDataFileInfo.file_info[findex].file_handle = *fhandle;

	status = WriteDeltaFileHeader(
		gDeltaDataFileInfo.file_info[findex].file_handle,
		&gDeltaDataFileInfo.file_info[findex].file_info, 0, 0);

	if (AR_EOK != status) return status;

	p_map_list = &map_list;

	status = acdb_heap_ioctl(
		ACDB_HEAP_CMD_GET_MAP_LIST, NULL, (uint8_t*)&p_map_list, sizeof(uint32_t));
	if (AR_EOK != status) return status;

	cur_node = p_map_list->p_head;

	while (cur_node != NULL)
	{
        map = ((AcdbDeltaKVToSubgraphDataMap*)cur_node->p_struct);
        status = WriteMapChunk(*fhandle, map);
        if (AR_EOK != status) break;
		cur_node = cur_node->p_next;
	}

	status = ar_fseek(*fhandle, 0, AR_FSEEK_BEGIN);

	if (AR_EOK != status) return status;

	fsize = (uint32_t)ar_fsize(*fhandle);
	fdata_size = fsize - sizeof(AcdbDeltaFileHeader) + sizeof(uint32_t);

	status = WriteDeltaFileHeader(
		gDeltaDataFileInfo.file_info[findex].file_handle,
		&gDeltaDataFileInfo.file_info[findex].file_info, fdata_size, p_map_list->length);

	if (AR_EOK != status) return status;

	AcdbListClear(p_map_list);

	p_map_list = NULL;
	gDeltaDataFileInfo.file_info[findex].is_updated = TRUE;
	gDeltaDataFileInfo.file_info[findex].file_size = fsize;
	return status;
}

int32_t AcdbDeltaDataCmdReset()
{
	int32_t result = AR_EOK;
	uint32_t idx = 0;


	for (idx = 0; idx < gDeltaDataFileInfo.file_count; idx++)
	{
		//result = AcdbDeltaDataCmdSave(idx);

		//if (AR_EOK != result)
		//{
		//	ACDB_ERR_MSG_1("Failed to save delta file", result);
		//	return result;
		//}

		result = acdb_heap_ioctl(ACDB_HEAP_CMD_CLEAR, NULL, NULL, 0);

		memset(&gDeltaDataFileInfo.file_info[idx].file_info, 0, sizeof(AcdbDeltaFileInfo));
		gDeltaDataFileInfo.file_info[idx].exists = FALSE;
		gDeltaDataFileInfo.file_info[idx].file_size = 0;
		if (gDeltaDataFileInfo.file_info[idx].file_handle != NULL)
		{
			int ret = ar_fclose(gDeltaDataFileInfo.file_info[idx].file_handle);
			if (ret != 0)
			{
				ACDB_ERR("AcdbDeltaDataCmdReset() failed, delta file close was unsuccessful %d", ret);
				return AR_EFAILED;
			}
			gDeltaDataFileInfo.file_info[idx].file_handle = NULL;
		}
	}
	gDeltaDataFileInfo.file_count = 0;
	return result;
}

//int32_t AcdbDeltaDataWriteFile(ar_fhandle fhandle, int32_t findex, uint32_t map_count, AcdbDeltaCkvToSubgraphDataMap *map_list)
//{
//	int32_t status = AR_EOK;
//
//	//Case 1: Delta file is empty, write the header and the chunk information
//	if (gDeltaDataFileInfo.file_info[findex].file_size == 0)
//	{
//		status = WriteDeltaFileHeader(fhandle);
//
//		if (AR_EOK != status) return status;
//	}
//
//	//Case 2: Delta file has at heast one Map Chunk
//	for (uint32_t i = 0; i < map_count; i++)
//	{
//		status = WriteMapChunk(fhandle, map_list[i]);
//
//		if (AR_EOK != status) return status;
//	}
//
//	gDeltaDataFileInfo.file_info[findex].is_updated = TRUE;
//
//	return status;
//}

//int32_t AcdbDeltaDataWriteFile(int32_t findex, uint32_t map_count, const AcdbDeltaCkvToSubgraphDataMap *map_list)
//{
//	int32_t status = AR_EOK;
//
//	//Case 1: Delta file is empty, write the header and the chunk information
//	if (gDeltaDataFileInfo.file_info[findex].file_size == 0)
//	{
//		status = WriteDeltaFileHeader(
//			gDeltaDataFileInfo.file_info[findex].file_handle, 
//			&gDeltaDataFileInfo.file_info[findex].file_info);
//
//		if (AR_EOK != status) return status;
//	}
//
//	//Case 2: Delta file has at least one Map Chunk
//	for (uint32_t i = 0; i < map_count; i++)
//	{
//		status = WriteMapChunk(
//			gDeltaDataFileInfo.file_info[findex].file_handle, 
//			map_list[i]);
//
//		if (AR_EOK != status) return status;
//	}
//
//	gDeltaDataFileInfo.file_info[findex].is_updated = TRUE;
//
//	return status;
//}

int32_t AcdbDeltaGetMapChunkInfo(uint32_t findex, uint32_t *foffset, AcdbDeltaMapChunkInfo* chunk_info)
{
	return AcdbDeltaParserGetMapChunkInfo(
		gDeltaDataFileInfo.file_info[findex].file_handle,
		foffset,
		chunk_info);//AcdbDeltaParserGetMap(ar_fhandle fhandle, uint32_t *offset, AcdbDeltaCkvToSubgraphDataMap *map);
}

//int32_t AcdbDeltaGetMap(uint32_t findex, uint32_t *foffset, AcdbDeltaCkvToSubgraphDataMap* map)
//{
//	return AcdbDeltaParserGetMap(
//		gDeltaDataFileInfo.file_info[findex].file_handle,
//		foffset, map);
//}

int32_t TranslateDeltaFileToHeap(uint32_t findex)
{
	int32_t status = AR_EOK;
	uint32_t foffset = sizeof(AcdbDeltaFileHeader);
	AcdbDeltaKVToSubgraphDataMap *map = NULL;

	while (foffset < gDeltaDataFileInfo.file_info[findex].file_size)
	{
		map = ACDB_MALLOC(AcdbDeltaKVToSubgraphDataMap,1);

        if (IsNull(map))
        {
            ACDB_ERR("Failed to allocate memory for Key Vector to Subgraph Map.");
            status = AR_ENOMEMORY;
            break;
        }

        ACDB_CLEAR_BUFFER(*map);

		status = ReadMapChunk(
			gDeltaDataFileInfo.file_info[findex].file_handle,
			&foffset, map);

		if (AR_EOK != status && !IsNull(map))
		{
			ACDB_ERR_MSG_1("Failed to read map from the *.acdbdelta file.", status);
            free_map(map);
			break;
		}

		status = acdb_heap_ioctl(
			ACDB_HEAP_CMD_ADD_MAP, 
			(uint8_t*)map,
			NULL, 0);

		if (AR_EOK != status)
		{
			//ACDB_FREE all data on heap and break
			ACDB_ERR_MSG_1("Failed to add map to the heap.", status);
			break;
		}
	}

	if (AR_EOK != status)
	{
		acdb_heap_ioctl(ACDB_HEAP_CMD_CLEAR, NULL, NULL, 0);
	}

	return status;
}

int32_t AcdbDeltaDeleteFile(int32_t findex)
{
    int32_t status = AR_EOK;
    ar_fhandle *fhandle = &gDeltaDataFileInfo.file_info[findex].file_handle;
    AcdbFileManPathInfo file_name_info = { 0 };

    memset(&file_name_info.path[0], 0, MAX_FILENAME_LENGTH);
    file_name_info.path_len = 0;
    status = acdb_file_man_ioctl(ACDB_FILE_MAN_GET_FILE_NAME,
        (uint8_t *)&findex, sizeof(uint32_t),
        (uint8_t *)&file_name_info, sizeof(file_name_info));

    //Close and delete old delta file
    status = ar_fclose(*fhandle);
    if (AR_EOK != status)
    {
        ACDB_ERR("Error[%d]: Failed to close delta file", status);
        return status;
    }

    status = AcdbInitUtilDeleteDeltaFileData(
        file_name_info.path, file_name_info.path_len,
        &gDeltaDataFileInfo.file_info[findex].delta_file_path);

    if (AR_EOK != status) return status;

    gDeltaDataFileInfo.file_info[findex].exists = FALSE;
    gDeltaDataFileInfo.file_info[findex].is_updated = FALSE;

    return status;
}

int32_t acdb_delta_data_ioctl(uint32_t nCommandId,
	uint8_t *pInput,
	uint32_t nInputSize,
	uint8_t *pOutput)
{
	int32_t status = AR_EOK;

	switch (nCommandId)
	{
	case ACDB_DELTA_DATA_CMD_INIT:
	{
		if (pInput == NULL || nInputSize != sizeof(AcdbCmdDeltaFileInfo))
		{
			return AR_EBADPARAM;
		}
		status = AcdbDeltaDataCmdInit((AcdbCmdDeltaFileInfo *)pInput);
	}
	break;
	case ACDB_DELTA_DATA_CMD_RESET:
	{
		status = AcdbDeltaDataCmdReset();
	}
	break;
	case ACDB_DELTA_DATA_CMD_CLEAR_FILE_BUFFER:
	{
		status = AcdbDeltaDataClearFileInfo();
	}
	break;
	case ACDB_DELTA_DATA_CMD_GET_FILE_VERSION:
	{
		uint32_t delta_major = 0;
		uint32_t delta_minor = 0;
		uint32_t *pDeltafile_index = NULL;
		AcdbDeltaFileVersion *deltaVersion = ACDB_MALLOC(AcdbDeltaFileVersion, 1);
		if (deltaVersion == NULL)
		{
			return AR_ENOMEMORY;
		}
		if (pInput == NULL || nInputSize != sizeof(uint32_t))
		{
			return AR_EBADPARAM;
		}
		pDeltafile_index = (uint32_t *)pInput;
		status = AcdbDeltaDataCmdGetDeltaVersion(*pDeltafile_index, &delta_major, &delta_minor);
		deltaVersion->major = delta_major;
		deltaVersion->minor = delta_minor;
		ACDB_MEM_CPY(pOutput, deltaVersion, sizeof(AcdbDeltaFileVersion));
		ACDB_FREE(deltaVersion);
		status = AR_EOK;
	}
	break;
	case ACDB_DELTA_DATA_CMD_INIT_HEAP:
	{
		status = TranslateDeltaFileToHeap((uint32_t)*pInput);
	}
	break;
	case ACDB_DELTA_DATA_CMD_SET_DATA:
	{
		status = AR_ENOTIMPL;
		//status = AcdbDeltaDataWriteFile(0, 1, (AcdbDeltaCkvToSubgraphDataMap*)pInput);
	}
	break;
	case ACDB_DELTA_DATA_CMD_GET_DATA:
	{
		//status = AcdbDeltaGetCalData();
		status = AR_ENOTIMPL;
	}
	break;
	case ACDB_DELTA_DATA_CMD_SAVE:
	{
		status = AcdbDeltaDataCmdSave(0);
	}
    break;
    case ACDB_DELTA_DATA_CMD_DELETE_ALL_FILES:
    {
        status = AcdbDeltaDeleteFile(0);
    }
	break;
	default:
		status = AR_EBADPARAM;
		ACDB_ERR("acdb_delta_data_ioctl() failed, Unsupported commandID %08X", nCommandId);
	}

	return status;
}
