#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

typedef struct root_entry{
	char filename[16];
	uint32_t size;
	uint16_t index;
}root_entry;


typedef struct __attribute__((__packed__)) FAT{
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
	root_entry *root_array[128];
	int used;
}RD;



SB* sb;
FAT* fat;
RD* rd;

int fs_mount(const char *diskname)
{

	if(block_disk_open(diskname) == -1)
	{
		return -1;
	}

	sb = (SB*)malloc(sizeof(SB));
	fat = (FAT*)malloc(sizeof(FAT));
	rd = (RD*)malloc(sizeof(RD));

	fat->used = 1;


	block_read(0, (void*)sb);


	return 0;
}

int fs_umount(void)
{
	return(block_disk_close());
}

int fs_info(void)
{
	puts("FS Info:");
	printf("total_blk_count=%hu\n",sb->block_total);
	printf("fat_blk_count=%hu\n", sb->FAT_blocks);
	puts("rdir_blk=1\n"); 						// come back to this
	puts("data_blk=1\n"); 						// come back to this
	printf("data_blk_count=%hu\n", sb->data_blocks);
	printf("fat_free_ratio=%hu/%hu\n", sb->data_blocks - fat->used, sb->data_blocks);
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
		if(rd->root_array[i] != NULL)
		{
			used_count++;
			if(strcmp(rd->root_array[i]->filename, filename) == 0)
				return -1;

			if(used_count == rd->used)
			{
				break;
			}
		}
	}

	while(rd->root_array[free_index] != NULL)
	{
		free_index++;

		if(free_index == 128)
		{
			return -1;
		}
	}


	root_entry *new_entry = (root_entry*)malloc(sizeof(root_entry));
	strcpy(new_entry->filename, filename);
	new_entry->size = 0;
	new_entry->index = 0xFFFF;
	rd->root_array[free_index] = new_entry;

	return 0;
}

int fs_delete(const char *filename)
{
	return 0;
}

int fs_ls(void)
{
	printf("ls");
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
