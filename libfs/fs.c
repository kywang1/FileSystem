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
	char padding[10];
}root_entry;


typedef struct __attribute__((__packed__)) FAT{			// max fat block count is 4 since max block count is 8192
	uint16_t fat_entry;

}FAT;

typedef struct __attribute__((__packed__)) SB{
	char signature[8];
	uint16_t block_total;
	uint16_t root_index;
	uint16_t start_index;
	uint16_t data_blocks;
	uint8_t FAT_blocks;
	char padding[4079];
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
SB sb;
RD rd;

int error_check(void)
{
	sb.signature[8] = '\0';
	if(strncmp(sb.signature, "ECS150FS", strlen(sb.signature)) != 0 || sb.block_total + sb.FAT_blocks + 2 != block_disk_count() || sb.root_index - sb.FAT_blocks != 1 || sb.start_index - sb.root_index != 1)
	{
		puts("error");
		return -1;
	}

	return 0;
}

int fs_mount(const char *diskname)
{
	rootFree = 0;
	fatFree = 0;

	if(block_disk_open(diskname) == -1)
	{
		return -1;
	}

	block_read(0, &sb);

	if(error_check() == -1)
	{
		return -1;
	}

	fat = (FAT*)malloc(BLOCK_SIZE * sb.FAT_blocks);
	

	for(int i = 0; i < sb.FAT_blocks; i++)
	{
		block_read(1 + i,(void*)fat + i*(BLOCK_SIZE));
	}

	printf("fat[1].fat_entry: %d\n",fat[1].fat_entry);
	printf("fat[2].fat_entry: %d\n",fat[2].fat_entry);
	printf("fat[3].fat_entry: %d\n",fat[3].fat_entry);
	printf("fat[4].fat_entry: %d\n",fat[4].fat_entry);
	printf("fat[5].fat_entry: %d\n",fat[5].fat_entry);
	printf("fat[6].fat_entry: %d\n",fat[6].fat_entry);

	for(int i = 0; i < sb.data_blocks; i++)
	{
		if(fat[i].fat_entry == 0)
		{
			fatFree++;
		}
	}

	block_read(sb.root_index, &rd.root_array);

	for(int i = 0; i < 128; i++)
	{
		if(rd.root_array[i].filename[0] == 0)
		{
			rootFree++;
		}
	}

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
	if(fat)
	{
		free(fat);
	}

	return(block_disk_close());
}

int fs_info(void)
{
	if(error_check() == -1)
	{
		return -1;
	}
	printf("FS Info:\n");
	printf("total_blk_count=%hu\n",sb.block_total + sb.FAT_blocks + 2);
	printf("fat_blk_count=%hu\n", sb.FAT_blocks);
	printf("rdir_blk=%hu\n", sb.root_index); 						// come back to this
	printf("data_blk=%hu\n", sb.start_index); 						// come back to this
	printf("data_blk_count=%hu\n", sb.data_blocks);
	printf("fat_free_ratio=%hu/%hu\n", fatFree, sb.data_blocks);
	printf("rdir_free_ratio=%hu/128\n", rootFree);			//loop through disk to find how many root directories are free
	return 0;
}


int find_free_fat(int curr_index)
{
	for(int i = 0; i < BLOCK_SIZE * sb.FAT_blocks; i++)
	{
		if(fat[i].fat_entry == 0 && i != curr_index)
		{
			return i;
		}
	}

	return -1;
}

void fat_delete(int index)
{
	int curr_index = index, next = 0;
	char empty_block[4096];
	memset(empty_block, 0, 4096);

	while(curr_index != FAT_EOC)
	{
		next = fat[curr_index].fat_entry;
		fat[curr_index].fat_entry = 0;
		block_write(sb.start_index + curr_index, empty_block);
		curr_index = next;
	}	
}

void fat_write()
{
	for(int i = 0; i < sb.FAT_blocks; i++)
	{
		block_write(1 + i,(void*)fat + i*(BLOCK_SIZE));
	}
}

int fs_create(const char *filename)
{
	if(error_check() == -1)
	{
		puts("stderr");
		return -1;
	}
	
	if(strlen(filename) > 16)
	{
		puts("filename");
		return -1;
	}

	int rd_free_index = 0;
/*	, fat_free_index = 0;
	fat_free_index = find_free_fat();

	if(fat_free_index == -1)
	{
		puts("fat");
		return -1;
	}
*/

	for(int i = 0; i < 128; i++)
	{
		if(rd.root_array[i].filename[0] != 0)
		{
			if(strcmp(rd.root_array[i].filename, filename) == 0)
			{
				puts("dup");
				return -1;
			}
		}
	}

	while(rd.root_array[rd_free_index].filename[0] != 0)
	{
		rd_free_index++;

		if(rd_free_index == 128)
		{
			puts("max");
			return -1;
		}
	}

	rd.root_array[rd_free_index].size = 0;
	rd.root_array[rd_free_index].index = FAT_EOC;
	strcpy(rd.root_array[rd_free_index].filename, filename);

//	fat[fat_free_index].fat_entry = FAT_EOC;
	rootFree--;
//	fatFree--;
	
	block_write(sb.root_index,&rd);
	fat_write();
	

	return 0;
}

int fs_delete(const char *filename)
{
	if(error_check() == -1)
	{
		return -1;
	}

	if(strlen(filename) > 16)
	{
		return -1;
	}

	for(int i = 0; i < 128; i++)
	{
		if(strcmp(rd.root_array[i].filename, filename) == 0)
		{
			fat_delete(rd.root_array[i].index);
			memset(rd.root_array[i].filename, 0, 16);
			block_write(sb.root_index, rd.root_array);
			return 0;
		}
	}
	
	
	return -1;
}

int fs_ls(void)
{
	if(error_check() == -1)
	{
		return -1;
	}
	printf("FS Ls:\n");
	for(int i = 0; i < 128; i++)
	{
		if(rd.root_array[i].filename[0] != 0)
		{
			printf("file: %s, size: %d, data_blk: %d\n", rd.root_array[i].filename, rd.root_array[i].size, rd.root_array[i].index);
		}
	}
	
	return 0;
}

int fs_open(const char *filename)
{
	if(error_check() == -1)
	{
		return -1;
	}
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
		if(rd.root_array[i].filename[0] != 0)
		{

			if(strcmp(rd.root_array[i].filename, filename) == 0)
			{
				FD_Array[free_index]->fd = free_index;
				FD_Array[free_index]->offset = 0;
				FD_Array[free_index]->file = &(rd.root_array[i]);
				FD_Open++;
				return free_index;
			}
		}
	}
	
	return -1;
}

int fs_close(int fd)
{
	if(error_check() == -1)
	{
		return -1;
	}
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
	if(error_check() == -1)
	{
		return -1;
	}
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
	if(error_check() == -1)
	{
		return -1;
	}
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
	int curr_block, blocks_left = (count / BLOCK_SIZE), wrote = 0, size_flag = 0, next = 0;


	if(error_check() == -1 || fd < 0 || fd >= 32 )
	{
		return -1;
	}

	int offset = FD_Array[fd]->offset;

	if(count % BLOCK_SIZE != 0)
	{
		blocks_left++;
	}	

	if(FD_Array[fd]->file->index == FAT_EOC)				// empty file
	{
		curr_block = find_free_fat(-1);
		FD_Array[fd]->file->index = curr_block;
		printf("curr: %d\n", curr_block);
	}
	else
	{
		curr_block = FD_Array[fd]->file->index + (FD_Array[fd]->offset / BLOCK_SIZE);
	}

	if(offset > BLOCK_SIZE * sb.FAT_blocks || offset > FD_Array[fd]->file->size || curr_block >= sb.data_blocks)
	{
		return 0;
	}
	printf("blocks %d\n", blocks_left);

	char* build = malloc(BLOCK_SIZE * sb.FAT_blocks);
	char* block = malloc(BLOCK_SIZE);

	if(FD_Array[fd]->file->size != 0)
	{
		block_read(sb.start_index + curr_block, build);
		build += offset;
		strcat(build, buf);
		size_flag = 1;
	}
	else
	{
		memcpy(build,buf,BLOCK_SIZE * sb.FAT_blocks);
	}

	memcpy(block, build, BLOCK_SIZE);
	build += BLOCK_SIZE;

	block_write(sb.start_index + curr_block, block);
	memset(block, 0, BLOCK_SIZE);
	wrote += BLOCK_SIZE - (offset % BLOCK_SIZE);

	blocks_left--;
	if(blocks_left == 0)
	{
		fat[curr_block].fat_entry = FAT_EOC;
		if(size_flag)
		{
			FD_Array[fd]->file->size = FD_Array[fd]->file->size - offset + count;
		}
		else
		{
			FD_Array[fd]->file->size = count;
		}
		fat_write();
		block_write(sb.root_index, rd.root_array);
//		free(block);
//		free(build);
		return count;
	}

	while(blocks_left > 1)
	{
		next = fat[curr_block].fat_entry;

		if(next == FAT_EOC || next == 0)
		{
			next = find_free_fat(curr_block);
		
			if(next == -1)
			{
				fat[curr_block].fat_entry = FAT_EOC;
				if(size_flag)
				{
					FD_Array[fd]->file->size = FD_Array[fd]->file->size - offset + wrote;
				}
				else
				{
					FD_Array[fd]->file->size = wrote;
				}
				fat_write();
				block_write(sb.root_index, rd.root_array);
//				free(block);
//				free(build);
				return count;
			}
		}
		fat[curr_block].fat_entry = next;
		curr_block = next;
		memcpy(block, build, BLOCK_SIZE);
		build += BLOCK_SIZE;

		block_write(sb.start_index + curr_block, block);
		memset(block, 0, BLOCK_SIZE);

		wrote += BLOCK_SIZE;
		blocks_left--;

	}

	if(wrote < count)
	{
		next = find_free_fat(curr_block);
		fat[curr_block].fat_entry = next;

		if(next == -1)
		{
			fat[curr_block].fat_entry = FAT_EOC;
			if(size_flag)
			{
				FD_Array[fd]->file->size = FD_Array[fd]->file->size - offset + wrote;
			}
			else
			{
				FD_Array[fd]->file->size = wrote;
			}
			fat_write();
			block_write(sb.root_index, rd.root_array);
//			free(build);
//			free(block);
			return count;
		}
		curr_block = next;
		char* last_block = malloc(BLOCK_SIZE);
		block_read(sb.start_index + curr_block, last_block);
		last_block += count - wrote;
		strcat(build, last_block);

		block_write(sb.start_index + curr_block, build);
		wrote += count - wrote;
		fat[curr_block].fat_entry = FAT_EOC;

		if(size_flag)
		{
			FD_Array[fd]->file->size = FD_Array[fd]->file->size - offset + count;
		}
		else
		{
			FD_Array[fd]->file->size = count;
		}

		block_write(sb.root_index, rd.root_array);
		fat_write();
//		free(last_block);
//		free(block);
//		free(build);
		return wrote;
	}

	return 0;
}


int fs_read(int fd, void *buf, size_t count)
{
	if(error_check() == -1)
	{
		return -1;
	}
 
	if(fd < 0 || fd >= 32)
	{
		return -1;
	}

	if(FD_Array[fd]->offset > BLOCK_SIZE * sb.FAT_blocks || FD_Array[fd]->offset > FD_Array[fd]->file->size)
	{
		return 0;
	}

	int curr_block, read = 0;
	char* buffer = malloc(BLOCK_SIZE) ;
	char* build = malloc(BLOCK_SIZE * sb.FAT_blocks);

	curr_block = FD_Array[fd]->file->index;

	block_read(sb.start_index + curr_block, (void*)build);


	read = strlen(build);

	curr_block = fat[curr_block].fat_entry;

	while(curr_block != FAT_EOC)
	{	
		block_read(sb.start_index + curr_block, (void*)buffer);
		read += strlen(buffer);
		strcat(build,buffer);
		memset(buffer, 0, BLOCK_SIZE);
		curr_block = fat[curr_block].fat_entry;
		
		if(curr_block >= BLOCK_SIZE * sb.FAT_blocks)
		{
			break;
		}
	}

	build = build + FD_Array[fd]->offset;

	if(read > count)
	{
		memcpy(buf, (void*)build, count);
		fs_lseek(fd, FD_Array[fd]->offset + count);

//		free(build);
//		free(buffer);
		return count;
	}
	else
	{
		read -= FD_Array[fd]->offset;
		memcpy(buf, (void*)build, read);
		fs_lseek(fd, FD_Array[fd]->offset + read);

//		free(buffer);
//		free(build);
		return read;
	}
	
	
}	
