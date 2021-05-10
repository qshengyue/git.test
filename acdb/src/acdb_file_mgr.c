/**
*=============================================================================
* \file acdb_file_mgr.c
*
* \brief
*		This file contains the implementation of the acdb file manager
*		interfaces.
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

#include <stdarg.h>
#include "acdb_file_mgr.h"
#include "acdb_parser.h"
#include "acdb_init_utility.h"
#include "acdb_init.h"
#include "ar_osal_error.h"
#include "ar_osal_file_io.h"
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

static AcdbFileManFileInfo gDataFileInfo;
static AcdbFileManPathInfo gtemp_file_path;
static int32_t gACDBFileIndex = -1;
/* ---------------------------------------------------------------------------
* Static Function Declarations and Definitions
*--------------------------------------------------------------------------- */

/**
* \brief
* Retrieves the file name from the given path.
* Ex. path - c:\my\file.acdb file name - file.acdb
*
* \param[in] fpath: the file path to get the file name from
* \param[in] fpath_len: length of fpath
* \param[in/out] fname: the output file name
* \param[out] fname_len: length of the filename
*
* \return none
*/
int32_t GetFileNameFromPath(char* fpath, uint32_t fpath_len, char* fname, uint32_t *fname_len)
{
    if (fpath == NULL || fpath_len == 0)
    {
        ACDB_ERR("Error[%d]: The input file path is empty", AR_EBADPARAM);
        return AR_EBADPARAM;
    }
    else
    {
        //Delta file path is provided, so form the full path
        //Ex. ..\acdb_data + \ + acdb_data.acdb + delta
        uint32_t char_index = fpath_len;

        /* Detect windows\linux style path prefixes
        Absolute Paths
            Ex. C:\acdb_data\ar_acdb.acdb
            Ex. /acdb_data/ar_acdb.acdb
        Relative paths
            Ex. ..\acdb_data\ar_acdb.acdb
            Ex. ../acdb_data/ar_acdb.acdb
        */
        //if (!((fpath[0] == '.' && fpath[1] == '.') ||
        //    (fpath[1] == ':' && fpath[2] == '\\') ||
        //    (fpath[1] == ':' && fpath[2] == '/') ||
        //    fpath[0] == '/'))
        //{
        //    char_index = 0;
        //}

        while (--char_index > 0)
        {
            if (fpath[char_index] == '\\' ||
                fpath[char_index] == '/')
            {
                char_index++;
                break;
            }
        }

        fname = &fpath[char_index];
        if (IsNull(fname))
        {
            ACDB_ERR("Error[%d]: Unable to retrieve file name from %s", AR_EBADPARAM, fpath);
            return AR_EBADPARAM;
        }

        *fname_len = fpath_len - char_index;
    }

    return AR_EOK;
}

int32_t AcdbFileManUpdateFileInfo(AcdbCmdFileInfo *cmdInfo)
{
    int32_t result = AR_EOK;
    uint32_t idx;

    if (IsNull(cmdInfo))
    {
        ACDB_ERR("Acdb File info is null");
        return AR_EFAILED;
    }

    if (gDataFileInfo.file_count == ACDB_MAX_ACDB_FILES)
    {
        ACDB_ERR("Max number of files is %d", ACDB_MAX_ACDB_FILES);
        return AR_EFAILED;
    }

    // check if set acdb data request is a duplicate request.
    // if so take the latest acdb
    for (idx = 0; idx < gDataFileInfo.file_count; idx++)
    {
        if (memcmp(&cmdInfo->chFileName[0], &gDataFileInfo.fInfo[idx].chFileName[0], MAX_FILENAME_LENGTH) == 0)
        {
            cmdInfo->file_handle = NULL;
            return AR_EOK;
        }
    }
    idx = gDataFileInfo.file_count++;
    ACDB_MEM_CPY(&gDataFileInfo.fInfo[idx].chFileName[0], &cmdInfo->chFileName[0], MAX_FILENAME_LENGTH);
    gDataFileInfo.fInfo[idx].filename_len = cmdInfo->filename_len;
    gDataFileInfo.fInfo[idx].file_size = cmdInfo->file_size;
    gDataFileInfo.fInfo[idx].file_buffer = cmdInfo->file_buffer;
    gDataFileInfo.fInfo[idx].file_handle = cmdInfo->file_handle;
    gDataFileInfo.fInfo[idx].file_type = cmdInfo->file_type;

    //Set the Global ACDB File index to the first acdb file initialized
    if (cmdInfo->file_type == ACDB_FILE_TYPE_DATABASE && gACDBFileIndex == -1)
    {
        gACDBFileIndex = idx;
    }

    return result;
}

int32_t AcdbFileManUpdateTempPathInfo(AcdbFileManPathInfo *cmdInfo)
{
    if (IsNull(cmdInfo))
    {
        ACDB_ERR("Error[%d]: The input path info is null", AR_EFAILED);
        return AR_EFAILED;
    }

    ACDB_MEM_CPY(&gtemp_file_path.path[0], &cmdInfo->path[0], MAX_FILENAME_LENGTH);
    gtemp_file_path.path_len = cmdInfo->path_len;

    return AR_EOK;
}

int32_t AcdbFileManReset()
{
    int32_t status = AR_EOK;
    uint32_t idx = 0;
    for (idx = 0; idx < gDataFileInfo.file_count; idx++)
    {
        /* The workspace file is only opened/closed when ACDB clients
         * connect through ATS (see AcdbFileManGetLoadedFileInfo and
         * AcdbFileManGetFileData). By this point its already closed. */
        if (gDataFileInfo.fInfo[idx].file_handle != NULL &&
            gDataFileInfo.fInfo[idx].file_type != ACDB_FILE_TYPE_WORKSPACE)
        {
            status = ar_fclose(gDataFileInfo.fInfo[idx].file_handle);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to close %s", status,
                    gDataFileInfo.fInfo[idx].chFileName);
                status = AR_EFAILED;
            }

            ACDB_FREE(gDataFileInfo.fInfo[idx].file_buffer);
        }

        memset(&gDataFileInfo.fInfo[idx], 0, sizeof(AcdbCmdFileInfo));
    }

    gDataFileInfo.file_count = 0;
    gACDBFileIndex = -1;
    return status;
}

int32_t AcdbFileManGetFileName(uint32_t *findex, AcdbFileManPathInfo *file_name_info)
{
    int32_t status = AR_EOK;

    size_t dst_str_size = sizeof(file_name_info->path);
    //size_t src_str_size = sizeof(gDataFileInfo.fInfo[*findex].chFileName);
    size_t src_str_size = gDataFileInfo.fInfo[*findex].filename_len;

    if (src_str_size > dst_str_size)
    {
        ACDB_ERR("Error[%d]: Unable to copy file name. Source(%s) is "
            "longer than Destination(%s)", AR_EBADPARAM, src_str_size,
            dst_str_size);
        return AR_EBADPARAM;
    }

    if (IsNull(findex) || IsNull(file_name_info))
    {
        ACDB_ERR("Error[%d]: The input parameter(s) are null")
        return AR_EBADPARAM;
    }

    if (*findex < gDataFileInfo.file_count)
    {
        status = ACDB_STR_CPY_SAFE(file_name_info->path, dst_str_size,
            gDataFileInfo.fInfo[*findex].chFileName, src_str_size);

        if (AR_EOK != status) return status;

        file_name_info->path_len = \
            gDataFileInfo.fInfo[*findex].filename_len;
    }
    else
    {
        ACDB_ERR("Error[%d]: The file index is greater than the file count");
        return AR_EBADPARAM;
    }

    return status;
}

int32_t AcdbFileManCheckQwspFileIsLoaded()
{
    int32_t status = AR_EOK;
    uint32_t idx = 0;
    uint32_t wksp_file_count = 0;
    for (idx = 0; idx < gDataFileInfo.file_count; idx++)
    {
        if (gDataFileInfo.fInfo[idx].file_type == ACDB_FILE_TYPE_WORKSPACE)
        {
            wksp_file_count += 1;
        }
    }

    if (wksp_file_count != 1)
    {
        status = AR_EFAILED;
        if (wksp_file_count == 0)
        {
            ACDB_ERR("File Manager found no workspace file.");
        }
        else
        {
            ACDB_ERR("File Manager found multiple workspace files."
                "Only once workspace file is permitted");
        }
    }

    return status;
}

int32_t AcdbFileManQwspFileIO(AcdbCmdFileInfo *qwspInfo, uint32_t fileOp)
{
    int32_t status = AR_EOK;
    ar_fhandle fhandle = NULL;
    if (IsNull(qwspInfo))
    {
        ACDB_ERR("Workspace file info is null");
        return AR_EBADPARAM;
    }

    switch (fileOp)
    {
    case 0://Open QWSP
        status = ar_fopen(&fhandle, qwspInfo->chFileName,
            AR_FOPEN_READ_ONLY);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Failed to open the workspace file.");
        }
        qwspInfo->file_handle = fhandle;

        qwspInfo->file_size = (uint32_t)ar_fsize(qwspInfo->file_handle);
        break;
    case 1://Close QWSP
        status = ar_fclose(qwspInfo->file_handle);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Failed to close the workspace file.");
        }
        break;
    default:
        break;
    }

    return status;
}

int32_t AcdbFileManGetLoadedFileInfo(AcdbFileManBlob *rsp)
{
    int32_t status = AR_EOK;

    /*
    The format of the output is:

    +-----------------------------+
    |            #files           |
    +-----------------------------+
    +-----------------------------+
    | fsize | fnamelength | fname | File #1
    +-----------------------------+
    ...
    ...
    +-----------------------------+
    | fsize | fnamelength | fname | File #N
    +-----------------------------+
    */

    //Total size of the response
    uint32_t sz_file_info_rsp = 0;
    uint32_t offset = 0;
    uint32_t i = 0;
    uint32_t file_count = gDataFileInfo.file_count;
    AcdbCmdFileInfo *finfo = NULL;

    status = AcdbFileManCheckQwspFileIsLoaded();
    if (AR_FAILED(status))
    {
        return status;
    }

    sz_file_info_rsp += sizeof(file_count);

    //Get total size of response
    for (i = 0; i < file_count; i++)
    {
        finfo = &gDataFileInfo.fInfo[i];

        //Open the workspace file to prep for download
        if (finfo->file_type == ACDB_FILE_TYPE_WORKSPACE)
        {
            status = AcdbFileManQwspFileIO(finfo, 0);//Open
            if (AR_FAILED(status)) return status;
        }

        //Close the workspace
        if (finfo->file_type == ACDB_FILE_TYPE_WORKSPACE)
        {
            status = AcdbFileManQwspFileIO(finfo, 1);//Close
            if (AR_FAILED(status)) return status;
        }

        sz_file_info_rsp += sizeof(finfo->file_size);
        sz_file_info_rsp += sizeof(finfo->filename_len);
        sz_file_info_rsp += finfo->filename_len;
    }

    if (sz_file_info_rsp > rsp->size)
    {
        ACDB_ERR("There isnt enough memory to copy file information");
        return AR_ENOMEMORY;
    }

    ACDB_MEM_CPY_SAFE(rsp->buf, sizeof(uint32_t), &file_count, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint32_t *tmp_int = NULL;
    char *tmp_char = NULL;


    for (i = 0; i < file_count; i++)
    {
        finfo = &gDataFileInfo.fInfo[i];

        tmp_int = &finfo->file_size;
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(uint32_t),
            tmp_int, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        tmp_int = &finfo->filename_len;
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(uint32_t),
            tmp_int, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        tmp_char = &finfo->chFileName[0];
        ACDB_MEM_CPY_SAFE(&rsp->buf[offset], *tmp_int,
            tmp_char,
            *tmp_int);
        offset += *tmp_int;
    }

    tmp_int = NULL;
    tmp_char = NULL;

    rsp->bytes_filled = sz_file_info_rsp;

    return status;
}

int32_t AcdbFileManGetFileData(AcdbFileManGetFileDataReq *req, AcdbFileManBlob *rsp)
{
    int32_t status = AR_EOK;
    uint32_t i = 0;
    uint32_t data_copy_size = 0;
    size_t bytes_read = 0;
    char *fname = NULL;
    uint32_t fname_len = 0;
    //Number of *.acdb files + *.qwsp file
    uint32_t file_count = gDataFileInfo.file_count;
    AcdbCmdFileInfo *finfo = NULL;

    if (req == NULL || rsp == NULL)
    {
        ACDB_ERR("ACDB_FILEMGR: Received invalid input/output params\n");
        return AR_EFAILED;
    }

    for (i = 0; i < file_count; i++)
    {
        finfo = &gDataFileInfo.fInfo[i];

        status = GetFileNameFromPath(&finfo->chFileName[0], finfo->filename_len, fname, &fname_len);

        fname = &finfo->chFileName[finfo->filename_len - fname_len];

        if (AR_FAILED(status) || fname == NULL)
        {
            ACDB_ERR("Error[%d]: failed to get file name from path", status);
            return status;
        }

        data_copy_size = 0;
        if (IsNull(strstr((char*)fname, (char*)req->file_name)))
        {
            status = AR_ENORESOURCE;
            continue;
        }

        status = AR_EOK;
        if (rsp->size < req->file_name_len)
        {
            ACDB_ERR("ACDB_FILEMGR: Insufficient memory buffer provided to copy the requested length of file data");
            return AR_EFAILED;
        }
        if (finfo->file_size < req->file_offset)
        {
            ACDB_ERR("ACDB_FILEMGR: Invalid offset provided to copy the data from the file");
            return AR_EFAILED;
        }
        if ((finfo->file_size - req->file_offset) < req->file_data_len)
        {
            data_copy_size = finfo->file_size - req->file_offset;
        }
        else
        {
            data_copy_size = req->file_data_len;
        }

        //Open the workspace file for downloading its contents
        if (finfo->file_type == ACDB_FILE_TYPE_WORKSPACE)
        {
            status = AcdbFileManQwspFileIO(finfo, 0);//Open
            if (AR_FAILED(status)) return status;

            status = ar_fseek(finfo->file_handle, req->file_offset, AR_FSEEK_BEGIN);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to seek to file offset %d", status, req->file_offset);
            }

            status = ar_fread(finfo->file_handle, rsp->buf, data_copy_size, &bytes_read);
            if (bytes_read != data_copy_size)
            {
                ACDB_DBG("Expected to read %d bytes, but read %d bytes", data_copy_size, bytes_read);
                return status;
            }
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read data for %c", status, finfo->chFileName);
                return status;
            }
        }
        else if (finfo->file_type == ACDB_FILE_TYPE_DATABASE)
        {
            status = FileManReadBuffer(rsp->buf, data_copy_size, &req->file_offset);
            if (AR_FAILED(status))
            {
                ACDB_ERR("Error[%d]: Failed to read data for %s", status, finfo->chFileName);
                return status;
            }
        }
        else
        {
            ACDB_ERR("Error[%d]: The file type for %s is unknown", AR_EUNEXPECTED, finfo->chFileName);
            return AR_EUNEXPECTED;
        }

        //Close the workspace file when its finished being downloaded
        //if (finfo->file_type == ACDB_FILE_TYPE_WORKSPACE &&
        //    req->file_offset + bytes_read >= finfo->file_size)
        //{
        //    status = AcdbFileManQwspFileIO(finfo, 1);//Close
        //    if (AR_FAILED(status)) return status;
        //}

        if (finfo->file_type == ACDB_FILE_TYPE_WORKSPACE)
        {
            status = AcdbFileManQwspFileIO(finfo, 1);//Close
            if (AR_FAILED(status)) return status;
        }

        finfo = NULL;
        rsp->bytes_filled = data_copy_size;
        return status;
    }
    return status;
}

int32_t acdb_file_man_ioctl(uint32_t cmd_id,
    uint8_t *req,
    uint32_t sz_req,
    uint8_t *rsp,
    uint32_t sz_rsp)
{
    int32_t status = AR_EOK;
    switch (cmd_id)
    {
    case ACDB_FILE_MAN_SET_FILE_INFO:
    {
        if (req == NULL || sz_req != sizeof(AcdbCmdFileInfo))
        {
            return AR_EBADPARAM;
        }
        status = AcdbFileManUpdateFileInfo((AcdbCmdFileInfo *)req);
        break;
    }
    case ACDB_FILE_MAN_SET_TEMP_PATH_INFO:
    {
        if (req == NULL || sz_req != sizeof(AcdbFileManPathInfo))
        {
            return AR_EBADPARAM;
        }
        status = AcdbFileManUpdateTempPathInfo(
            (AcdbFileManPathInfo *)req);
        break;
    }
    case ACDB_FILE_MAN_RESET:
    {
        status = AcdbFileManReset();
        break;
    }
    case ACDB_FILE_MAN_GET_AVAILABLE_FILE_SLOTS:
    {
        int32_t no_of_avail_slots = 0;

        if (rsp == NULL || sz_rsp != sizeof(uint32_t))
        {
            return AR_EBADPARAM;
        }

        no_of_avail_slots = ACDB_MAX_ACDB_FILES - gDataFileInfo.file_count;
        ACDB_MEM_CPY(rsp, &no_of_avail_slots, sizeof(int32_t));
        status = AR_EOK;
        break;
    }
    case ACDB_FILE_MAN_GET_FILE_NAME:
    {
        if (req == NULL || sz_req != sizeof(uint32_t) ||
            rsp == NULL || sz_rsp != sizeof(AcdbFileManPathInfo))
        {
            ACDB_DBG("ACDBFILE_MGR: Received invalid input/output params\n");
            return AR_EBADPARAM;
        }
        else
        {
            uint32_t *pfileIndex = (uint32_t *)req;
            AcdbFileManPathInfo *pfileNameInfo = (AcdbFileManPathInfo *)rsp;
            status = AcdbFileManGetFileName(pfileIndex, pfileNameInfo);
        }
        break;
    }
    case ACDB_FILE_MAN_GET_LOADED_FILES_INFO:
    {
        if (rsp == NULL || sz_rsp != sizeof(AcdbFileManBlob))
        {
            ACDB_ERR("Received invalid output params");
            return AR_EBADPARAM;
        }
        else
        {
            AcdbFileManBlob *response = (AcdbFileManBlob*)rsp;
            status = AcdbFileManGetLoadedFileInfo(response);
        }
        break;
    }
    case ACDB_FILE_MAN_GET_LOADED_FILE_DATA:
    {
        if (req == NULL || sz_req == 0 ||
            rsp == NULL || sz_rsp != sizeof(AcdbFileManBlob))
        {
            ACDB_ERR("Received invalid input/output params");
            return AR_EBADPARAM;
        }
        else
        {
            AcdbFileManGetFileDataReq *request = (AcdbFileManGetFileDataReq*)req;
            AcdbFileManBlob *response = (AcdbFileManBlob*)rsp;
            status = AcdbFileManGetFileData(request, response);
        }
        break;
    }
    case ACDB_FILE_MAN_GET_TEMP_PATH_INFO:
    {
        if (rsp == NULL || sz_rsp != sizeof(AcdbFileManPathInfo))
        {
            return AR_EBADPARAM;
        }

        ((AcdbFileManPathInfo*)rsp)->path_len = gtemp_file_path.path_len;
        ACDB_MEM_CPY(&((AcdbFileManPathInfo*)rsp)->path[0],
            &gtemp_file_path.path[0], gtemp_file_path.path_len);

        status = AR_EOK;
        break;
    }
    default:
        status = AR_EUNSUPPORTED;
        ACDB_ERR("Unsupported Command[%08X]", cmd_id);
    }
    return status;
}

int32_t AcdbGetChunkInfo(uint32_t chkID, uint32_t* chkBuf, uint32_t* chkLen)
{
    if(gACDBFileIndex == -1)
	    return AR_ENORESOURCE;

    int32_t res = AcdbFileGetChunkInfo(gDataFileInfo.fInfo[gACDBFileIndex].file_buffer,
        gDataFileInfo.fInfo[gACDBFileIndex].file_size, chkID, chkBuf, chkLen);
    if (res != ACDB_PARSE_SUCCESS) {
        ACDB_ERR("Failed to get information for Chunk[%08X]", chkID);
		return AR_ENOTEXIST;
    }

    if (*chkLen <= 0)
    {
        ACDB_ERR("The requested Chunk[%08X] is empty");
        return AR_ENOTEXIST;
    }

    return AR_EOK;
}

int32_t AcdbGetChunkInfo2(uint32_t count, ...)
{
    int32_t status = AR_EOK;
    va_list args;
    ChunkInfo *ci = NULL;
    va_start(args, count);

    for (uint32_t i = 0; i < count; i++)
    {
        ci = va_arg(args, ChunkInfo*);

        if (IsNull(ci))
            return AR_EBADPARAM;

        status = AcdbGetChunkInfo(ci->chunk_id, &ci->chunk_offset, &ci->chunk_size);
        if (AR_FAILED(status))
        {
            return status;
        }
    }

    va_end(args);
    return status;
}

int file_seek(long offset, ar_fseek_reference_t origin)
{
    if(gACDBFileIndex == -1)
	return AR_ENORESOURCE;
    return ar_fseek(gDataFileInfo.fInfo[gACDBFileIndex].file_handle, offset, origin);
}

int file_read(void *buffer, size_t length)
{
    if(gACDBFileIndex == -1)
	return AR_ENORESOURCE;
    size_t bytes_read = 0;
    return ar_fread(gDataFileInfo.fInfo[gACDBFileIndex].file_handle, buffer, length, &bytes_read);
}

int32_t FileSeekRead(void* buffer, size_t read_size, uint32_t *offset)
{
    if(gACDBFileIndex == -1)
	return AR_ENORESOURCE;
    int32_t status = AR_EOK;
    size_t bytes_read = 0;

    status = ar_fseek(gDataFileInfo.fInfo[gACDBFileIndex].file_handle,
        *offset, AR_FSEEK_BEGIN);

    if (AR_FAILED(status)) return status;

    status = ar_fread(gDataFileInfo.fInfo[gACDBFileIndex].file_handle,
        buffer, read_size, &bytes_read);

    *offset += (uint32_t)bytes_read;

    return status;
}

int32_t FileManReadBuffer(void* buffer, size_t read_size, uint32_t *offset)
{
    int32_t status = AR_EOK;

    if (read_size == 0)
        return status;

    if (IsNull(buffer) || IsNull(offset))
        return AR_EBADPARAM;

    if (gACDBFileIndex == -1)
        return AR_ENORESOURCE;

    uint8_t *buffer_ptr =
        (uint8_t*)gDataFileInfo.fInfo[gACDBFileIndex].file_buffer;

    buffer_ptr += *offset;

    if (*offset + read_size > gDataFileInfo.fInfo[gACDBFileIndex].file_size)
    {
        return AR_EBADPARAM;
    }

    status = ar_mem_cpy(buffer, read_size, buffer_ptr, read_size);
    if (AR_FAILED(status)) return status;

    *offset += (uint32_t)read_size;

    return status;
}

int32_t FileManGetFilePointer1(void** file_ptr, size_t data_size, uint32_t *offset)
{
    int32_t status = AR_EOK;

    if (IsNull(file_ptr) || IsNull(offset))
        return AR_EBADPARAM;

    if (gACDBFileIndex == -1)
        return AR_ENORESOURCE;

    *file_ptr = (uint8_t*)gDataFileInfo
        .fInfo[gACDBFileIndex]
        .file_buffer + *offset;

    *offset += (uint32_t)data_size;

    return status;
}

int32_t FileManGetFilePointer2(void** file_ptr, uint32_t offset)
{
    int32_t status = AR_EOK;

    if (IsNull(file_ptr))
        return AR_EBADPARAM;

    if (gACDBFileIndex == -1)
        return AR_ENORESOURCE;

    *file_ptr = (uint8_t*)gDataFileInfo
        .fInfo[gACDBFileIndex]
        .file_buffer + offset;

    return status;
}
