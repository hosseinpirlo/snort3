/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 ** Copyright (C) 2011-2013 Sourcefire, Inc.
 **
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License Version 2 as
 ** published by the Free Software Foundation.  You may not use, modify or
 ** distribute this program under any other version of the GNU General
 ** Public License.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **
 ** 9/7/2011 - Initial implementation ... Hui Cao <hcao@sourcefire.com>
 */

#include "sfrt_flat.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "snort_types.h"
#include "snort_debug.h"
#include "sfip/ipv6_port.h"

/* Create new lookup table
 * @param   table_flat_type Type of table. Uses the types enumeration in route.h
 * @param   ip_type    IPv4 or IPv6. Uses the types enumeration in route.h
 * @param   data_size  Max number of unique data entries
 *
 * Returns the new table. */
table_flat_t *sfrt_flat_new(char table_flat_type, char ip_type,  long data_size, uint32_t mem_cap)
{
    table_flat_t *table;
    MEM_OFFSET table_ptr;
    uint8_t *base;

    table_ptr = segment_malloc(sizeof(table_flat_t));

#if 0
    /*The first allocation always return 0*/
    if(!table_ptr)
    {
        //  return NULL;
    }
#endif

    base = (uint8_t *)segment_basePtr();
    table = (table_flat_t *)(&base[table_ptr]);


    /* If this limit is exceeded, there will be no way to distinguish
     * between pointers and indeces into the data table.  Only
     * applies to DIR-n-m. */

#if SIZEOF_LONG_INT == 8
    if(data_size >= 0x800000000000000)
#else
        if(data_size >= 0x8000000)
#endif
        {
            segment_free(table_ptr);
            return NULL;
        }

    /* mem_cap is specified in megabytes, but internally uses bytes. Convert */
    mem_cap *= 1024*1024;

    /* Maximum allowable number of stored entries */
    table->max_size = data_size;

    table->data = (INFO)segment_calloc(sizeof(INFO) * table->max_size, 1);

    if(!table->data)
    {
        segment_free(table_ptr);
        return NULL;
    }

    table->allocated = sizeof(table_flat_t) + sizeof(INFO) * table->max_size;

    table->ip_type = ip_type;
    table->table_flat_type = table_flat_type;

    /* This will point to the actual table lookup algorithm */
    table->rt = 0;
    table->rt6 = 0;

    /* index 0 will be used for failed lookups, so set this to 1 */
    table->num_ent = 1;

    /* Allocate the user-specified DIR-n-m table */
    switch(table_flat_type)
    {
    case DIR_24_8:
        table->rt = sfrt_dir_flat_new( mem_cap, 2, 24, 8);
        break;
    case DIR_16x2:
        table->rt = sfrt_dir_flat_new( mem_cap, 2, 16,16);
        break;
    case DIR_16_8x2:
        table->rt = sfrt_dir_flat_new( mem_cap, 3, 16,8,8);
        break;
    case DIR_16_4x4:
        table->rt = sfrt_dir_flat_new( mem_cap, 5, 16,4,4,4,4);
        break;
    case DIR_8x4:
        table->rt = sfrt_dir_flat_new( mem_cap, 4, 8,8,8,8);
        break;
        /* There is no reason to use 4x8 except for benchmarking and
         * comparison purposes. */
    case DIR_4x8:
        table->rt = sfrt_dir_flat_new( mem_cap, 8, 4,4,4,4,4,4,4,4);
        break;
        /* There is no reason to use 2x16 except for benchmarking and
         * comparison purposes. */
    case DIR_2x16:
        table->rt = sfrt_dir_flat_new( mem_cap, 16,
                2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2);
        break;
    case DIR_16_4x4_16x5_4x4:
        table->rt = sfrt_dir_flat_new(mem_cap, 5, 16,4,4,4,4);
        table->rt6 = sfrt_dir_flat_new(mem_cap, 14, 16,4,4,4,4,16,16,16,16,16,4,4,4,4);
        break;
    case DIR_16x7_4x4:
        table->rt = sfrt_dir_flat_new(mem_cap, 5, 16,4,4,4,4);
        table->rt6 = sfrt_dir_flat_new(mem_cap, 11, 16,16,16,16,16,16,16,4,4,4,4);
        break;
    case DIR_16x8:
        table->rt = sfrt_dir_flat_new(mem_cap, 2, 16,16);
        table->rt6 = sfrt_dir_flat_new(mem_cap, 8, 16,16,16,16,16,16,16,16);
        break;
    case DIR_8x16:
        table->rt = sfrt_dir_flat_new( mem_cap, 4, 16,8,4,4);
        table->rt6 = sfrt_dir_flat_new(mem_cap, 16,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8);
        break;
    };

    if((!table->rt) || (!table->rt6))
    {
        if (table->rt)
            sfrt_dir_flat_free( table->rt );
        if (table->rt6)
            sfrt_dir_flat_free( table->rt6 );
        segment_free(table->data);
        segment_free(table_ptr);
        return NULL;
    }

    return table;
}

/* Free lookup table */
void sfrt_flat_free(TABLE_PTR table_ptr)
{

    table_flat_t *table;
    uint8_t *base;

    if(!table_ptr)
    {
        /* What are you calling me for? */
        return;
    }

    base = (uint8_t *)segment_basePtr();
    table = (table_flat_t *)(&base[table_ptr]);

    if(!table->data)
    {
        /* This really really should not have happened */
    }
    else
    {
        segment_free(table->data);
    }

    if(!table->rt)
    {
        /* This should not have happened either */
    }
    else
    {
        sfrt_dir_flat_free( table->rt );
    }

    if(!table->rt6)
    {
        /* This should not have happened either */
    }
    else
    {
        sfrt_dir_flat_free( table->rt6 );
    }

    segment_free(table_ptr);
}

/* Perform a lookup on value contained in "ip" */
GENERIC sfrt_flat_lookup(void *adr, table_flat_t *table)
{
    tuple_flat_t tuple;
    INFO *data;
    sfip_t *ip;
    TABLE_PTR rt = 0;
    uint8_t *base;

    if(!adr)
    {
        return NULL;
    }

    if(!table)
    {
        return NULL;
    }

    ip = (sfip_t*)adr;
    if (ip->family == AF_INET)
    {
        rt = table->rt;
    }
    else if (ip->family == AF_INET6)
    {
        rt = table->rt6;
    }

    if (!rt)
    {
        return NULL;
    }

    tuple = sfrt_dir_flat_lookup(ip, rt);

    if(tuple.index >= table->num_ent)
    {
        return NULL;
    }
    base = (uint8_t *)segment_basePtr();
    data = (INFO *)(&base[table->data]);
    if (data[tuple.index])
        return (GENERIC) &base[data[tuple.index]];
    else
        return NULL;

}



/* Insert "ip", of length "len", into "table", and have it point to "ptr" */
/* Insert "ip", of length "len", into "table", and have it point to "ptr" */
int sfrt_flat_insert(void *adr, unsigned char len, INFO ptr,
        int behavior, table_flat_t* table, updateEntryInfoFunc updateEntry)
{
    int index;
    int res =  RT_SUCCESS;
    INFO *data;
    sfip_t *ip;
    tuple_flat_t tuple;
    TABLE_PTR rt = 0;
    uint8_t *base;
    int64_t bytesAllocated;

    if(!adr )
    {
        return RT_INSERT_FAILURE;
    }

    if (len == 0)
        return RT_INSERT_FAILURE;


    if(!table || !table->data)
    {
        return RT_INSERT_FAILURE;
    }

    if( (table->ip_type == IPv4 && len > 32) ||
            (table->ip_type == IPv6 && len > 128) )
    {
        return RT_INSERT_FAILURE;
    }

    ip = (sfip_t*)adr;

    if (ip->family == AF_INET)
    {
        rt = table->rt;
    }
    else if (ip->family == AF_INET6)
    {
        rt = table->rt6;
    }
    if (!rt)
    {
        return RT_INSERT_FAILURE;
    }

    tuple = sfrt_dir_flat_lookup(ip, table->rt);

    base = (uint8_t *)segment_basePtr();
    data = (INFO *)(&base[table->data]);

    if(tuple.length != len)
    {
        if( table->num_ent >= table->max_size)
        {
            return RT_POLICY_TABLE_EXCEEDED;
        }

        index = table->num_ent;
        table->num_ent++;
        /* Insert value into policy table */
        data[index] = 0;
    }
    else
    {
        index = tuple.index;
    }

    bytesAllocated = updateEntry(&data[index], ptr, SAVE_TO_CURRENT, base);

    if (bytesAllocated < 0)
    {
        if(tuple.length != len)
            table->num_ent--;
        return MEM_ALLOC_FAILURE;
    }

    table->allocated += (uint32_t)bytesAllocated;

    /* The actual value that is looked-up is an index
     * into the data table. */
    res = sfrt_dir_flat_insert(ip, len, index, behavior, rt, updateEntry, data);

    /* Check if we ran out of memory. If so, need to decrement
     * table->num_ent */
    if(res == MEM_ALLOC_FAILURE)
    {
        /* From the control flow above, it's possible table->num_ent was not
         * incremented.  It should be safe to decrement here, because the only
         * time it will be incremented above is when we are potentially
         * mallocing one or more new entries (It's not incremented when we
         * overwrite an existing entry). */
        table->num_ent--;
    }

    return res;
}

uint32_t sfrt_flat_num_entries(table_flat_t* table)
{
    if(!table)
    {
        return 0;
    }

    if( !table->rt || !table->allocated)
    {
        return 0;
    }

    /* There is always a root node, so subtract 1 for it */
    return table->num_ent - 1;
}

uint32_t sfrt_flat_usage(table_flat_t *table)
{
    uint32_t usage;
    if(!table || !table->rt || !table->allocated )
    {
        return 0;
    }

    usage = table->allocated + sfrt_dir_flat_usage( table->rt );

    if (table->rt6)
    {
        usage += sfrt_dir_flat_usage( table->rt6 );
    }

    return usage;
}

/* Perform a lookup on value contained in "ip"
 * For performance reason, we use this simplified version instead of sfrt_lookup
 * Note: this only applied to table setting: DIR_8x16 (DIR_16_8_4x2 for IPV4), DIR_8x4*/
GENERIC sfrt_flat_dir8x_lookup(void *adr, table_flat_t* table)
{
    dir_sub_table_flat_t *subtable;
    DIR_Entry *entry;
    uint8_t *base = (uint8_t *) table;
    int i;
    sfip_t *ip;
    dir_table_flat_t *rt = NULL;
    int index;
    INFO *data = (INFO *) (&base[table->data]);

    ip = (sfip_t*)adr;
    if (ip->family == AF_INET)
    {
        rt = (dir_table_flat_t *)(&base[table->rt]);
        subtable = (dir_sub_table_flat_t *)(&base[rt->sub_table]);
        /* 16 bits*/
        index = ntohs(ip->ip16[0]);
        entry = (DIR_Entry *)(&base[subtable->entries]);
        if( !entry[index].value || entry[index].length)
        {
            if (data[entry[index].value])
                return (GENERIC) &base[data[entry[index].value]];
            else
                return NULL;
        }
        subtable = (dir_sub_table_flat_t *)(&base[entry[index].value]);

        /* 8 bits*/
        index = ip->ip8[2];
        entry = (DIR_Entry *)(&base[subtable->entries]);
        if( !entry[index].value || entry[index].length)
        {
            if (data[entry[index].value])
                return (GENERIC) &base[data[entry[index].value]];
            else
                return NULL;
        }
        subtable = (dir_sub_table_flat_t *)(&base[entry[index].value]);

        /* 4 bits */
        index = ip->ip8[3] >> 4;
        entry = (DIR_Entry *)(&base[subtable->entries]);
        if( !entry[index].value || entry[index].length)
        {
            if (data[entry[index].value])
                return (GENERIC) &base[data[entry[index].value]];
            else
                return NULL;
        }
        subtable = (dir_sub_table_flat_t *)(&base[entry[index].value]);

        /* 4 bits */
        index = ip->ip8[3] & 0xF;
        entry = (DIR_Entry *)(&base[subtable->entries]);
        if( !entry[index].value || entry[index].length)
        {
            if (data[entry[index].value])
                return (GENERIC) &base[data[entry[index].value]];
            else
                return NULL;
        }
    }
    else if (ip->family == AF_INET6)
    {

        rt = (dir_table_flat_t *)(&base[table->rt6]);
        subtable = (dir_sub_table_flat_t *)(&base[rt->sub_table]);
        for (i = 0; i < 16; i++)
        {
            index = ip->ip8[i];
            entry = (DIR_Entry *)(&base[subtable->entries]);
            if( !entry[index].value || entry[index].length)
            {
                if (data[entry[index].value])
                    return (GENERIC) &base[data[entry[index].value]];
                else
                    return NULL;
            }
            subtable = (dir_sub_table_flat_t *)(&base[entry[index].value]);
        }
    }
return NULL;

}

