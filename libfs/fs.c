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


int find_free_fat(void)
{
	for(int i = 0; i < BLOCK_SIZE * sb.FAT_blocks; i++)
	{
		if(fat[i].fat_entry == 0)
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

int fs_create(const char *filename)
{
	if(error_check() == -1)
	{
		return -1;
	}
	
	if(strlen(filename) > 16)
	{
		return -1;
	}

	int rd_free_index = 0, fat_free_index = 0;
	fat_free_index = find_free_fat();

	if(fat_free_index == -1)
	{
		return -1;
	}

	for(int i = 0; i < 128; i++)
	{
		if(rd.root_array[i].filename[0] != 0)
		{
			if(strcmp(rd.root_array[i].filename, filename) == 0)
			{
				return -1;
			}
		}
	}

	while(rd.root_array[rd_free_index].filename[0] != 0)
	{
		rd_free_index++;

		if(rd_free_index == 128)
		{
			return -1;
		}
	}

	rd.root_array[rd_free_index].size = 0;

	rd.root_array[rd_free_index].index = fat_free_index; //fat_free_index;
	strcpy(rd.root_array[rd_free_index].filename, filename);

	fat[fat_free_index].fat_entry = FAT_EOC;

	rootFree--;
	fatFree--;

	block_write(sb.root_index,&rd);

	for(int i = 0; i < sb.FAT_blocks; i++)
	{
		block_write(1 + i,(void*)fat + i*(BLOCK_SIZE));
	}

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
			block_write(sb.root_index,&rd);
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
	int curr_block, write = count, next = 0, rd_loc = -1;
	int offset = FD_Array[fd]->offset;
	curr_block = FD_Array[fd]->file->index + (FD_Array[fd]->offset / BLOCK_SIZE);

	printf("a %d\n", (int)fat[0].fat_entry);
	printf("b %d\n", (int)fat[1].fat_entry);

	if(error_check() == -1)
	{
		puts("stderr");
		return -1;
	}

	if(fd < 0 || fd >= 32)
	{
		puts("inval fd");
		return -1;
	}
	printf("a %d\n", (int)fat[0].fat_entry);
	printf("b %d\n", (int)fat[1].fat_entry);

	if(offset > BLOCK_SIZE * sb.FAT_blocks || offset > FD_Array[fd]->file->size || curr_block >= sb.data_blocks)
	{
		puts("inval index");
		return 0;
	}

	for(int i = 0; i < 128; i++)
	{
		if(strcmp(rd.root_array[i].filename, FD_Array[fd]->file->filename) == 0)
		{
			puts("rd found");
			rd_loc = i;
			break;
		}
	}

	printf("a %d\n", (int)fat[0].fat_entry);
	printf("b %d\n", (int)fat[1].fat_entry);

	if(rd_loc < 0)
	{
		return -1;
	}

	char* build = malloc(BLOCK_SIZE * sb.FAT_blocks);
	char* block = malloc(BLOCK_SIZE);

	if(FD_Array[fd]->file->size != 0)
	{
		block_read(curr_block, build);
		strcat(build, buf);
	}
	else
	{
		strcat(build,buf);
	}


	if(count + offset <= BLOCK_SIZE)				// small operation
	{
		puts("small");
		block_write(curr_block, build);
		FD_Array[fd]->file->size = count;
		rd.root_array[rd_loc].size = count;
		block_write(sb.root_index,&rd);

		return count;
	}
	else								// big operation
	{
		puts("big");
		memcpy(block,build, BLOCK_SIZE);
		block_write(curr_block, block);
		memset(block, 0, BLOCK_SIZE);
		write -= BLOCK_SIZE - (offset % 4096);
		build += 4096;
		next = fat[curr_block].fat_entry;

		while(next != FAT_EOC)
		{
			curr_block = next;

			if(write < 4096)				// last block
			{
				memcpy(block, build, write);
				block_write(curr_block, block);
				FD_Array[fd]->file->size = count;
				return count;
			}
			else
			{
				memcpy(block,build, BLOCK_SIZE);
				block_write(curr_block, block);
				memset(block, 0, BLOCK_SIZE);
				write -= BLOCK_SIZE - (offset % 4096);
				build += 4096;
			}
		
			next = fat[curr_block].fat_entry;
		}
		FD_Array[fd]->file->size = count;
		return count;
//		next = find_fat_free();
		
	}


// small operations
// count + offset < block_size == small op
	// no EOC or find_free_fat calls




// big operations
// count + offset > block_size
// count and sizeof(buf) > block_size
	// offset vs no offset
		// iterate through fat

// extensions	offset >= filesize || count + offset > filesize is a possibility
// floor(count || sizeof(buf) ) + offset >= filesize
	// offset vs no offset
	// call find_fat_free and update EOC accordingly



//count + offset > block_size > count + offset > filesize

/*	if(offset == 0)
	{
		memcpy(build, buf, count);

		if(strlen(build) < BLOCK_SIZE)
		{
			memcpy(buffer, build, strlen(build));
			write = strlen(build) - (offset % 4096);
			block_write(curr_block, buffer);
			fat[curr_block].fat_entry = FAT_EOC;
			return write;
		}
		else
		{
			memcpy(buffer, build, BLOCK_SIZE);
			write = 4096 - (offset % 4096);
			build += BLOCK_SIZE;
			block_write(curr_block, buffer);
		}

		if(fat[curr_block].fat_entry == FAT_EOC)
		{
			next = find_free_fat();
		}
		memset(buffer, 0, 4096);
		next = find_free_fat();
		while(next != -1)
		{
			fat[curr_block].fat_entry = next;
			curr_block = next;
			memcpy(buffer, build, BLOCK_SIZE);
			block_write(curr_block, buffer);
			build += BLOCK_SIZE;
			write += strlen(buffer)

			if(strlen(buffer) < 4096)
			{
				break;
			}

			next = find_free_fat();
		}
	}

	else	
	{
		block_read(curr_block, (void*)build);					// assume count >= sizeof(buf)
		strcat(build, buf);							// build holds everything

		if(strlen(build) < BLOCK_SIZE)
		{
			memcpy(buffer, build, strlen(build));
			write = strlen(build) - (offset % 4096);
			block_write(curr_block, buffer);
			fat[curr_block].fat_entry = FAT_EOC;
			return write;
		}
		else
		{
			memcpy(buffer, build, BLOCK_SIZE);
			write = 4096 - (offset % 4096);
			build += BLOCK_SIZE;
			block_write(curr_block, buffer);
			fat[curr_block].fat_entry = FAT_EOC;
		}

		memset(buffer, 0, 4096);


		next = find_free_fat();
	
		while(next != -1)
		{
			fat[curr_block].fat_entry = next;
			curr_block = next;
			memcpy(buffer, build, BLOCK_SIZE);
			block_write(curr_block, buffer);
			build += BLOCK_SIZE;
			write += strlen(buffer)

			if(strlen(buffer) < 4096)
			{
				break;
			}

			next = find_free_fat();
		}


		fat[curr_block].fat_entry = FAT_EOC;
		return write;
	}
*/
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
	printf("curr_block: %d\n", curr_block);

	block_read(sb.start_index + curr_block, (void*)build);

	read = strlen(build);

	curr_block = fat[curr_block].fat_entry;

	printf("curr_block: %d\n", curr_block);


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

		free(build);
		return count;
	}
	else
	{
		read -= FD_Array[fd]->offset;
		memcpy(buf, (void*)build, read);
		fs_lseek(fd, FD_Array[fd]->offset + read);

		free(buffer);
		return read;
	}
	
	
}	
