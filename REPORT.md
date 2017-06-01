# Project 4 File System

#### General Information   
- Authors: Kyle Wang and Alan Zhao   
- Source Files: fs.c, Makefile   
- Test Files: test_fs.c

# High Level Design Choices 
In our implemenation we created the neccessary struct to respresent the different logical parts of our filesystem. 
``` 
typedef struct __attribute__((__packed__)) root_entry{
	char filename[16];
	uint32_t size;
	uint16_t index;
	char padding[10];
}root_entry;

typedef struct __attribute__((__packed__)) FAT{		
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
```
In our root_entry struct we hold the filename, size, index, and padding. In our RD struct we have an array of 128 root_entry structs. In our FAT struct we have a fat_entry with a units16_t member called fat_entry that will store the value of the index of the FAT array. Our SB struct will hold the superblock, its signature, block_total, root_index, start_index, data_blocks, FAT_blocks, and padding. The FD struct will store the file descriptors that we created when opening a file, it will content the fd number, offset value, and a pointer to the root_entry that this file descriptor is associated with. 

We keep a global variable to keep tract of the free spaces we have in our root and FAT. Our FAT, SB, FD, SB,and RD structs will be global in order to be accessible from all of our functions. 
# **Implementation Details**

### Phase 1: Mounting/unmounting
#### int fs_mount(const char *diskname)
In this function we opened the disk with block_disk_open() and loaded all the content of the disk into our structs. We used malloc that create the size of our fat array, according to how many FAT_blocks we had. One thing worth noting is that our global Fat* fat was read in with a loop that read according to the number of FAT blocks that we had. We also had loops that calculated the number of Fat and root entires that were free. All the reading from block was performed with the block_read() function
#### int fs_umount(void)
We deallocated the fat that we malloced in mount and closed the disk. 
#### int fs_info(void)
With the Superblock data that we read into our sb struct, we print out alll neccessary information. rootFree and fatFree were calculated in our mount function. 

### Phase 2 File creation/deletion
#### int fs_create(const char *filename)
An important function that we use in fs_create() is the find_free_fat() function. This function returns the first free index in our fat. We perofrm our checks to make sure that the file that we are creating does not already exist in our root, and another loop to find the first free index in our root array. We than populate the root entry with the neccessary information. We set the index of the root entry to the first free index in the fat array that we found. We than write our root directory, and fat back to block. 
#### void fat_delete(int index)
In delete we search through our directory to look for the file that is to be deleted. If the file is found, we mreove it from our root, and follow the Fat indexes and delete all associated data blocks. We write all of our changes in data blocks and root directory back to disk. 
#### int fs_ls(void)
We perform a loop through all the entries in our root array, and if the filename is not blank, we print the name along the with size and index of the first data block. 

### Phase 3: File descriptor operations
In this phase we make use of the FD struct that we made and also the globa FD* FD_Array[32] array that we made to keep track of all of our file descriptors. 
### int fs_open(const char *filename)
First we look for a open file descriptor in our FD_Array, and save this index as free_index. We perform a loop through the root array to check if the filename that we are trying to open exists in our root. If this files exists, we set the the neccessary information in our FD struct with free_index into our FD_Array, and point the root_entry pointer in the struct to point at the file we found in our root array. We return the free_index as the file descriptor. 
### int fs_close(int fd)
We take the fd that the user wishes to close, and index into our FD_Array and reset all the values to that this index is now free to return file descriptors. 
### int fs_stat(int fd)
After checking if the passed in fd is valid, we index into FD_Array and return the file size.
### fs_lseek(int fd, size_t offset)
After checking if the passed in fd and offset is valid, we index into FD_Array and adjust the offset of the file. 
# Sources Used

- www.piazaa.com
- https://linux.die.net/man/2/mprotect
- http://pubs.opengroup.org/onlinepubs/7908799/xsh/sigaction.html


