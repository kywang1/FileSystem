#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

typedef struct __attribute__((__packed__)) root_entry{
	char filename[16];
	uint32_t size;
	uint16_t index;
}root_entry;


typedef struct __attribute__((__packed__)) FAT{			// max fat block count is 4 since max block count is 8192
	uint16_t fat_array[4096];
	int used;
}FAT;

typedef struct __attribute__((__packed__)) SB{
	uint64_t signature;
	uint16_t block_total;
	uint16_t root_index;
	uint16_t start_index;
	uint16_t data_blocks;
	uint8_t FAT_blocks;
}SB;

typedef struct __attribute__((__packed__)) RD{
	char test[2816];
	root_entry *root_array[128];
	int used;
}RD;

FAT* fat[4];
SB* sb;
RD* rd;

int fs_mount(const char *diskname)
{

	if(block_disk_open(diskname) == -1)
	{
		return -1;
	}

	sb = (SB*)malloc(sizeof(SB));
	rd = (RD*)malloc(sizeof(RD));

	block_read(0, (void*)sb);
	
	for(int i = 0; i < sb->FAT_blocks; i++)
	{
		fat[i] = (FAT*)malloc(sizeof(FAT));
		block_read(1 + i,(void*)(fat[i]->fat_array));
	}

	block_read(sb->root_index, (void*)(rd->test));

	for(int i = 0; i < 128; i++)
	{
		rd->root_array[i] = (root_entry*)malloc(sizeof(root_entry));
		memcpy(rd->root_array[i], rd->test + (i * 22), 22);
	}

	fat[0]->used = 1;

	return 0;
}

int fs_umount(void)
{
/*	if(sb)
	{
		free(sb);
	}
*/
	for(int i = 0; i < 4; i++)
	{
		if(fat[i])
		{
			free(fat[i]);
		}
		i++;
	}

	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i])
		{
			free(rd->root_array[i]);
		}

		i++;
	}

//	free(rd);	

	return(block_disk_close());
}

int fs_info(void)
{
	printf("FS Info:\n");
	printf("total_blk_count=%hu\n",sb->block_total);
	printf("fat_blk_count=%hu\n", sb->FAT_blocks);
	printf("rdir_blk=%hu\n", sb->root_index); 						// come back to this
	printf("data_blk=%hu\n", sb->start_index); 						// come back to this
	printf("data_blk_count=%hu\n", sb->data_blocks);
	printf("fat_free_ratio=%hu/%hu\n", (sb->data_blocks - fat[0]->used), sb->data_blocks);
	printf("rdir_free_ratio=%hu/128\n", 128 - rd->used);
	return 0;
}

int fs_create(const char *filename)
{

	if(strlen(filename) > 16)
	{
		return -1;
	}

	int free_index = 0;
	int i = 0, used_count = 0;

	for(; i < 128; i++)
	{
		if(rd->root_array[i]->filename[0] != 0)
		{
			used_count++;
			if(strcmp(rd->root_array[i]->filename, filename) == 0)
			{
				return -1;
			}

			if(used_count == rd->used)
			{
				break;
			}
		}
	}

	while(rd->root_array[free_index]->filename[0] != 0)
	{
		free_index++;

		if(free_index == 128)
		{
			return -1;
		}
	}


	rd->root_array[free_index]->size = 0;
	rd->root_array[free_index]->index = FAT_EOC;
	strcpy(rd->root_array[free_index]->filename, filename);
	rd->used++;


	return 0;
}

int fs_delete(const char *filename)
{
	if(strlen(filename) > 16)
	{
		return -1;
	}

	int i = 0, used_count = 0;

	for(; i < 128; i++)
	{
		if(rd->root_array[i]->filename[0] != 0)
		{
			if(strcmp(rd->root_array[i]->filename, filename) == 0)
			{
				memset(rd->root_array[i]->filename, 0, 16);
				rd->used--;
				return 0;
			}

			if(used_count == rd->used)
			{
				break;
			}
		}
	}

	return 0;
}

int fs_ls(void)
{
	int used = 0;
	printf("FS Ls:\n");
	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i]->filename[0] != 0)
		{
			printf("file: %s, size: %d, data_blk: %d\n", rd->root_array[i]->filename, rd->root_array[i]->size, rd->root_array[i]->index);
			used++;
		}
		if(used == rd->used)
		{
			break;
		}
	}
	return 0;
}

int fs_open(const char *filename)
{
	return 0;
}

int fs_close(int fd)
{
	return 0;
}

int fs_stat(int fd)
{
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	return 0;
}
