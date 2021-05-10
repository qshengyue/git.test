/**
*==============================================================================
* \file acdb_heap.h
*
* \brief
*      Manages the calibration data set by ACDB SW clients in RAM.
*
* \copyright
*  Copyright (c) 2019-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
* 
*==============================================================================
*/

#include "acdb_heap.h"
#include "acdb_common.h"
#include <stdio.h>


/**
===============================================================================
*   Defines
*==============================================================================
*/
#define ACDB_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define ACDB_MIN(a,b) (((a) < (b)) ? (a) : (b))
#define HEX_DIGITS 8
/*
===============================================================================
*	Notes
*==============================================================================
    Handling Muliple Delta ACDB files
        Each Tree Root must be associated with a findex(file index). This will
        keep the heaps for multiple files separate. Use the HeapInfo structure
*/
/**
===============================================================================
*	Global and Static variables
*==============================================================================
*/
static TreeNode *g_root = NULL;

//TODO: Used for managing heap data for multiple acdb files
//static AcdbDeltaHeapInfo g_delta_data_heap_info;

/**
*==============================================================================
*	Helper functions
*==============================================================================
*/

/**
* \breif
*	Converts a key vector to a string.
*	Caller is responsible for freeing the returned string pointer
*
* \return pointer to key vector string on succes, null on failure
*/
char *key_vector_to_string(const AcdbGraphKeyVector *key_vector, size_t *sz_str_key_vector)
{
	/* Example
	Format	: <key, value>...<key, value>
	Input	: <0xA, 0x1><0xB, 0x1><0xC, 0x1>
	Output  : 'A1B1C1' in Big Endian
	*/
	if (IsNull(key_vector)) return NULL;

	uint32_t offset = 0;
	//Max Key Vector String length = 8 * 2 * key_vector->num_keys;
	size_t str_size = 0;
	//8 Hex Digits in 32-bit integer * 2 members in <key,value>
    uint32_t sz_str_kv_pair = 16 + 1;
	char tmp[16 + 1] = "";
	char *key_vector_str = NULL;

	for (uint32_t i = 0; i < key_vector->num_keys; i++)
	{
        if (0 > ar_sprintf(tmp, sz_str_kv_pair, "%08x%08x",
            key_vector->graph_key_vector[i].key,
            key_vector->graph_key_vector[i].value))
        {
            return NULL;
        }
        str_size += ar_strlen(tmp, (size_t)sz_str_kv_pair);
	}

	str_size += 1;// '\0'

	key_vector_str = ACDB_MALLOC(char, str_size);
	if (IsNull(key_vector_str)) return NULL;
	memset(key_vector_str, 0, str_size);

	for (uint32_t i = 0; i < key_vector->num_keys; i++)
	{
        if (key_vector_str + offset < key_vector_str + str_size)
        {
            if (0 > ar_sprintf(key_vector_str + offset, sz_str_kv_pair,
                "%08x%08x",
                key_vector->graph_key_vector[i].key,
                key_vector->graph_key_vector[i].value))
            {
                ACDB_FREE(key_vector_str);
                return NULL;
            }
            offset = (uint32_t)ar_strlen(key_vector_str, str_size);
        }
	}
	
	*sz_str_key_vector = offset;
	return key_vector_str;
}

/**
* \breif
*	Get Calibration or Tag Key Vector from the KV to Subgraph Map
*
* \param[in/out] map: map to extract key vector from
* \return pointer to key vector
*/
AcdbGraphKeyVector *get_key_vector_from_map(AcdbDeltaKVToSubgraphDataMap* map)
{
    AcdbGraphKeyVector *kv = NULL;

    if (IsNull(map))
        return NULL;

    if (IsNull(map->key_vector_data))
        return NULL;

    switch (map->key_vector_type)
    {
    case TAG_KEY_VECTOR:
        kv = &((AcdbModuleTag*)map->key_vector_data)->tag_key_vector;
        break;
    case CAL_KEY_VECTOR:
        kv = ((AcdbGraphKeyVector*)map->key_vector_data);
        break;
    default:
        break;
    }

    return kv;
}

bool_t is_leaf(TreeNode *tnode)
{
    if (tnode == NULL)
        return FALSE;

    //or if the depth of the node is zero

    //No left or right subtrees, but has a parent, so it must be a leaf
    if (IsNull(tnode->left) && IsNull(tnode->right) &&
        !IsNull(tnode->parent))
    {
        return TRUE;
    }
    return FALSE;
}

bool_t is_tree_empty(TreeNode *root)
{
    if (IsNull(root) ||
        (IsNull(root->p_struct) &&
            IsNull(root->left) &&
            IsNull(root->right) &&
            IsNull(root->parent)))
    {
        return TRUE;
    }
    return FALSE;
}

bool_t is_tree(TreeNode *tnode)
{
    return !is_leaf(tnode);
}

bool_t is_balanced(TreeNode *tnode, int32_t *factor)
{
    //difference = right.depth - left.depth
    //-1 - left heavy
    // 0 - balanced
    // 1 - right heavy
    int32_t diff = 0;

    //if both left and right are non-null this is the equivalent of
    //diff = tnode->right->depth - tnode->left->depth;
    if (!is_leaf(tnode))
    {
        if (!IsNull(tnode->right))
        {
            diff += tnode->right->depth;
        }
        if (!IsNull(tnode->left))
        {
            diff += -1 * tnode->left->depth;
        }
    }

    *factor = diff;

    if (diff < -1 || diff > 1)
        return FALSE;

    return TRUE;
}

void update_depth(TreeNode *tnode)
{
    TreeNode *p_cur_node;
    p_cur_node = tnode;

    while (p_cur_node != NULL)
    {
        if (IsNull(p_cur_node))
        {
            break;
        }

        if (!is_leaf(p_cur_node) && !IsNull(p_cur_node))
        {
            if (IsNull(p_cur_node->right))
            {
                p_cur_node->depth = 1 + p_cur_node->left->depth;
            }
            else if (IsNull(p_cur_node->left))
            {
                p_cur_node->depth = 1 + p_cur_node->right->depth;
            }
            else
            {
                p_cur_node->depth = 1 + ACDB_MAX(p_cur_node->left->depth, p_cur_node->right->depth);
            }
        }
        else if (is_leaf(p_cur_node) && !IsNull(p_cur_node))
        {
            p_cur_node->depth = 1;
        }

        p_cur_node = p_cur_node->parent;
    }

    p_cur_node = NULL;
}

int32_t tree_direction(TreeNode *p_x, TreeNode *p_y)
{
    if (IsNull(p_x) || IsNull(p_y))
    {
        return TREE_DIR_NONE;
    }
    if (IsNull(p_x->p_struct) || IsNull(p_y->p_struct))
    {
        return TREE_DIR_NONE;
    }

    int32_t direction = strcmp(
        ((KVSubgraphMapBin*)p_x->p_struct)->str_key_vector,
        ((KVSubgraphMapBin*)p_y->p_struct)->str_key_vector);

    if (direction < 0)
    {
        return TREE_DIR_LEFT;
    }
    else if (direction > 0)
    {
        return TREE_DIR_RIGHT;
    }

    return TREE_DIR_NONE;
}

void rotate_right(TreeNode *p_a, TreeNode *p_c)
{
    //Left heavy means right rotation
    //rotate right
    //    a
    //  b     =>  b
    //c         c   a

    TreeNode *p_b = p_c->parent;

    p_b->parent = p_a->parent;
    p_a->parent = p_b;

    if (!IsNull(p_b->right))
    {
        p_b->right->parent = p_a;
    }

    p_a->left = p_b->right;
    p_b->right = p_a;

    if (TREE_DIR_LEFT == tree_direction(p_b, p_b->parent))
    {
        p_b->parent->left = p_b;
    }
    else if (TREE_DIR_RIGHT == tree_direction(p_b, p_b->parent))
    {
        p_b->parent->right = p_b;
    }

    update_depth(p_a);
    update_depth(p_c);
}

void rotate_left(TreeNode *p_a, TreeNode *p_c)
{
    //Right heavy means left rotation
    //rotate left
    //a
    //  b     =>     b
    //   c         a   c
    TreeNode *p_b = p_c->parent;

    p_b->parent = p_a->parent;
    p_a->parent = p_b;

    if (!IsNull(p_b->left))
    {
        p_b->left->parent = p_a;
    }

    p_a->right = p_b->left;
    p_b->left = p_a;

    if (TREE_DIR_LEFT == tree_direction(p_b, p_b->parent))
    {
        p_b->parent->left = p_b;
    }
    else if (TREE_DIR_RIGHT == tree_direction(p_b, p_b->parent))
    {
        p_b->parent->right = p_b;
    }

    update_depth(p_a);
    update_depth(p_c);
}

void rotate(TreeNode *p_a, TreeNode *p_c, int32_t factor)
{
    //a - grand parent
    //b - parent
    //c - child
    TreeNode *p_b = p_c->parent;

    //Single Rotation (Left, Right)
    if (factor < -1 && TREE_DIR_LEFT == tree_direction(p_c, p_b))
    {
        rotate_right(p_a, p_c);
    }
    else if (factor > 1 && TREE_DIR_RIGHT == tree_direction(p_c, p_b))
    {
        rotate_left(p_a, p_c);
    }

    //Double Rotation (Left-Right, Right-Left)
    else if (factor < -1 && TREE_DIR_RIGHT == tree_direction(p_c, p_b))
    {
        //Left heavy means right rotation
        //Left-Right, rotate left, the rotate right
        //    a         a
        //  b     =>  c      =>    c
        //    c     b            b   a     
        //rotate_left(p_b, p_c);

        p_c->parent = p_b->parent;
        p_b->parent = p_c;
        p_a->left = p_c;

        p_b->right = p_c->left;
        p_c->left = p_b;

        rotate_right(p_a, p_b);
    }
    else if (factor > 1 && TREE_DIR_LEFT == tree_direction(p_c, p_b))
    {
        //Right heavy means left rotation
        //Right-Left, rotate right, the rotate left
        //a           a    
        //  b     =>    c      =>    c
        //c               b        a   b
        //rotate_right(p_b, p_c);

        p_c->parent = p_b->parent;
        p_b->parent = p_c;
        p_a->right = p_c;

        p_b->left = p_c->right;
        p_c->right = p_b;

        rotate_left(p_a, p_b);
    }
}

KVSubgraphMapBin *create_map_bin()
{
    KVSubgraphMapBin *bin = ACDB_MALLOC(KVSubgraphMapBin, 1);

    if (bin == NULL) return NULL;

    bin->str_key_vector_size = 0;
    bin->str_key_vector = NULL;
    bin->map = NULL;
    //bin->list.length = 0;
    //bin->list.p_head = NULL;
    //bin->list.p_tail = NULL;

    return bin;
}

TreeNode *create_tree_node(void *p_struct)
{
    TreeNode *bin_node = ACDB_MALLOC(TreeNode, 1);

    if (bin_node == NULL) return NULL;

    bin_node->depth = 1;
    bin_node->left = NULL;
    bin_node->right = NULL;
    bin_node->parent = NULL;
    bin_node->p_struct = p_struct;

    return bin_node;
}

LinkedListNode *create_list_node(void *p_struct)
{
    LinkedListNode *lnode = ACDB_MALLOC(LinkedListNode, 1);

    if (lnode == NULL) return NULL;

    lnode->p_next = NULL;
    lnode->p_struct = p_struct;

    return lnode;
}

void free_persistance_data(AcdbDeltaPersistanceData *persistance_data)
{
    if (IsNull(persistance_data)) return;

    AcdbDeltaModuleCalData *p_cal_data = NULL;
    LinkedListNode *cur_node = persistance_data->cal_data_list.p_head;

    while (!(IsNull(cur_node)))
    {
        if (!IsNull(cur_node->p_struct))
        {
            p_cal_data = ((AcdbDeltaModuleCalData*)cur_node->p_struct);
            ACDB_FREE((uint8_t*)p_cal_data->param_payload);
            ACDB_FREE(p_cal_data);
        }

        cur_node = cur_node->p_next;
    }

    AcdbListClear(&persistance_data->cal_data_list);

    p_cal_data = NULL;
    cur_node = NULL;
}

void free_key_vector_data(KeyVectorType key_vector_type, void* key_vector_data)
{
    AcdbKeyValuePair* kv_pair = NULL;
    if (IsNull(key_vector_data)) return;

    switch (key_vector_type)
    {
    case TAG_KEY_VECTOR:
        kv_pair = ((AcdbModuleTag*)
            key_vector_data)->tag_key_vector.graph_key_vector;

        if(!IsNull(kv_pair)) ACDB_FREE(kv_pair);

        break;
    case CAL_KEY_VECTOR:
        kv_pair = ((AcdbGraphKeyVector*)
            key_vector_data)->graph_key_vector;

        if (!IsNull(kv_pair)) ACDB_FREE(kv_pair);

        break;
    default:
        return;
    }

    ACDB_FREE(key_vector_data);
}

void free_map(AcdbDeltaKVToSubgraphDataMap *map)
{
    if (IsNull(map)) return;

    //AcdbDeltaModuleCalData *p_cal_data = NULL;
    LinkedListNode *sg_data_node = NULL;
    AcdbDeltaSubgraphData *sg_data = NULL;

    sg_data_node = map->subgraph_data_list.p_head;

    do
    {
        if (IsNull(sg_data_node)) break;

        sg_data = (AcdbDeltaSubgraphData*)sg_data_node->p_struct;
        free_persistance_data(&sg_data->non_global_data);
        free_persistance_data(&sg_data->global_data);
        ACDB_FREE(sg_data);

    } while (TRUE != AcdbListSetNext(&sg_data_node));

    AcdbListClear(&map->subgraph_data_list);

    free_key_vector_data(map->key_vector_type, map->key_vector_data);
    ACDB_FREE(map);
}

void free_bin(KVSubgraphMapBin **bin)
{
    free_map((*bin)->map);
    ACDB_FREE(*bin);
    *bin = NULL;
}

/**
* \breif acdb_heap_insert
*	Insert a node into the tree
*
* \return 0 on succes, non-zero on failure
*/
int32_t acdb_heap_insert(TreeNode *tnode)
{
    TreeNode *t = g_root;
    TreeNode *u = NULL;
    int32_t factor = 0;

    if (TRUE == is_tree_empty(t))
    {
        g_root = tnode;
        return 0;
    }

    while (TRUE)
    {
        if (TREE_DIR_LEFT == tree_direction(tnode, t))
        {
            if (t->left == NULL)
            {
                t->left = tnode;
                tnode->parent = t;
                update_depth(t);
                break;
            }
            else
            {
                t = t->left;
            }
        }
        else if (TREE_DIR_RIGHT == tree_direction(tnode, t))
        {
            if (t->right == NULL)
            {
                t->right = tnode;
                tnode->parent = t;
                update_depth(t);
                break;
            }
            else
            {
                t = t->right;
            }
        }
        else
        {
            break;
        }
    }

    //Check balance and rotate when nessesary. Set the new root
    u = tnode;
    while (t != NULL)
    {
        if (is_balanced(t, &factor))
        {
            if (t->parent == NULL)
            {
                g_root = t;
            }

            if (t->depth - u->depth >= 2)
            {
                u = u->parent;
            }

            t = t->parent;
        }
        else
        {
            //t - grandparent
            //  - parent
            //u - child
            rotate(t, u, factor);
        }
    }

    return AR_EOK;
}

//int32_t acdb_heap_remove(TreeNode *tnode)
//{
//	int32_t status = AR_ENOTIMPL;
//	//the reverse of insert
//	//copy the same code but instead of inserting the node, ACDB_FREE the memory associated with the node
//	return status;
//}

int32_t acdb_heap_get_bin(size_t size_str_key_vector, char* str_key_vector, KVSubgraphMapBin **bin)
{
    int32_t status = AR_EOK;
    TreeNode *t = g_root;
    TreeNode tnode;
    KVSubgraphMapBin tmp_bin;
    int32_t tree_dir = TREE_DIR_NONE;
    if (TRUE == is_tree_empty(t))
    {
        return AR_ENOTEXIST;
    }

    tmp_bin.str_key_vector = str_key_vector;
    tmp_bin.str_key_vector_size = (uint32_t)(size_str_key_vector);
    tnode.depth = 1;
    tnode.left = NULL;
    tnode.right = NULL;
    tnode.parent = NULL;
    tnode.p_struct = &tmp_bin;

    while (TRUE)
    {
        tree_dir = tree_direction(&tnode, t);

        switch (tree_dir)
        {
        case TREE_DIR_LEFT:
        {
            if (t->left == NULL)
            {
                status = AR_ENOTEXIST;
                break;
            }

            t = t->left;
            break;
        }
        case TREE_DIR_RIGHT:
        {
            if (t->right == NULL)
            {
                status = AR_ENOTEXIST;
                break;
            }

            t = t->right;
            break;
        }
        case TREE_DIR_NONE:
        {
            *bin = (KVSubgraphMapBin*)t->p_struct;
            break;
        }
        default:
            break;
        }

        if (TREE_DIR_NONE == tree_dir ||
            AR_EOK != status) break;
    }

    tmp_bin.str_key_vector = NULL;
    tmp_bin.str_key_vector_size = 0;
    t = NULL;

    return status;
}


int32_t acdb_stack_push(LinkedList *stack, LinkedListNode *node)
{
    if (stack->length == 0)
    {
        stack->p_head = node;

        stack->p_tail = node;

        stack->length++;
    }
    else
    {
        //get what head is currently pointing to and point it to the inserted bin_node
        //stack->p_head->p_next = node;

        node->p_next = stack->p_head;
        //set the inserted bin_node to tail
        stack->p_head = node;

        stack->length++;
    }

    return 0;
}

int32_t acdb_stack_pop(LinkedList *stack, LinkedListNode **node)
{
    //(*node)->p_struct = stack->p_head->p_struct;
    *node = stack->p_head;

    if (!IsNull(stack->p_head->p_next))
    {
        stack->p_head = stack->p_head->p_next;
    }
    else
    {
        stack->p_head = NULL;
        stack->p_tail = NULL;
    }

    if (stack->length != 0)
    {
        stack->length--;
    }

    return 0;
}

int32_t acdb_heap_add_ckv_subgraph_map(AcdbDeltaKVToSubgraphDataMap* map)
{
    int32_t status = AR_EOK;
    size_t size_str_key_vector = 0;

    AcdbGraphKeyVector *map_key_vector = get_key_vector_from_map(map);
	if (IsNull(map_key_vector))
		return AR_EBADPARAM;
    char* str_key_vector = key_vector_to_string(
        map_key_vector, &size_str_key_vector);

    TreeNode *bin_node = NULL;
    KVSubgraphMapBin *bin = NULL;

    //Create Map Node on heap
    LinkedListNode *map_node = create_list_node(map);
    if (IsNull(map_node) || IsNull(str_key_vector)) return AR_ENOMEMORY;

    //Check to see if Tree has the appropriate bin
    //Locate bin and append to linked list
    if (AR_EOK == (status =
        acdb_heap_get_bin(size_str_key_vector,
            str_key_vector, &bin)))
    {
        return status;
    }
    else
    {
        //Create new bin and insert into tree
        bin = create_map_bin();
        if (IsNull(bin)) return AR_ENOMEMORY;

        bin_node = create_tree_node(bin);
        if (IsNull(bin_node)) return AR_ENOMEMORY;

        bin->str_key_vector = str_key_vector;
        bin->str_key_vector_size = (uint32_t)(size_str_key_vector);
        bin->map = map;

        acdb_heap_insert(bin_node);
        status = AR_EOK;
    }

    return status;
}

int32_t acdb_heap_get_ckv_subgraph_map(const AcdbGraphKeyVector *cal_key_vector, AcdbDeltaKVToSubgraphDataMap** map)
{
    int32_t status = AR_EOK;
    KVSubgraphMapBin *bin = NULL;
    AcdbGraphKeyVector *map_key_vector = NULL;
    AcdbDeltaKVToSubgraphDataMap *tmp_map = NULL;
    size_t size_str_key_vector = 0;
    char* str_key_vector = key_vector_to_string(
        cal_key_vector, &size_str_key_vector);

    //Search tree for key vector
    status = acdb_heap_get_bin(size_str_key_vector, str_key_vector, &bin);

    if (AR_EOK != status)
    {
        //ACDB_ERR("Key Vector not found in heap");
        //LogKeyVector(cal_key_vector);
        goto end;
    }

    tmp_map = (AcdbDeltaKVToSubgraphDataMap*)bin->map;

    if (IsNull(tmp_map->key_vector_data))
    {
        ACDB_DBG("Map key vector data is null for Key Vector<%s>", str_key_vector);
        status = AR_ENOTEXIST;
        goto end;
    }

    //Determine Key Vector
    switch (tmp_map->key_vector_type)
    {
    case TAG_KEY_VECTOR:
        map_key_vector = &((AcdbModuleTag*)tmp_map->key_vector_data)->tag_key_vector;
        break;
    case CAL_KEY_VECTOR:
        map_key_vector = ((AcdbGraphKeyVector*)tmp_map->key_vector_data);
        break;
    default:
        ACDB_DBG("Unable to determine Key Vector type for Key Vector<%s>", str_key_vector);
        status = AR_ENOTEXIST;
        goto end;
    }

    //Compare Sorted Key Vectors and values
    if (0 == ACDB_MEM_CMP(
        cal_key_vector->graph_key_vector,
        map_key_vector->graph_key_vector,
        map_key_vector->num_keys * sizeof(AcdbKeyValuePair)))
    {
        *map = tmp_map;
        tmp_map = NULL;
    }

    if (IsNull(*map))
    {
        ACDB_DBG("KV Map does not exist for Key Vector<%s>", str_key_vector);
        status = AR_ENOTEXIST;
    }

    end:
    if (!IsNull(str_key_vector))
    {
        ACDB_FREE(str_key_vector);
        str_key_vector = NULL;
    }

    return status;
}

/**
* \brief  acdb_heap_get_map_list
*           Returns an aggregated list of all the maps in the tree using an
*			inoder tree traversal(Left, Root, Right). Each node in map_list must be freed by caller.
* \param[out] map_list: A linked list of CKV Bin linked lists
*
* \return
* 0 -- Success
* Nonzero -- Failure
*/
int32_t acdb_heap_get_map_list(LinkedList **map_list)
{
    int32_t status = AR_EOK;
    TreeNode *cur_node = g_root;
    LinkedListNode *item_node = NULL;
    LinkedList stack = { 0 };

    if (IsNull(*map_list)) return AR_EBADPARAM;

    while (cur_node != NULL || stack.length != 0)
    {
        if (!IsNull(cur_node))
        {
            item_node = create_list_node(cur_node);
            if (IsNull(item_node))
            {
                return AR_ENOMEMORY;
            }
            acdb_stack_push(&stack, item_node);
            cur_node = cur_node->left;
            continue;
        }

        if (IsNull(cur_node) && stack.length > 0)
        {
            acdb_stack_pop(&stack, &item_node);
            cur_node = (TreeNode*)item_node->p_struct;
            ACDB_FREE(item_node);

            //Collect Key Vector Subgraph Maps and add/append to list
            LinkedListNode *node = create_list_node(((KVSubgraphMapBin*)cur_node->p_struct)->map);
			if (IsNull(node))
			{
				return AR_ENOMEMORY;
			}
            AcdbListAppend(*map_list, node);
            cur_node = cur_node->right;
        }
    }

    return status;
}

/**
* \brief  acdb_heap_clear
*           Clears all heap data in an inoder fashion
* \return
* 0 -- Success
* Nonzero -- Failure
*/
int32_t acdb_heap_clear()
{
    TreeNode *cur_node = g_root;
    TreeNode *tmp_node = NULL;
    LinkedListNode *item_node = NULL;
    LinkedList stack = { 0 };

    while (cur_node != NULL || stack.length != 0)
    {
        if (!IsNull(cur_node))
        {
            item_node = create_list_node(cur_node);
            if (IsNull(item_node))
            {
                return AR_ENOMEMORY;
            }
            acdb_stack_push(&stack, item_node);
            cur_node = cur_node->left;
            continue;
        }

        if (IsNull(cur_node) && stack.length > 0)
        {
            acdb_stack_pop(&stack, &item_node);
            cur_node = (TreeNode*)item_node->p_struct;

            KVSubgraphMapBin **bin = (KVSubgraphMapBin**)(&(cur_node->p_struct));
            free_bin(bin);
            //free_bin(&(CkvSubgraphMapBin*)cur_node->p_struct);

            //Free stack nodes
            ACDB_FREE(item_node);

            //Free Tree nodes
            tmp_node = cur_node->right;
            ACDB_FREE(cur_node);
            cur_node = tmp_node;
        }
    }

    return AR_EOK;
}

/**
* \brief  acdb_heap_get_heap_info
*           Performs an inorder traversal of the heap and collects information
            about each heap node in an. This includes:
*           1. Key Vector
*           2. Key Vector Type (CKV or TKV)
*           3. Calibration data size
* \return
* 0 -- Success
* Nonzero -- Failure
*/
int32_t acdb_heap_get_heap_info(AcdbBufferContext *rsp)
{
    int32_t status = AR_EOK;
    TreeNode *cur_node = g_root;
    LinkedListNode *item_node = NULL;
    LinkedList stack = { 0 };
    uint32_t offset = 0;
    uint32_t num_nodes = 0;

    //Make room to write number of heap nodes at the end of the function
    offset += sizeof(num_nodes);

    if (IsNull(rsp)) return AR_EBADPARAM;

    while (cur_node != NULL || stack.length != 0)
    {
        if (!IsNull(cur_node))
        {
            item_node = create_list_node(cur_node);
			if (IsNull(item_node))
			{
				return AR_ENOMEMORY;
			}
            acdb_stack_push(&stack, item_node);
            cur_node = cur_node->left;
            continue;
        }

        if (IsNull(cur_node) && stack.length > 0)
        {
            acdb_stack_pop(&stack, &item_node);
            cur_node = (TreeNode*)item_node->p_struct;
            ACDB_FREE(item_node);

            KVSubgraphMapBin* bin = (KVSubgraphMapBin*)cur_node->p_struct;

            num_nodes++;

            char* next_int = bin->str_key_vector;
            char tmp[HEX_DIGITS];
            uint32_t num_keys = 0;

            offset += sizeof(num_keys);//Make room to write number of keys
            /** Get Key Vector
             * This loop will write key followed by value for each key value pair.
             * The str_key_vector is in the form A0000000A0000001B0000000B0000001 where
             * key1.ID = A0000000 key1.Value = A0000001
             * key2.ID = B0000000 key2.Value = B0000001
             */
            while (*next_int != '\0')
            {
                ACDB_MEM_CPY_SAFE(&tmp[0], HEX_DIGITS, next_int, HEX_DIGITS);//8 bytes for 8 hex digits

                uint32_t key = 0;//(uint32_t)strtoul(&tmp[0], NULL, 16);

                ar_sscanf(&tmp[0], "%x", &key);
                next_int += HEX_DIGITS;

                //write to rsp buf
                ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(key), &key, sizeof(key));
                offset += sizeof(key);
                num_keys++;
            }

            next_int = NULL;
            num_keys = num_keys / 2;
            ACDB_MEM_CPY_SAFE(&rsp->buf[offset - (num_keys * sizeof(AcdbKeyValuePair) + sizeof(num_keys))],
                sizeof(num_keys), &num_keys, sizeof(num_keys));

            //Get Size of map
            AcdbDeltaKVToSubgraphDataMap *map =
                (AcdbDeltaKVToSubgraphDataMap*)bin->map;

            ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(map->map_size), &map->map_size, sizeof(map->map_size));
            offset += sizeof(map->map_size);

            //Determine type of Key Vector
            uint32_t kv_type = map->key_vector_type;
            ACDB_MEM_CPY_SAFE(&rsp->buf[offset], sizeof(kv_type), &kv_type, sizeof(kv_type));
            offset += sizeof(kv_type);

            cur_node = cur_node->right;
        }
    }

    ACDB_MEM_CPY_SAFE(rsp->buf, sizeof(num_nodes), &num_nodes, sizeof(num_nodes));

    rsp->bytes_filled = offset;
    return status;
}
/**
*==============================================================================
*	Public functions
*==============================================================================
*/
int32_t acdb_heap_ioctl(uint32_t cmd_id,
    uint8_t *pInput,
    uint8_t *pOutput,
    uint32_t nOutputSize)
{
    int32_t status = AR_EOK;

    switch (cmd_id)
    {
    case ACDB_HEAP_CMD_INIT:
    {
        break;
    }
    case ACDB_HEAP_CMD_CLEAR:
    {
        status = acdb_heap_clear();
        break;
    }
    case ACDB_HEAP_CMD_ADD_MAP:
    {
        status = acdb_heap_add_ckv_subgraph_map(
            (AcdbDeltaKVToSubgraphDataMap*)pInput);
        break;
    }
    case ACDB_HEAP_CMD_REMOVE_MAP:
    {
        //Similar to Get Map but frees the memory associated with the map
        break;
    }
    case ACDB_HEAP_CMD_GET_MAP:
    {
        status = acdb_heap_get_ckv_subgraph_map(
            (AcdbGraphKeyVector*)pInput,
            (AcdbDeltaKVToSubgraphDataMap**)pOutput);

        break;
    }
    case ACDB_HEAP_CMD_GET_MAP_LIST:
    {
        status = acdb_heap_get_map_list((LinkedList**)pOutput);
        break;
    }

    case ACDB_HEAP_CMD_GET_HEAP_INFO:
    {
        if (nOutputSize < sizeof(AcdbBufferContext))
        {
            return AR_EBADPARAM;
        }

        AcdbBufferContext *rsp = (AcdbBufferContext*)pOutput;
        status = acdb_heap_get_heap_info(rsp);
        break;
    }
    default:
        break;
    }
    return status;
}
