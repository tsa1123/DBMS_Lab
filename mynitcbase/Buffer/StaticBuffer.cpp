#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer(){
	Disk::readBlock(blockAllocMap, 0);
	Disk::readBlock(blockAllocMap + BLOCK_SIZE, 1);
	Disk::readBlock(blockAllocMap + 2*BLOCK_SIZE, 2);
	Disk::readBlock(blockAllocMap + 3*BLOCK_SIZE, 3);

	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		metainfo[bufferIndex].free = true;
		metainfo[bufferIndex].dirty = false;
		metainfo[bufferIndex].timeStamp = -1;
		metainfo[bufferIndex].blockNum = -1;
	}
}

StaticBuffer::~StaticBuffer(){
	Disk::writeBlock(blockAllocMap, 0);
	Disk::writeBlock(blockAllocMap + BLOCK_SIZE, 1);
        Disk::writeBlock(blockAllocMap + 2*BLOCK_SIZE, 2);
        Disk::writeBlock(blockAllocMap + 3*BLOCK_SIZE, 3);

	for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++){
		if(!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty){
			Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
		} 
	}
}

int StaticBuffer::getFreeBuffer(int blockNum){
	if(blockNum<0 || blockNum>=DISK_BLOCKS)return E_OUTOFBOUND;

	for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++){
		if(!metainfo[bufferIndex].free){
			metainfo[bufferIndex].timeStamp++;
		}
	}

	int bufferNum=-1;

	int maxTSIndex = -1, maxTS = -1;
	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		if(metainfo[bufferIndex].free){
			bufferNum=bufferIndex;
			break;	
		}
		if(metainfo[bufferIndex].timeStamp > maxTS){
			maxTSIndex = bufferIndex;
			maxTS = metainfo[bufferIndex].timeStamp;
		}
	}
	
	if(bufferNum == -1){
		if(metainfo[maxTSIndex].dirty){
			Disk::writeBlock(blocks[maxTSIndex], metainfo[maxTSIndex].blockNum);
		}
		bufferNum = maxTSIndex;
	}

	metainfo[bufferNum].free = false;
	metainfo[bufferNum].dirty = false;
	metainfo[bufferNum].blockNum = blockNum;
	metainfo[bufferNum].timeStamp = 0;

	return bufferNum;
}

int StaticBuffer::setDirtyBit(int blockNum){
	int bufferNum = getBufferNum(blockNum);

	if(bufferNum == E_BLOCKNOTINBUFFER){
		return E_BLOCKNOTINBUFFER;
	}

	if(bufferNum == E_OUTOFBOUND){
		return E_OUTOFBOUND;
	}

	else{
		metainfo[bufferNum].dirty = true;
	}

	return SUCCESS;
}

int StaticBuffer::getBufferNum(int blockNum){
	if(blockNum<0 || blockNum>=DISK_BLOCKS)return E_OUTOFBOUND;

	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		if(metainfo[bufferIndex].blockNum==blockNum)return bufferIndex;
	}
	return E_BLOCKNOTINBUFFER;
}
