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
	uint16_t fat_entry;
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
	root_entry root_array[128];

}RD;

typedef struct FD
{
	int fd;
	int offset;
	root_entry* file;
}FD;

int FD_Open;
int rootFree;
int fatFree;
FAT* fat;
FD* FD_Array[32];
SB* sb;
RD* rd;

int fs_mount(const char *diskname)
{
	rootFree = 0;
	fatFree = 0;

	if(block_disk_open(diskname) == -1)
	{
		return -1;
	}

	sb = (SB*)malloc(sizeof(SB));

	rd = (RD*)malloc(sizeof(RD));

	block_read(0, (void*)sb);

	fat = (FAT*)malloc(BLOCK_SIZE * sb->FAT_blocks);
	
/*	for(int i = 0; i < sb->FAT_blocks; i++)
	{
		fat[i] = (FAT*)malloc(sizeof(FAT));
		block_read(1 + i,(void*)(fat[i]->fat_array));
	}
*/

	for(int i = 0; i < sb->FAT_blocks; i++)
	{
		block_read(1 + i,(void*)fat + i*(BLOCK_SIZE));
	}



	for(int i = 0; i < sb->data_blocks; i++)
	{
		if(fat[i].fat_entry == 0)
		{
			fatFree++;
		}
	}

	block_read(sb->root_index, (void*)rd->root_array);

	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i].filename[0] == 0)
		{
			rootFree++;
		}
	}

	//Initialize FD_Array
	FD_Open = 0;
	for(int i = 0 ; i < 32; i++){
		FD_Array[i] = (FD*)malloc(sizeof(FD));
		FD_Array[i]->fd = -1;
		FD_Array[i]->offset = -1;
		FD_Array[i]->file = NULL;
	}

	return 0;
}

int fs_umount(void)
{
/*	if(sb)
	{
		free(sb);
	}

	if(fat)
	{
		free(fat);
	}
	
	if(fat)
	{
		free(rd);
	}
*/	

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
	printf("fat_free_ratio=%hu/%hu\n", fatFree, sb->data_blocks);
	printf("rdir_free_ratio=%hu/128\n", rootFree);			//loop through disk to find how many root directories are free
	return 0;
}

int fs_create(const char *filename)
{
	
	if(strlen(filename) > 16)
	{
		return -1;
	}

	int free_index = 0;

	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i].filename[0] != 0)
		{
			if(strcmp(rd->root_array[i].filename, filename) == 0)
			{
				return -1;
			}

		}
	}

	while(rd->root_array[free_index].filename[0] != 0)
	{
		free_index++;

		if(free_index == 128)
		{
			return -1;
		}
	}


	rd->root_array[free_index].size = 0;
	rd->root_array[free_index].index = FAT_EOC;
	strcpy(rd->root_array[free_index].filename, filename);
	printf("new filename: %s\n", rd->root_array[free_index].filename);
	rootFree--;
	block_write(sb->root_index,rd);

	return 0;
}

int fs_delete(const char *filename)
{

	if(strlen(filename) > 16)
	{
		return -1;
	}

	int i = 0;

	for(; i < 128; i++)
	{
		if(rd->root_array[i].filename[0] != 0)
		{
			if(strcmp(rd->root_array[i].filename, filename) == 0)
			{
				memset(rd->root_array[i].filename, 0, 16);
				block_write(sb->root_index,rd);
				return 0;
			}
		}
	}
	
	
	return -1;
}

int fs_ls(void)
{
	printf("FS Ls:\n");
	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i].filename[0] != 0)
		{
			printf("file: %s, size: %d, data_blk: %d\n", rd->root_array[i].filename, rd->root_array[i].size, rd->root_array[i].index);
		}
	}
	
	return 0;
}

int fs_open(const char *filename)
{
	if(strlen(filename) > 16){
		return -1;
	}

	int free_index = 0;

	for(; free_index < 32; free_index++)
	{
		if(FD_Array[free_index]->fd == -1)
			break;
	}

	if(free_index == 32)
	{
		return -1;
	}


	for(int i = 0; i < 128; i++)
	{
		if(rd->root_array[i].filename[0] != 0)
		{
			if(strcmp(rd->root_array[i].filename, filename) == 0)
			{
				FD_Array[free_index]->fd = free_index;
				FD_Array[free_index]->offset = 0;
				FD_Array[free_index]->file = &(rd->root_array[i]);
				FD_Open++;
				return free_index;
			}
		}
	}

	return -1;
}

int fs_close(int fd)
{
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	if(FD_Array[fd]->fd == -1)
	{
		return -1;
	}

	FD_Array[fd]->fd = -1;
	FD_Array[fd]->offset = -1;
	FD_Array[fd]->file = NULL;
	return 0;
}

int fs_stat(int fd)
{
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	if(FD_Array[fd]->fd == -1)
	{
		return -1;
	}

	return FD_Array[fd]->file->size;
}

int fs_lseek(int fd, size_t offset)
{
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	if(FD_Array[fd]->fd == -1)
	{
		return -1;
	}

	FD_Array[fd]->offset = offset;		

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	int next, start;
	char* buffer = NULL;
	char* build = NULL;
	build = (char*)malloc(BLOCK_SIZE); 
 
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	if(FD_Array[fd]->offset > BLOCK_SIZE * sb->FAT_blocks) // || FD_Array[fd]->offset > FD_Array[fd]->file->size)
	{
		return 0;
	}


	start = FD_Array[fd]->file->index + (FD_Array[fd]->offset / 4096);		// start is an fd index

	block_read(sb->start_index + start, (void*)build);

	
	while(fat[next].fat_entry != FAT_EOC)
	{
		next = fat[next].fat_entry;

		block_read(sb->start_index + next, (void*)buffer);

		strcat(build, buffer);
	}

	build = build + FD_Array[fd]->offset;

	memcpy(buf, (void*)build, count);

	return count;
}
