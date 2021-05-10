/**
*=============================================================================
* \file acdb.c
*
* \brief
*      Contains the implementation of the public interface to the Audio
*      Calibration Database (ACDB) module.
*
* \copyright
*  Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*=============================================================================
*/

#include "acdb.h"
#include "acdb_file_mgr.h"
#include "acdb_delta_file_mgr.h"
#include "acdb_init.h"
#include "acdb_init_utility.h"
#include "acdb_command.h"
#include "acdb_common.h"
#include "acdb_utility.h"
#include "ar_osal_error.h"
#include "ar_osal_file_io.h"
#include "ar_osal_mutex.h"


/* ---------------------------------------------------------------------------
* Global Data Definitions
*--------------------------------------------------------------------------- */
// Variable to enable/disable persistence from test framework.
// It is not expected to be used externally.
int32_t g_persistenceStatus = FALSE;
ar_osal_mutex_t acdb_lock;
/* ----------------------------------------------------------------------------
* Private Function Definitions
*--------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------------
* Public Function Definitions
*--------------------------------------------------------------------------- */

int32_t acdb_init(AcdbDataFiles *acdb_data_files, AcdbFile* delta_file_path)
{
	int32_t status = AR_EOK;
	uint32_t i = 0;
    uint32_t remaining_file_slots = 0;
    uint32_t num_data_files = 0;
	bool_t should_free_delta_file = FALSE;
	AcdbFileInfo acdb_finfo;
	AcdbCmdFileInfo *p_acdb_cmd_finfo = NULL;
	AcdbCmdDeltaFileInfo *p_acdb_cmd_delta_finfo = NULL;

	ACDB_PKT_LOG_INIT();

    status = AcdbARHeapInit();
    if (AR_FAILED(status))
    {
        return status;
    }

    AcdbLogSwVersion(
        ACDB_SOFTWARE_VERSION_MAJOR,
        ACDB_SOFTWARE_VERSION_MINOR,
        ACDB_SOFTWARE_VERSION_REVISION,
        ACDB_SOFTWARE_VERSION_CPLINFO);

	if (IsNull(acdb_data_files) || acdb_data_files->num_files <= 0 ||
        acdb_data_files->num_files > MAX_ACDB_FILE_COUNT)
	{
		ACDB_ERR_MSG_1("Files do not exist or the file count is more than 20.", AR_EBADPARAM);
		return AR_EBADPARAM;
	}

	p_acdb_cmd_finfo = ACDB_MALLOC(AcdbCmdFileInfo, acdb_data_files->num_files);
	if (IsNull(p_acdb_cmd_finfo))
	{
		ACDB_ERR_MSG_1("Insufficient memory for acdb file allocation.", AR_ENOMEMORY);
		return AR_ENOMEMORY;
	}

	p_acdb_cmd_delta_finfo = ACDB_MALLOC(AcdbCmdDeltaFileInfo, acdb_data_files->num_files);
	if (IsNull(p_acdb_cmd_delta_finfo))
	{
		ACDB_ERR_MSG_1("Insufficient memory for delta acdb file allocation.", AR_ENOMEMORY);
		ACDB_FREE(p_acdb_cmd_finfo);
		return AR_ENOMEMORY;
	}

	for (i = 0; i < acdb_data_files->num_files; i++)
	{
		ACDB_CLEAR_BUFFER(p_acdb_cmd_finfo[i]);
		ACDB_CLEAR_BUFFER(p_acdb_cmd_delta_finfo[i]);

		//Copy filename to AcdbCmdFileInfo
		(p_acdb_cmd_finfo + i)->filename_len = acdb_data_files->acdbFiles[i].fileNameLen;

		status = ACDB_STR_CPY_SAFE(
			(p_acdb_cmd_finfo + i)->chFileName,
			MAX_FILENAME_LENGTH,
			acdb_data_files->acdbFiles[i].fileName,
			acdb_data_files->acdbFiles[i].fileNameLen);

		if (AR_EOK != status)
		{
			ACDB_ERR_MSG_1("Copying filename failed.", status);

			ACDB_FREE(p_acdb_cmd_finfo);
			ACDB_FREE(p_acdb_cmd_delta_finfo);
			return status;
		}

		status = AcdbInit(&p_acdb_cmd_finfo[i], &acdb_finfo);
		if(AR_FAILED(status))
		{
			// If even one file is not valid then acdb_init should return failure.
			// So we should ACDB_FREE the memory allocated for any of the files.

			uint32_t idx = 0;
			for (idx = 0; idx < i + 1; idx++)
			{
				status = ar_fclose(p_acdb_cmd_finfo[idx].file_handle);
				if (AR_EOK != status)
				{
					ACDB_ERR_MSG_1("Could not close the acdb file.", status);
					return AR_EFAILED;
				}

				p_acdb_cmd_finfo[idx].file_handle = NULL;

				if (NULL != p_acdb_cmd_delta_finfo[idx].file_handle)
				{
					status = ar_fclose(p_acdb_cmd_delta_finfo[idx].file_handle);
					if (AR_EOK != status)
					{
						ACDB_ERR_MSG_1("Could not close the acdb delta file.", status);
						return AR_EFAILED;
					}

					p_acdb_cmd_delta_finfo[idx].file_handle = NULL;
				}
			}

			ACDB_FREE(p_acdb_cmd_finfo);
			ACDB_FREE(p_acdb_cmd_delta_finfo);

			ACDB_ERR_MSG_1("Acdb initialization failed.", AR_EFAILED);
			return AR_EFAILED;
		}

        if (p_acdb_cmd_finfo[i].file_type == ACDB_FILE_TYPE_WORKSPACE) continue;

        num_data_files++;
		if (TRUE == AcdbIsPersistenceSupported())
		{
			//Load delta file if present.
			status = AcdbDeltaInit(
				&p_acdb_cmd_finfo[i],
				&p_acdb_cmd_delta_finfo[i],
				delta_file_path);

			should_free_delta_file = ACDB_INIT_SUCCESS != status ? TRUE : FALSE;

			if ((p_acdb_cmd_delta_finfo + i)->file_size == 0)
			{
				//Delta File is empty, so copy the set acdb version info to delta files acdb version info
				ACDB_MEM_CPY(&(p_acdb_cmd_delta_finfo + i)->file_info, &acdb_finfo, sizeof(acdb_finfo));
			}

			if (TRUE == (p_acdb_cmd_delta_finfo + i)->exists &&
				FALSE == AcdbInitDoSwVersionsMatch(&acdb_finfo, &(p_acdb_cmd_delta_finfo + i)->file_info))
			{
				should_free_delta_file = TRUE;
			}

			if (should_free_delta_file == TRUE)
			{
				// ignore loading this delta acdb file
				if ((p_acdb_cmd_delta_finfo + i)->file_handle != NULL)
				{
					status = ar_fclose((p_acdb_cmd_delta_finfo + i)->file_handle);
					if (AR_EOK != status)
					{
						ACDB_ERR_MSG_1("Could not close the acdb delta file.", status);
						return AR_EFAILED;
					}

					(p_acdb_cmd_delta_finfo + i)->file_handle = NULL;
				}

				(p_acdb_cmd_delta_finfo + i)->exists = FALSE;
				(p_acdb_cmd_delta_finfo + i)->file_size = 0;
				(p_acdb_cmd_delta_finfo + i)->is_updated = FALSE;
			}
		}
	}

    if (num_data_files == 0)
    {
        ACDB_FREE(p_acdb_cmd_finfo);
        ACDB_FREE(p_acdb_cmd_delta_finfo);
        ACDB_ERR("Error[%d]: No *.acdb files were found.", AR_ENORESOURCE);
        return AR_ENORESOURCE;
    }

    if (!IsNull(delta_file_path))
    {
        /* The delta file path serves two purposes:
        *  1. to store the delta files
        *  2. to store temporary files generated by ATS clients (e.g ARCT)
        * The delta file path will be set as the temporary file path managed
        * by the File Manager
        */
        status = acdb_file_man_ioctl(ACDB_FILE_MAN_SET_TEMP_PATH_INFO,
            (uint8_t *)delta_file_path, sizeof(AcdbFileManPathInfo), NULL, 0);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to set temporary file path using "
                "the delta file path. Skipping..");
            status = AR_EOK;
        }
    }
    else
    {
        ACDB_ERR("Error[%d]: Unable to set temporary file path using "
            "the delta file path. No path was provided. Skipping..");
    }


	// Now that all the acbd files requested for initialization
	// are valid, check if acdb sw can host this many files first
	// and then update the file manger with each files information
	status = acdb_file_man_ioctl(ACDB_FILE_MAN_GET_AVAILABLE_FILE_SLOTS,
        NULL, 0, (uint8_t *)&remaining_file_slots, sizeof(uint32_t));

	if (AR_EOK != status || remaining_file_slots < acdb_data_files->num_files)
	{
		uint8_t idx = 0;
		for (idx = 0; idx < acdb_data_files->num_files; idx++)
		{
			int ret = ar_fclose((p_acdb_cmd_finfo + idx)->file_handle);
			if (ret != 0)
			{
				ACDB_ERR_MSG_1("Could not close the acdb file.", ret);
				return AR_EFAILED;
			}
			(p_acdb_cmd_finfo + idx)->file_handle = NULL;
			if ((p_acdb_cmd_delta_finfo + idx)->file_handle != NULL)
			{
				ret = ar_fclose((p_acdb_cmd_delta_finfo + idx)->file_handle);
				if (ret != 0)
				{
					ACDB_ERR_MSG_1("Could not close the acdb delta file.", ret);
					return AR_EFAILED;
				}
				(p_acdb_cmd_delta_finfo + idx)->file_handle = NULL;
			}
		}
		ACDB_FREE(p_acdb_cmd_finfo);
		ACDB_FREE(p_acdb_cmd_delta_finfo);
		ACDB_ERR_MSG_1("Could not count the number of avalible file slots.", status);
		return AR_EFAILED;
	}

	for (i = 0; i < acdb_data_files->num_files; i++)
	{
		status = acdb_file_man_ioctl(ACDB_FILE_MAN_SET_FILE_INFO,
            (uint8_t *)&p_acdb_cmd_finfo[i], sizeof(AcdbCmdFileInfo), NULL, 0);
		if (AR_EOK != status)
		{
			// This failure should never occur,
			// if so ACDB_FREE up the rest of the acdb files memory which was created for safety
			while (i < acdb_data_files->num_files)
			{
				int ret = ar_fclose((p_acdb_cmd_finfo + i)->file_handle);
				if (ret != 0)
				{
					ACDB_ERR_MSG_1("Could not close the acdb file.", ret);
					return AR_EFAILED;
				}
				(p_acdb_cmd_finfo + i)->file_handle = NULL;
				if ((p_acdb_cmd_delta_finfo + i)->file_handle != NULL)
				{
					ret = ar_fclose((p_acdb_cmd_delta_finfo + i)->file_handle);
					if (ret != 0)
					{
						ACDB_ERR_MSG_1("Could not close the acdb delta file.", ret);
						return AR_EFAILED;
					}
					(p_acdb_cmd_delta_finfo + i)->file_handle = NULL;
				}
				i++;
			}
			ACDB_FREE(p_acdb_cmd_finfo);
			ACDB_FREE(p_acdb_cmd_delta_finfo);
			ACDB_ERR_MSG_1("Could not set acdb data.", status);
			return AR_EFAILED;
		}
	}

	if (TRUE == AcdbIsPersistenceSupported())
	{
		uint32_t delta_result = 0;
		for (i = 0; i < num_data_files; i++)
		{
			(p_acdb_cmd_delta_finfo + i)->file_index = i;
			AcdbCmdDeltaFileInfo *p_delta_finfo = (p_acdb_cmd_delta_finfo + i);
			delta_result = acdb_delta_data_ioctl(ACDB_DELTA_DATA_CMD_INIT,
                (uint8_t *)p_delta_finfo, sizeof(AcdbCmdDeltaFileInfo), NULL);
			if (AR_EOK != delta_result)
			{
				// remove from memory.
				if (NULL != (p_acdb_cmd_delta_finfo + i)->file_handle)
				{
					int ret = ar_fclose((p_acdb_cmd_delta_finfo + i)->file_handle);
					if (ret != 0)
					{
						ACDB_ERR_MSG_1("Could not close the acdb delta file.", ret);
						return AR_EFAILED;
					}
					(p_acdb_cmd_delta_finfo + i)->file_handle = NULL;
				}
				ACDB_ERR_MSG_1("Failed to initialize delta acdb files.", delta_result);
				return AR_EFAILED;
			}

			delta_result = acdb_delta_data_ioctl(ACDB_DELTA_DATA_CMD_INIT_HEAP, (uint8_t*)&i, sizeof(uint32_t), NULL);
			if (AR_EOK != delta_result)
			{
				ACDB_ERR_MSG_1("Heap initialization failed.", delta_result);
			}
		}
	}

	if (!acdb_lock)
	{
		int32_t mutex_status = ar_osal_mutex_create(&acdb_lock);
		if (mutex_status)
		{
			ACDB_ERR("failed to create mutex: %d", mutex_status);
			return mutex_status;
		}
	}

	ACDB_FREE(p_acdb_cmd_finfo);
	ACDB_FREE(p_acdb_cmd_delta_finfo);
	return status;
}

int32_t acdbCmdIsPersistenceSupported(uint32_t *resp)
{
	int32_t result = AR_EOK;
	if (resp == NULL)
	{
		ACDB_ERR(__FUNCTION__, "failed  %d", AR_EBADPARAM);
		return AR_EBADPARAM;
	}

	*resp = AcdbIsPersistenceSupported();

	return result;
}

int32_t acdb_flash_init(uint32_t* acdbdata_base_addr)
{
	*acdbdata_base_addr = 0x0;
	int32_t result = AR_EOK;
	return result;
}

int32_t acdb_deinit()
{
	int32_t status = AR_EOK;

	if (TRUE == AcdbIsPersistenceSupported())
	{
		status = acdb_delta_data_ioctl(
            ACDB_DELTA_DATA_CMD_RESET, NULL, 0, NULL);
	}

    status = acdb_file_man_ioctl(
        ACDB_FILE_MAN_RESET, NULL, 0, NULL, 0);
	if (acdb_lock)
	{
		int32_t mutex_result = ar_osal_mutex_destroy(acdb_lock);
		if (mutex_result)
		{
			ACDB_ERR("failed to destroy mutex: %d", status);
			return mutex_result;
		}
		acdb_lock = NULL;
	}
	ACDB_PKT_LOG_DEINIT();

    status = AcdbARHeapDeinit();
    if (AR_FAILED(status))
    {
        return status;
    }

	return status;
}

int32_t acdb_ioctl(uint32_t cmd_id,
	const void *cmd_struct,
	uint32_t cmd_struct_size,
	void *rsp_struct,
	uint32_t rsp_struct_size)
{
	int32_t result = AR_EOK;

	ACDB_PKT_LOG_DATA("ACDB_IOCTL_CMD_ID", &cmd_id, sizeof(cmd_id));
	if (acdb_lock)
	{
		result = ar_osal_mutex_lock(acdb_lock);
		if (result)
		{
			ACDB_ERR("failed to acquire mutex: %d", result);
			return result;
		}
	}
	switch (cmd_id) {
	case ACDB_CMD_GET_GRAPH:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGraphKeyVector) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbGraphKeyVector *pInput = (AcdbGraphKeyVector *)cmd_struct;
			AcdbGetGraphRsp *pOutput = (AcdbGetGraphRsp *)rsp_struct;
			result = AcdbCmdGetGraph(pInput, pOutput, rsp_struct_size);
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_DATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSgIdGraphKeyVector) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSgIdGraphKeyVector *pInput = (AcdbSgIdGraphKeyVector*)cmd_struct;
			AcdbGetSubgraphDataRsp *pOutput = (AcdbGetSubgraphDataRsp*)rsp_struct;
			if ((pInput->sg_ids == NULL) || (pInput->graph_key_vector.graph_key_vector == NULL))
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphData(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_CONNECTIONS:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSubGraphList) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSubGraphList *pInput = (AcdbSubGraphList*)cmd_struct;
			AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;
			if ((pInput->num_subgraphs == 0) || (pInput->subgraphs == NULL))
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphConn(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSgIdCalKeyVector) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSgIdCalKeyVector *pInput = (AcdbSgIdCalKeyVector*)cmd_struct;
			AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;
			if ((pInput->num_sg_ids == 0) || (pInput->sg_ids == NULL))
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphCalDataNonPersist(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_PERSIST:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSgIdCalKeyVector) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSgIdCalKeyVector *pInput = (AcdbSgIdCalKeyVector*)cmd_struct;
			AcdbSgIdPersistCalData *pOutput = (AcdbSgIdPersistCalData*)rsp_struct;
			if ((pInput->num_sg_ids == 0) || (pInput->sg_ids == NULL))
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphCalDataPersist(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_IDENTIFIERS:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSgIdCalKeyVector) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSgIdCalKeyVector *pInput = (AcdbSgIdCalKeyVector*)cmd_struct;
			AcdbGlbPsistIdentifierList  *pOutput = (AcdbGlbPsistIdentifierList*)rsp_struct;
			if ((pInput->num_sg_ids == 0) || (pInput->sg_ids == NULL))
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphGlbPersistIds(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_GLB_PSIST_CALDATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGlbPersistCalDataCmdType) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbGlbPersistCalDataCmdType *pInput = (AcdbGlbPersistCalDataCmdType*)cmd_struct;
			AcdbBlob  *pOutput = (AcdbBlob*)rsp_struct;
			if (pInput->cal_Id == 0)
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphGlbPersistCalData(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_MODULE_TAG_DATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSgIdModuleTag) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSgIdModuleTag *pInput = (AcdbSgIdModuleTag*)cmd_struct;
			AcdbBlob  *pOutput = (AcdbBlob*)rsp_struct;
		    result = AcdbCmdGetModuleTagData(pInput, pOutput, rsp_struct_size);

		}
		break;
	case ACDB_CMD_GET_TAGGED_MODULES:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGetTaggedModulesReq) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbGetTaggedModulesReq *pInput = (AcdbGetTaggedModulesReq*)cmd_struct;
			AcdbGetTaggedModulesRsp  *pOutput = (AcdbGetTaggedModulesRsp*)rsp_struct;
		    result = AcdbCmdGetTaggedModules(pInput, pOutput, rsp_struct_size);
		}
		break;
	case ACDB_CMD_GET_DRIVER_DATA:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbDriverData) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbDriverData *pInput = (AcdbDriverData*)cmd_struct;
            AcdbBlob  *pOutput = (AcdbBlob*)rsp_struct;

            result = AcdbCmdGetDriverData(pInput, pOutput, rsp_struct_size);
        }
		break;
	case ACDB_CMD_SET_CALIBRATION_DATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSetCalibrationDataReq))
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbSetCalibrationDataReq *pInput = (AcdbSetCalibrationDataReq*)cmd_struct;
			result = AcdbCmdSetSubgraphCalData(pInput);
		}
		break;
	case ACDB_CMD_ENABLE_PERSISTANCE:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(uint8_t))
		{
			result = AR_EBADPARAM;
		}
		else
		{
			uint8_t *pInput = (uint8_t*)cmd_struct;
			g_persistenceStatus = *pInput == 1 ? TRUE : FALSE;
			result = AR_EOK;
		}

		break;

	case ACDB_CMD_GET_AMDB_REGISTRATION_DATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbAmdbProcID) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbAmdbProcID *pInput = (AcdbAmdbProcID*)cmd_struct;
			AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;

			result = AcdbCmdGetAmdbRegData(pInput, pOutput, rsp_struct_size);

		}
		break;
	case ACDB_CMD_GET_AMDB_DEREGISTRATION_DATA:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbAmdbProcID) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbAmdbProcID *pInput = (AcdbAmdbProcID*)cmd_struct;
			AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;

			result = AcdbCmdGetAmdbDeRegData(pInput, pOutput, rsp_struct_size);

		}
		break;
	case ACDB_CMD_GET_SUBGRAPH_PROCIDS:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbCmdGetSubgraphProcIdsReq) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbCmdGetSubgraphProcIdsReq *pInput = (AcdbCmdGetSubgraphProcIdsReq*)cmd_struct;
			AcdbCmdGetSubgraphProcIdsRsp *pOutput = (AcdbCmdGetSubgraphProcIdsRsp*)rsp_struct;
			if (pInput->num_sg_ids == 0)
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetSubgraphProcIds(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
	case ACDB_CMD_GET_AMDB_BOOTUP_LOAD_MODULES:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbAmdbProcID) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbAmdbProcID *pInput = (AcdbAmdbProcID*)cmd_struct;
			AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;

			result = AcdbCmdGetAmdbBootupLoadModules(pInput, pOutput, rsp_struct_size);

		}
		break;
	case ACDB_CMD_GET_TAGS_FROM_GKV:
		if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbCmdGetTagsFromGkvReq) ||
			rsp_struct == NULL || rsp_struct_size == 0)
		{
			result = AR_EBADPARAM;
		}
		else
		{
			AcdbCmdGetTagsFromGkvReq *pInput = (AcdbCmdGetTagsFromGkvReq*)cmd_struct;
			AcdbCmdGetTagsFromGkvRsp *pOutput = (AcdbCmdGetTagsFromGkvRsp*)rsp_struct;
			if (pInput->graph_key_vector->num_keys == 0)
			{
				result = AR_EBADPARAM;
			}
			else
			{
				result = AcdbCmdGetTagsFromGkv(pInput, pOutput, rsp_struct_size);
			}
		}
		break;
    case ACDB_CMD_GET_GRAPH_CAL_KVS:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGraphKeyVector) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbGraphKeyVector *pInput = (AcdbGraphKeyVector*)cmd_struct;
            AcdbKeyVectorList *pOutput = (AcdbKeyVectorList*)rsp_struct;
            if (pInput->num_keys == 0)
            {
                result = AR_EBADPARAM;
            }
            else
            {
                result = AcdbCmdGetGraphCalKeyVectors(
                    pInput, pOutput, rsp_struct_size);
            }
        }
        break;
    case ACDB_CMD_GET_SUPPORTED_GKVS:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbUintList) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbUintList *pInput = (AcdbUintList*)cmd_struct;
            AcdbKeyVectorList *pOutput = (AcdbKeyVectorList*)rsp_struct;
            result = AcdbCmdGetSupportedGraphKeyVectors(
                pInput, pOutput, rsp_struct_size);
        }
        break;
    case ACDB_CMD_GET_DRIVER_MODULE_KVS:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(uint32_t) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            uint32_t *pInput = (uint32_t*)cmd_struct;
            AcdbKeyVectorList *pOutput = (AcdbKeyVectorList*)rsp_struct;
            result = AcdbCmdGetDriverModuleKeyVectors(
                *pInput, pOutput, rsp_struct_size);
        }
        break;
    case ACDB_CMD_GET_GRAPH_TAG_KVS:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGraphKeyVector) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbGraphKeyVector *pInput = (AcdbGraphKeyVector*)cmd_struct;
            AcdbTagKeyVectorList *pOutput = (AcdbTagKeyVectorList*)rsp_struct;
            if (pInput->num_keys == 0)
            {
                result = AR_EBADPARAM;
            }
            else
            {
                result = AcdbCmdGetGraphTagKeyVectors(
                    pInput, pOutput, rsp_struct_size);
            }
        }
        break;
    case ACDB_CMD_GET_CAL_DATA:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGetCalDataReq) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbGetCalDataReq *pInput = (AcdbGetCalDataReq*)cmd_struct;
            AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;
            result = AcdbCmdGetCalData(pInput, pOutput);
        }
        break;
    case ACDB_CMD_GET_TAG_DATA:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbGetTagDataReq) ||
            rsp_struct == NULL || rsp_struct_size == 0)
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbGetTagDataReq *pInput = (AcdbGetTagDataReq*)cmd_struct;
            AcdbBlob *pOutput = (AcdbBlob*)rsp_struct;
            result = AcdbCmdGetTagData(pInput, pOutput);
        }
        break;
    case ACDB_CMD_SET_TAG_DATA:
        if (cmd_struct == NULL || cmd_struct_size != sizeof(AcdbSetTagDataReq))
        {
            result = AR_EBADPARAM;
        }
        else
        {
            AcdbSetTagDataReq *pInput = (AcdbSetTagDataReq*)cmd_struct;
            result = AcdbCmdSetTagData(pInput);
        }
        break;
	default:
		result = AR_ENOTEXIST;
		ACDB_ERR("Error[%d]: Received unsupported command request"
            " with Command ID[%08X]", cmd_id);
		break;
	}

	if (acdb_lock)
	{
		int32_t mutex_result = ar_osal_mutex_unlock(acdb_lock);
		if (mutex_result)
		{
			ACDB_ERR("failed to release mutex: %d", result);
			return mutex_result;
		}
	}
	return result;
}

