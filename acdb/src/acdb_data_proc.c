/**
*=============================================================================
* \file acdb_data_proc.c
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

#include "acdb_data_proc.h"
#include "acdb_parser.h"
#include "acdb_heap.h"
#include "acdb_common.h"

ar_heap_info glb_ar_heap_info;
/**
* \brief AcdbBlobToCalData
*		Copies one module calibration from AcdbBlob to AcdbDeltaModuleCalData
*		at some offset in AcdbBlob.
* \param[in/out] caldata: Calibration data in the format of delta data that
*		must be freed in acdb_heap. calblob is copied to caldata
* \param[int] calblob: Module calibration blob passed in from the client
* \param[in/out] blob_offset: Offset within the blob
*/
void AcdbBlobToCalData(AcdbDeltaModuleCalData *caldata, const AcdbBlob calblob, uint32_t *blob_offset)
{
	if (*blob_offset < calblob.buf_size)
	{
		ACDB_MEM_CPY(&caldata->module_iid, (uint8_t*)calblob.buf + *blob_offset, sizeof(uint32_t));
		*blob_offset += sizeof(uint32_t);

		ACDB_MEM_CPY(&caldata->param_id, (uint8_t*)calblob.buf + *blob_offset, sizeof(uint32_t));
		*blob_offset += sizeof(uint32_t);

		ACDB_MEM_CPY(&caldata->param_size, (uint8_t*)calblob.buf + *blob_offset, sizeof(uint32_t));
		*blob_offset += sizeof(uint32_t);

		caldata->param_payload = ACDB_MALLOC(uint8_t, caldata->param_size);

		if (!IsNull(caldata->param_payload))
		{
			ACDB_MEM_CPY(caldata->param_payload, (uint8_t*)calblob.buf + *blob_offset, (size_t)caldata->param_size);
			*blob_offset += caldata->param_size;
		}
	}
}

void CalDataToAcdbBlob(AcdbDeltaModuleCalData *caldata, AcdbBlob calblob, uint32_t *blob_offset)
{
    if (*blob_offset < calblob.buf_size)
    {
        ACDB_MEM_CPY(calblob.buf + *blob_offset, &caldata->module_iid, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        ACDB_MEM_CPY(calblob.buf + *blob_offset, &caldata->param_id, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        ACDB_MEM_CPY(calblob.buf + *blob_offset, &caldata->param_size, sizeof(uint32_t));
        *blob_offset += sizeof(uint32_t);

        if (!IsNull(caldata->param_payload))
        {
            ACDB_MEM_CPY((uint8_t*)calblob.buf + *blob_offset, caldata->param_payload, (size_t)caldata->param_size);
            *blob_offset += caldata->param_size;
        }
    }
}

void FreeCalData(AcdbDeltaModuleCalData **caldata)
{
	ACDB_FREE((*caldata)->param_payload);
	ACDB_FREE(*caldata);
	*caldata = NULL;
}

void FreeSubgraphIIDMap(SubgraphModuleIIDMap* sg_iid_map)
{
    if (IsNull(sg_iid_map)) return;

    if (!IsNull(sg_iid_map) &&
        !IsNull(sg_iid_map->proc_info))
    {
        ACDB_FREE(sg_iid_map->proc_info);
        sg_iid_map->proc_info = NULL;
    }

    ACDB_FREE(sg_iid_map);
}

/**
* \brief
* Retrieve the mapping between a subgraph and its modules from the ACDB DATA files
* Caller is responsible for freeing memory to sg_iid_map
*
* \param[in] subgraph_id: The subgraph to search for
* \param[out] sg_iid_map: The map parsed from the data files
* \return 0 on Success, non-zero on failure
*/
int32_t GetSubgraphIIDMap(uint32_t subgraph_id, SubgraphModuleIIDMap **sg_iid_map)
{
	//Acdb File Manager will handle selecting the correct ACDB file to read from
	//ar_fhandle fhandle = NULL;

	uint32_t status = AR_EOK;
	uint32_t offset = 0;
    ChunkInfo ci = { ACDB_CHUNKID_SG_IID_MAP, 0, 0 };
	uint32_t subgraph_count = 0;
	uint32_t found_count = 0;
    //Header: <Subgraph ID, Processor Count, Size>
    size_t sz_subgraph_obj_header = 3 * sizeof(uint32_t);

    if (IsNull(sg_iid_map))
    {
        ACDB_ERR("Error[%d]: Subgraph Instance ID Map is null",
            AR_EBADPARAM);
        return AR_EBADPARAM;
    }

    status = ACDB_GET_CHUNK_INFO(&ci);
    if (AR_EOK != status) return status;

    offset = ci.chunk_offset;
    status = FileManReadBuffer(&subgraph_count, sizeof(uint32_t), &offset);
    if (AR_FAILED(status))
    {
        ACDB_ERR("Error[%d]: Failed to read subgraph count", status);
        return status;
    }

	for (uint32_t i = 0; i < subgraph_count; i++)
	{
        if (IsNull(*sg_iid_map))
        {
            *sg_iid_map = ACDB_MALLOC(SubgraphModuleIIDMap, 1);
            if (*sg_iid_map == NULL)
                return AR_ENOMEMORY;
            memset(*sg_iid_map, 0, sizeof(SubgraphModuleIIDMap));
        }

        status = FileManReadBuffer(*sg_iid_map, sz_subgraph_obj_header, &offset);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Failed to read subgraph count", status);
            goto end;
        }

		if (subgraph_id == (*sg_iid_map)->subgraph_id)
		{
            if ((*sg_iid_map)->size <= 0) continue;

            (*sg_iid_map)->proc_info = ACDB_MALLOC(
                ProcIIDMap, (*sg_iid_map)->size);

            if (IsNull((*sg_iid_map)->proc_info))
            {
                status = AR_ENOMEMORY;
                goto end;
            }

            //ACDB_DBG("Processor info Size(%d)", (*sg_iid_map)->size);

            status = FileManReadBuffer((*sg_iid_map)->proc_info,
                (*sg_iid_map)->size, &offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read processor info", status);
                goto end;
            }

			found_count++;
			break;
		}
		else
		{
			offset += (*sg_iid_map)->size;
		}
	}

    end:
    status = (0 == found_count)? AR_ENOTEXIST: status;

    if (AR_FAILED(status))
    {
        FreeSubgraphIIDMap(*sg_iid_map);
        *sg_iid_map = NULL;
    }

	return status;
}

/**
*	\brief DoesSubgraphContainModule
*		Checks if a module is contained in a subgraph
*	\param[in] subgraph_id: The subgraph to search for
*	\param[in] module_iid: The module instance to check for
*	\param[in] status: ar error status
*	\return TRUE if found, FALSE otherwise
*/
bool_t DoesSubgraphContainModule(uint32_t subgraph_id, uint32_t module_iid, int32_t *status)
{
    SubgraphModuleIIDMap *sg_iid_map = NULL;

    bool_t found = FALSE;

    *status = GetSubgraphIIDMap(subgraph_id, &sg_iid_map);

    if (AR_EOK != *status) return found;

    for (uint32_t i = 0; i < (sg_iid_map)->proc_count; i++)
    {
        for (uint32_t j = 0; j < (sg_iid_map)->proc_info[i].iid_mid_count; j++)
        {
            if (module_iid == (sg_iid_map)->proc_info[i].iid_mid_list[j].mid_iid)
            {
                found = TRUE;
                break;
            }
        }
        if (found)
        {
            break;
        }
    }

    FreeSubgraphIIDMap(sg_iid_map);

    return found;
}

int32_t IsPidGlobalyPersistent(uint32_t pid)
{
	//Acdb File Manager will handle selecting the correct ACDB file to read from
	//ar_fhandle fhandle = NULL;

	uint32_t status = AR_EOK;
	uint32_t offset = 0;
	uint32_t cal_id_count = 0;
	uint32_t search_index = 0;
	uint32_t *glb_persist_pid_map = NULL;
	ChunkInfo ci = { ACDB_CHUNKID_GLB_PID_PERSIST_MAP, 0, 0 };
	//Header: <Cal ID, PID, Cal Data offset in datapool>
	size_t sz_cal_id_obj_header = 3 * sizeof(uint32_t);

	CalibrationIdMap cal_id_obj;

	cal_id_obj.cal_id = 0;
	cal_id_obj.param_id = pid;
	cal_id_obj.cal_data_offset = 0;

	//ACDB_CLEAR_BUFFER(glb_buf_1);

	status = AcdbGetChunkInfo(ci.chunk_id, &ci.chunk_offset, &ci.chunk_size);
	if (AR_EOK != status) return status;

    offset = ci.chunk_offset;
    status = FileManReadBuffer(&cal_id_count, sizeof(uint32_t), &offset);
	if (AR_EOK != status) return status;

    if (cal_id_count == 0)
    {
        ACDB_ERR("Cannot determine global persistence of Parameter(%x). "
            "The number of Calibration Identifiers is zero.", pid);
        return AR_ENOTEXIST;
    }

    glb_persist_pid_map = ACDB_MALLOC(uint32_t, 3 * cal_id_count);

    status = FileManReadBuffer(glb_persist_pid_map, cal_id_count * sz_cal_id_obj_header, &offset);
    if (AR_EOK != status) return status;

	if (AR_EOK != AcdbDataBinarySearch2(
		glb_persist_pid_map/*glb_buf_1*/, cal_id_count * sz_cal_id_obj_header,
		&cal_id_obj, 1, sizeof(CalibrationIdMap)/sizeof(uint32_t),
		&search_index))
	{
		status =  AR_ENOTEXIST;
	}

	ACDB_FREE(glb_persist_pid_map);
	//ACDB_CLEAR_BUFFER(glb_buf_1);

	return status;
}

int32_t IsPidPersistent(uint32_t pid)
{
	int32_t status = AR_EOK;
	uint32_t num_pids = 0;
	uint32_t offset = 0;
	uint32_t search_index = 0;
	uint32_t *persist_pid_map = NULL;
	ChunkInfo ci = { ACDB_CHUNKID_PIDPERSIST, 0, 0 };

    status = ACDB_GET_CHUNK_INFO(&ci);
	if (AR_EOK != status) return status;

    offset = ci.chunk_offset;
    status = FileManGetFilePointer1(
        &persist_pid_map, sizeof(uint32_t), &offset);
    if (AR_EOK != status) return status;

    num_pids = *persist_pid_map;
    if (num_pids == 0)
    {
        return AR_ENOTEXIST;
    }

    status = FileManGetFilePointer1(
        &persist_pid_map, num_pids * sizeof(uint32_t), &offset);
    if (AR_EOK != status) return status;

	if (AR_EOK != AcdbDataBinarySearch2(
		persist_pid_map, num_pids * sizeof(uint32_t),
		&pid, 1, 1,
		&search_index))
	{
		status =  AR_ENOTEXIST;
	}

	return status;
}

int32_t DataProcGetPersistenceType(uint32_t pid, AcdbDataPersistanceType *persistence_type)
{
    int32_t status = AR_EOK;

    if (IsNull(persistence_type))
        return AR_EBADPARAM;

    status = IsPidPersistent(pid);
    if (AR_SUCCEEDED(status))
    {
        *persistence_type = ACDB_DATA_PERSISTENT;
        return status;
    }

    //PID not found in Persistent Map so its either Non-persist or global
    if(status == AR_ENOTEXIST)
    {
        status = IsPidGlobalyPersistent(pid);
        if (AR_SUCCEEDED(status))
        {
            *persistence_type = ACDB_DATA_GLOBALY_PERSISTENT;
            return status;
        }
        else if (status == AR_ENOTEXIST)
        {
            *persistence_type = ACDB_DATA_NON_PERSISTENT;
            status = AR_EOK;
            return status;
        }
    }

    *persistence_type = ACDB_DATA_UNKNOWN;
    ACDB_ERR("Error[%d] - Unable to determine the persistence type of Parameter(0x%x)", status, pid);
    return status;
}

/**
* \brief ConvertSetCalDataReqToMap
*		Converts the input request into a format usable by the
*		delta file manager.
* \param[in] req: Set Data request passed in by the client. The request can be
* AcdbSetCalibrationDataReq or AcdbSetTagDataReq
* \param[in] key_vector_type: The type of key vector used in the requeset(CKV, TKV, etc...)
* \return a map pointer on success, Null on failure
*/
AcdbDeltaKVToSubgraphDataMap *ConvertSetCalDataReqToMap(
	void *req, KeyVectorType key_vector_type, int32_t *status)
{
	AcdbBlob calblob;
	uint32_t blob_offset = 0;
	uint32_t subgraph_id = 0;
    uint32_t num_sg_ids = 0;
    uint32_t *sg_id_list = NULL;
	LinkedListNode *node = NULL;
	AcdbDeltaSubgraphData *subgraph_data = NULL;
    AcdbSetTagDataReq *tag_data_req = NULL;
    AcdbSetCalibrationDataReq *cal_data_req = NULL;
    AcdbGraphKeyVector *key_vector = NULL;
    AcdbDeltaKVToSubgraphDataMap *map =
		ACDB_MALLOC(AcdbDeltaKVToSubgraphDataMap, 1);

    calblob.buf = NULL;
    calblob.buf_size = 0;

    *status = AR_EOK;

	if (IsNull(map))
	{
		*status = AR_ENOMEMORY;
		return NULL;
	}

	ACDB_CLEAR_BUFFER(*map);
    map->map_size = 0;
    map->key_vector_type = key_vector_type;
    switch (key_vector_type)
    {
        case TAG_KEY_VECTOR:
            tag_data_req = (AcdbSetTagDataReq*)req;

            num_sg_ids = tag_data_req->num_sg_ids;
            sg_id_list = tag_data_req->sg_ids;

            calblob.buf_size = tag_data_req->blob_size;
            calblob.buf = tag_data_req->blob;

            map->key_vector_data = ACDB_MALLOC(AcdbModuleTag, 1);
            if (IsNull((AcdbModuleTag*)map->key_vector_data))
            {
                *status = AR_ENOMEMORY;
                ACDB_FREE(map);
                return NULL;
            }

            ((AcdbModuleTag*)map->key_vector_data)->tag_id = tag_data_req->module_tag.tag_id;
            key_vector = &((AcdbModuleTag*)map->key_vector_data)->tag_key_vector;

            key_vector->num_keys =
                tag_data_req->module_tag.tag_key_vector.num_keys;

            key_vector->graph_key_vector =
                ACDB_MALLOC(AcdbKeyValuePair,
                    key_vector->num_keys);


            if (IsNull(key_vector->graph_key_vector))
            {
                *status = AR_ENOMEMORY;
                ACDB_FREE(map);
                return NULL;
            }

            ACDB_MEM_CPY(
                key_vector->graph_key_vector,
                tag_data_req->module_tag.tag_key_vector.graph_key_vector,
                key_vector->num_keys * sizeof(AcdbKeyValuePair));

            break;
        case CAL_KEY_VECTOR:
            cal_data_req = (AcdbSetCalibrationDataReq*)req;

            num_sg_ids = cal_data_req->num_sg_ids;
            sg_id_list = cal_data_req->sg_ids;

            calblob.buf_size = cal_data_req->cal_blob_size;
            calblob.buf = cal_data_req->cal_blob;

            key_vector = ACDB_MALLOC(AcdbGraphKeyVector, 1);
            if (IsNull(key_vector))
            {
                *status = AR_ENOMEMORY;
                ACDB_FREE(map);
                return NULL;
            }

            key_vector->graph_key_vector = NULL;
            map->key_vector_data = key_vector;

            key_vector->num_keys =
                cal_data_req->cal_key_vector.num_keys;

            if (key_vector->num_keys)
            {
                key_vector->graph_key_vector =
                    ACDB_MALLOC(AcdbKeyValuePair,
                        key_vector->num_keys);

                if (IsNull(key_vector->graph_key_vector))
                {
                    *status = AR_ENOMEMORY;
                    ACDB_FREE(key_vector);
                    ACDB_FREE(map);
                    return NULL;
                }

                ACDB_MEM_CPY(
                    key_vector->graph_key_vector,
                    cal_data_req->cal_key_vector.graph_key_vector,
                    key_vector->num_keys * sizeof(AcdbKeyValuePair));
            }
            break;
    default:
        break;
    }

    map->num_subgraphs = num_sg_ids;
	ACDB_CLEAR_BUFFER(map->subgraph_data_list);

	for (uint32_t i = 0; i < num_sg_ids; i++)
	{
		subgraph_id = sg_id_list[i];

		subgraph_data = ACDB_MALLOC(AcdbDeltaSubgraphData, 1);

		if (IsNull(subgraph_data))
		{
			*status = AR_ENOMEMORY;
			break;
		}

		ACDB_CLEAR_BUFFER(subgraph_data->non_global_data);
		ACDB_CLEAR_BUFFER(subgraph_data->global_data);

		node = AcdbListCreateNode(subgraph_data);
		if (IsNull(node))
		{
			*status = AR_ENOMEMORY;
			break;
		}

		AcdbListAppend(&map->subgraph_data_list, node);

		subgraph_data->subgraph_id = subgraph_id;
		subgraph_data->subgraph_data_size = 0;

		blob_offset = 0;

		//Parse Cal Blob and map the subgraph to its modules
		while (blob_offset < calblob.buf_size)
		{
			AcdbDeltaModuleCalData *caldata =
				ACDB_MALLOC(AcdbDeltaModuleCalData, 1);

			if (IsNull(caldata))
			{
				*status = AR_ENOMEMORY;
				break;
			}

			AcdbBlobToCalData(caldata, calblob, &blob_offset);

			if (TRUE == DoesSubgraphContainModule(subgraph_id,
				caldata->module_iid, status) && AR_EOK == *status)
			{
				LinkedListNode *caldata_node = AcdbListCreateNode(caldata);

				if (AR_EOK == IsPidGlobalyPersistent(caldata->param_id))
				{
					//Global (Globaly Persistent)
					subgraph_data->global_data.data_size +=
						3 * sizeof(uint32_t) + caldata->param_size;

					AcdbListAppend(
						&subgraph_data->global_data.cal_data_list,
						caldata_node);
				}
				else
				{
					//Non Global(Persistent and Non-Persistent)
					subgraph_data->non_global_data.data_size +=
						3 * sizeof(uint32_t) + caldata->param_size;

					AcdbListAppend(
						&subgraph_data->non_global_data.cal_data_list,
						caldata_node);
				}
			}
			else
			{
                //ACDB_DBG("Subgraph(0x%x) does not contain Module Instance(0x%x).", subgraph_id, caldata->module_iid);
				FreeCalData(&caldata);
			}
		}

		subgraph_data->subgraph_data_size =
			subgraph_data->non_global_data.data_size +
			subgraph_data->global_data.data_size;

		map->map_size += subgraph_data->subgraph_data_size;
	}

	if (AR_EOK != *status)
	{
		free_map(map);
		map = NULL;
	}
	else
	{
		map->map_size += map->num_subgraphs * sizeof(uint32_t);
	}

	subgraph_data = NULL;
	node = NULL;

	return map;
}

int32_t SetSubraphData(void *req, KeyVectorType key_vector_type)
{
	int32_t status = AR_EOK;
	AcdbDeltaKVToSubgraphDataMap *heap_map = NULL;
    AcdbDeltaKVToSubgraphDataMap *req_map = NULL;
    AcdbGraphKeyVector* key_vector = NULL;

    switch (key_vector_type)
    {
    case TAG_KEY_VECTOR:
        key_vector = &((AcdbSetTagDataReq*)req)->module_tag.tag_key_vector;

        if (IsNull(key_vector))
        {
            ACDB_ERR("The request Tag Key Vector is null");
            return AR_EBADPARAM;
        }
        break;
    case CAL_KEY_VECTOR:
        key_vector = &((AcdbSetCalibrationDataReq*)req)->cal_key_vector;

        if (IsNull(key_vector))
        {
            ACDB_ERR("The request Calibration Key Vector is null");
            return AR_EBADPARAM;
        }
        break;
    default:
        break;
    }

    if (IsNull(key_vector))
    {
        ACDB_ERR("The request Key Vector is null");
        return AR_EBADPARAM;
    }

	status = AcdbSort2(key_vector->num_keys * sizeof(AcdbKeyValuePair),
        key_vector->graph_key_vector, sizeof(AcdbKeyValuePair), 0);

	req_map = ConvertSetCalDataReqToMap(req, key_vector_type, &status);

    if (IsNull(req_map) || AR_FAILED(status))
    {
        ACDB_ERR("Failed to convert Set Data request to map");
        return status;
    }

	status = acdb_heap_ioctl(
		ACDB_HEAP_CMD_GET_MAP,
		(uint8_t*)key_vector,
		(uint8_t*)&heap_map, sizeof(uint32_t));

    key_vector = NULL;

	if (AR_ENOTEXIST == status)
	{
		/**
        * Scenario 1 - Add New CKV-to-Subgraph Map to Heap
		* KV is not in the heap, so skip the comparison and write it to the heap
        */
		status = acdb_heap_ioctl(
			ACDB_HEAP_CMD_ADD_MAP,
			(uint8_t*)req_map,
			NULL, 0);
	}
	else
	{
		/**
		* Scenario 2 & 3
		* Update the subgraph data list for an existing map as well as update
        * module calibration for an existing subgraph.
		* Heap subgraph list is being modified. Any subgraph entry found is
        * moved to the end of the heap maps subgraph list
		*/
		LinkedListNode *heap_sg_data_node = heap_map->subgraph_data_list.p_head;
		LinkedListNode *req_sg_data_node = req_map->subgraph_data_list.p_head;
		AcdbDeltaSubgraphData *heap_sg_data = NULL;
		AcdbDeltaSubgraphData *req_sg_data = NULL;
		LinkedListNode *heap_prev = NULL;
		LinkedListNode *req_prev = NULL;
		LinkedListNode *dont_care = NULL;
		bool_t is_new_sg = FALSE;

		while (!IsNull(req_sg_data_node))
		{
			req_sg_data = (AcdbDeltaSubgraphData*)req_sg_data_node->p_struct;

			if (IsNull(heap_sg_data_node))
			{
				status = AR_EFAILED;
				break;
			}

			do
			{
				if (heap_sg_data_node == dont_care)
				{
					is_new_sg = TRUE;
					break;
				}

				heap_sg_data = (AcdbDeltaSubgraphData*)heap_sg_data_node->p_struct;

				if (heap_sg_data->subgraph_id == req_sg_data->subgraph_id)
				{
					//Perform Calibration Data Diff
					is_new_sg = FALSE;
					AcdbDeltaModuleCalData *heap_caldata = NULL;
					AcdbDeltaModuleCalData *req_caldata = NULL;
					LinkedListNode *heap_caldata_node = NULL;
					LinkedListNode *req_caldata_node = NULL;
					LinkedListNode *req_caldata_prev = NULL;
					bool_t found = FALSE;

					heap_caldata_node = heap_sg_data->non_global_data.cal_data_list.p_head;
					req_caldata_node = req_sg_data->non_global_data.cal_data_list.p_head;

					do
					{
						found = FALSE;
						req_caldata = (AcdbDeltaModuleCalData*)req_caldata_node->p_struct;
						do
						{
							heap_caldata = (AcdbDeltaModuleCalData*)heap_caldata_node->p_struct;

							// Compare <Instance ID, Parameter ID>
							if (0 == ACDB_MEM_CMP(heap_caldata, req_caldata, 2 * sizeof(uint32_t)))
							{
                                /* Replace the old parameter data with then new parameter data by
                                 * subtracting the old parameters size and freeing its data */

                                 // Remove old param data from heap map
                                heap_map->map_size -= heap_caldata->param_size;
                                heap_sg_data->non_global_data.data_size -= heap_caldata->param_size;

                                //Add request maps data size
                                heap_map->map_size += req_caldata->param_size;
                                heap_sg_data->non_global_data.data_size += req_caldata->param_size;

								ACDB_FREE(heap_caldata->param_payload);
								heap_caldata->param_payload = req_caldata->param_payload;
								heap_caldata->param_size = req_caldata->param_size;

								req_caldata->param_payload = NULL;
								req_caldata->param_size = 0;

								found = TRUE;
								break;
							}

						} while (TRUE != AcdbListSetNext(&heap_caldata_node));

						if (!found)
						{
							if (req_caldata_prev != NULL)
							{
							    AcdbListRemove(&req_sg_data->non_global_data.cal_data_list, req_caldata_prev, req_caldata_node);
							}
							else
							{
								AcdbListRemove(&req_sg_data->non_global_data.cal_data_list, NULL, req_caldata_node);
							}

							AcdbListAppend(&heap_sg_data->non_global_data.cal_data_list, req_caldata_node);

                            //Update Map Size and Subgraph data size
                            heap_sg_data->non_global_data.data_size += req_sg_data->non_global_data.data_size;
                            heap_map->map_size += req_sg_data->non_global_data.data_size;

							if (IsNull(req_prev))
							{
								//If the head is removed and added to the new list,
								//reset the request subgraph node to the new head
								req_caldata_node = req_sg_data->non_global_data.cal_data_list.p_head;
							}
							else
							{
								req_caldata_node = req_caldata_prev;
							}
						}

						heap_caldata_node = heap_sg_data->non_global_data.cal_data_list.p_head;
						req_caldata_prev = req_caldata_node;
					} while (TRUE != AcdbListSetNext(&req_caldata_node));

					if (dont_care == NULL)
					{
						dont_care = heap_sg_data_node;
					}

					AcdbListMoveToEnd(&heap_map->subgraph_data_list, &heap_sg_data_node, &heap_prev);
					break;
				}
				else
				{
					is_new_sg = TRUE;
				}

				heap_prev = heap_sg_data_node;

			} while (TRUE != AcdbListSetNext(&heap_sg_data_node));

			//Add any subgraphs that were not found in the heap here
			if (is_new_sg)
			{
				heap_map->map_size += sizeof(req_sg_data->subgraph_id) + req_sg_data->subgraph_data_size;
				heap_map->num_subgraphs++;

				if (req_prev != NULL)
				{
				    AcdbListRemove(&req_map->subgraph_data_list, req_prev, req_sg_data_node);
				}
				else
				{
					AcdbListRemove(&req_map->subgraph_data_list, NULL, req_sg_data_node);
				}

				AcdbListAppend(&heap_map->subgraph_data_list, req_sg_data_node);
				is_new_sg = FALSE;

				if (dont_care == NULL)
				{
					dont_care = req_sg_data_node;
				}
				if (IsNull(req_prev))
				{
					//If the head is removed and added to the new list,
					//reset the request subgraph node to the new head
					req_sg_data_node = req_map->subgraph_data_list.p_head;
					heap_sg_data_node = heap_map->subgraph_data_list.p_head;
					continue;
				}
				else
				{
					req_sg_data_node = req_prev;
				}
			}

			req_prev = req_sg_data_node;
			heap_sg_data_node = heap_map->subgraph_data_list.p_head;
			req_sg_data_node = req_sg_data_node->p_next;
		}

		//Free Request Map
		free_map(req_map);
		req_map = NULL;
	}

	return status;
}

int32_t DataProcSetCalData(AcdbSetCalibrationDataReq *req)
{
    return SetSubraphData(req, CAL_KEY_VECTOR);
}

int32_t DataProcSetTagData(AcdbSetTagDataReq *req)
{
    return SetSubraphData(req, TAG_KEY_VECTOR);
}

int32_t GetSubgraphCalData(
    AcdbDataPersistanceType persist_type, AcdbDataClient client,
	AcdbGraphKeyVector *key_vector,
	uint32_t subgraph_id, uint32_t module_iid, uint32_t param_id,
	uint32_t *blob_offset, AcdbBlob *blob)
{
	int32_t status = AR_EOK;
	int32_t error_code = 0;
	size_t sz_param_padded = 0;
    uint32_t sz_caldata = 0;
	uint32_t padding = 0;
	bool_t found = FALSE;
    AcdbDeltaKVToSubgraphDataMap *heap_map = NULL;
	LinkedListNode *sg_data_node = NULL;
	AcdbDeltaSubgraphData *sg_data = NULL;

	status = acdb_heap_ioctl(
		ACDB_HEAP_CMD_GET_MAP,
		(uint8_t*)key_vector,
		(uint8_t*)&heap_map, sizeof(uint32_t));

	if (AR_EOK != status) return status;

	sg_data_node = heap_map->subgraph_data_list.p_head;

    //Resolve persist type if its unknown
    if (persist_type == ACDB_DATA_UNKNOWN)
    {
        /* TODO: replace this check with DataProcGetPersistenceType(...)
         * when global persistence is supported */
        persist_type = IsPidPersistent(param_id) == AR_EOK ?
            ACDB_DATA_PERSISTENT : ACDB_DATA_NON_PERSISTENT;
    }

	do
	{
		sg_data = (AcdbDeltaSubgraphData*)sg_data_node->p_struct;

		if (subgraph_id == sg_data->subgraph_id)
		{
			LinkedListNode *cur_node = NULL;
			AcdbDeltaModuleCalData *caldata = NULL;

			switch (persist_type)
			{
			case ACDB_DATA_GLOBALY_PERSISTENT:
			{
				cur_node = sg_data->global_data.cal_data_list.p_head;
			}
			break;
			case ACDB_DATA_PERSISTENT:
			case ACDB_DATA_NON_PERSISTENT:
			{
				cur_node = sg_data->non_global_data.cal_data_list.p_head;
			}
			break;
			default:
				return AR_EBADPARAM;
			}

			while (cur_node != NULL)
			{
				caldata = (AcdbDeltaModuleCalData*)cur_node->p_struct;

				if (caldata->module_iid != module_iid ||
					caldata->param_id != param_id)
				{
					status = AR_ENOTEXIST;
					cur_node = cur_node->p_next;
					continue;
				}

				//Collect only persistent data
				if (ACDB_DATA_PERSISTENT == persist_type &&
					AR_EOK != IsPidPersistent(caldata->param_id))
				{
					cur_node = cur_node->p_next;
				}

				//Collect only non-persistent data
				if (ACDB_DATA_NON_PERSISTENT == persist_type &&
					AR_EOK == IsPidPersistent(caldata->param_id))
				{
					cur_node = cur_node->p_next;
				}

				//Collect only globaly persistent data
				if (ACDB_DATA_GLOBALY_PERSISTENT == persist_type &&
					AR_EOK != IsPidGlobalyPersistent(caldata->param_id))
				{
					cur_node = cur_node->p_next;
				}


                status = AR_EOK;
                //accumulate size

                /*
                When ARCT requests for calibration from the heap, padding is not requried.
                When ACDB Clients requests for calibration data, padding is requried since the
                DSP requires 8 byte alignment.
                */
                switch (client)
                {
                case ACDB_DATA_ACDB_CLIENT:
                    sz_param_padded = (size_t)ACDB_ALIGN_8_BYTE((uint32_t)caldata->param_size);
                    padding = (uint32_t)sz_param_padded - caldata->param_size;
                    break;
                case ACDB_DATA_ATS_CLIENT:
                    sz_param_padded = (size_t)caldata->param_size;
                    padding = 0;
                    break;
                default:
                    break;
                }

                sz_caldata = sizeof(AcdbDspModuleHeader) + (uint32_t)sz_param_padded;

				//AcdbDeltaModuleCalData to AcdbBlob
				if (blob->buf != NULL)
				{
                    if (client == ACDB_DATA_ATS_CLIENT)
                    {
                        blob->buf_size += sz_caldata;
                    }

                    //Copy MID, PID, Param Size
                    if (blob->buf_size >= (*blob_offset +
                        sizeof(AcdbDspModuleHeader) - sizeof(uint32_t)))
                    {
                        ACDB_MEM_CPY_SAFE(
                            blob->buf + *blob_offset, sizeof(AcdbDspModuleHeader) - sizeof(uint32_t),
                            caldata, sizeof(AcdbDspModuleHeader) - sizeof(uint32_t));
                        *blob_offset += sizeof(AcdbDspModuleHeader) - sizeof(uint32_t);
                    }
                    else
                    {
                        return AR_ENEEDMORE;
                    }

                    //Insert error code
                    if (blob->buf_size >= (*blob_offset + sizeof(error_code)))
                    {
                        ACDB_MEM_CPY_SAFE(
                            blob->buf + *blob_offset, sizeof(error_code),
                            &error_code, sizeof(error_code));
                        *blob_offset += sizeof(error_code);
                    }
                    else
                    {
                        return AR_ENEEDMORE;
                    }

                    //Payload
                    if (blob->buf_size >= (*blob_offset + caldata->param_size))
                    {
                        ACDB_MEM_CPY_SAFE(
                            blob->buf + *blob_offset, caldata->param_size,
                            caldata->param_payload, caldata->param_size);
                        *blob_offset += caldata->param_size;
                    }
					else
					{
						return AR_ENEEDMORE;
					}

					//Add Padding
					if (padding != 0)
					{
						memset((uint8_t*)blob->buf + *blob_offset, 0, padding);
						*blob_offset += padding;
					}
				}
                else
                {
                    //Moudle IID, Param ID, Param Size, Error Code, Payload
                    blob->buf_size += sz_caldata;
                    *blob_offset += sz_caldata;
                }

				found = TRUE;
				break;
			}
		}

		if (TRUE == found) break;

	} while (TRUE != AcdbListSetNext(&sg_data_node));

    if (!found) status = AR_ENOTEXIST;

	return status;
}

//int32_t DataProcGetCalData()
//{
//
//}
//
//int32_t DataProcGetTagData(AcdbGraphKeyVector *tag_key_vector, uint32_t *blob_offset, AcdbBlob *blob)
//{
//    //Get Tag Data from heap
//    int32_t status = AR_EOK;
//
//    return status;
//}

int32_t DataProcGetHeapInfo(AcdbBufferContext *rsp)
{
    int32_t status = AR_EOK;
    status = acdb_heap_ioctl(ACDB_HEAP_CMD_GET_HEAP_INFO,
        NULL,
        (uint8_t*)rsp, sizeof(AcdbBufferContext));

    if (AR_FAILED(status))
    {
        ACDB_ERR("Failed to get key vector and map info from the heap")
    }
    return status;
}

int32_t WriteCalDataToBuffer(LinkedListNode *caldata_head, AcdbBlob* blob, uint32_t *blob_offset)
{
    int32_t status = AR_EOK;
    AcdbDeltaModuleCalData *caldata = NULL;
    LinkedListNode *cur_node = caldata_head;
    uint32_t offset = *blob_offset;

    if (IsNull(caldata_head)) return AR_ENOTEXIST;

    while (!IsNull(cur_node))
    {
        caldata = (AcdbDeltaModuleCalData*)cur_node->p_struct;

        status = AR_EOK;

        CalDataToAcdbBlob(caldata, *blob, &offset);

        cur_node = cur_node->p_next;
    }

    *blob_offset = offset;
    cur_node = NULL;
    return status;
}

int32_t DataProcGetHeapData(AcdbGraphKeyVector *key_vector, AcdbBufferContext* rsp, uint32_t *blob_offset)
{
    int32_t status = AR_EOK;
    AcdbDeltaKVToSubgraphDataMap *heap_map = NULL;
    LinkedListNode *sg_data_node = NULL;
    AcdbDeltaSubgraphData *sg_data = NULL;
    AcdbGraphKeyVector *heap_map_key_vector = NULL;
    AcdbBlob blob = { 0 };
    uint32_t offset = *blob_offset;
    blob.buf = rsp->buf;
    blob.buf_size = rsp->size;

    status = acdb_heap_ioctl(
        ACDB_HEAP_CMD_GET_MAP,
        (uint8_t*)key_vector,
        (uint8_t*)&heap_map, sizeof(uint32_t));

    if (AR_ENOTEXIST == status)
    {
        ACDB_ERR("No heap entry found for the provided key vector.")
        return status;
    }

    heap_map_key_vector = get_key_vector_from_map(heap_map);
    if (IsNull(heap_map_key_vector))
    {
        ACDB_ERR("Error[%d]: Map retrieved from heap is null",
            AR_EBADPARAM);
            return AR_EBADPARAM;
    }

    //Write Heap map to buffer

    //Write Key Vector
    ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(heap_map_key_vector->num_keys),
        &heap_map_key_vector->num_keys,
        sizeof(heap_map_key_vector->num_keys));
    offset += sizeof(heap_map_key_vector->num_keys);

    uint32_t sz_key_vector = heap_map_key_vector->num_keys * sizeof(AcdbKeyValuePair);
    ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sz_key_vector,
        heap_map_key_vector->graph_key_vector,
        sz_key_vector);
    offset += sz_key_vector;

    //Write Map Size
    ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(heap_map->map_size),
        &heap_map->map_size, sizeof(heap_map->map_size));
    offset += sizeof(heap_map->map_size);

    //Write Number of Subgraphs Data Objs
    ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(heap_map->num_subgraphs),
        &heap_map->num_subgraphs, sizeof(heap_map->num_subgraphs));
    offset += sizeof(heap_map->num_subgraphs);

    //Write Subgraph Data Objs
    sg_data_node = heap_map->subgraph_data_list.p_head;

    do
    {
        sg_data = (AcdbDeltaSubgraphData*)sg_data_node->p_struct;

        //Write Subgraph ID
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->subgraph_id),
            &sg_data->subgraph_id, sizeof(sg_data->subgraph_id));
        offset += sizeof(sg_data->subgraph_id);

        //Write Subgraph Data Size
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->subgraph_data_size),
            &sg_data->subgraph_data_size, sizeof(sg_data->subgraph_data_size));
        offset += sizeof(sg_data->subgraph_data_size);

        //Write Calibration Data (Same code for Global and Non Global Data)
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->non_global_data.data_size),
            &sg_data->non_global_data.data_size, sizeof(sg_data->non_global_data.data_size));
        offset += sizeof(sg_data->non_global_data.data_size);

		ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->non_global_data.cal_data_list.length),
			&sg_data->non_global_data.cal_data_list.length, sizeof(sg_data->non_global_data.cal_data_list.length));
		offset += sizeof(sg_data->non_global_data.cal_data_list.length);

        //blob.buf = &rsp->buf[offset];

        status = WriteCalDataToBuffer(
            sg_data->non_global_data.cal_data_list.p_head,
            &blob, &offset);

        if (AR_SUCCEEDED(status))
        {
            *blob_offset += offset;
        }
        else
        {
            ACDB_DBG("There is no non-global subgraph calibration data in the heap entry");
            status = AR_EOK;
        }

        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->global_data.data_size),
            &sg_data->global_data.data_size, sizeof(sg_data->global_data.data_size));
        offset += sizeof(sg_data->global_data.data_size);

		ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(sg_data->global_data.cal_data_list.length),
			&sg_data->global_data.cal_data_list.length, sizeof(sg_data->global_data.cal_data_list.length));
		offset += sizeof(sg_data->global_data.cal_data_list.length);

        status = WriteCalDataToBuffer(
            sg_data->global_data.cal_data_list.p_head,
            &blob, &offset);

        if (AR_SUCCEEDED(status))
        {
            *blob_offset += offset;
        }
        else
        {
            ACDB_DBG("There is no global subgraph calibration data in the heap entry");
            status = AR_EOK;
        }

    } while (TRUE != AcdbListSetNext(&sg_data_node));

    *blob_offset = offset;

    return status;
}



