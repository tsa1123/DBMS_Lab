#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer(){
	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		metainfo[bufferIndex].free = true;
	}
}

StaticBuffer::~StaticBuffer(){}

int StaticBuffer::getFreeBuffer(int blockNum){
	if(blockNum<0 || blockNum>=DISK_BLOCKS)return E_OUTOFBOUND;

	int bufferNum=-1;

	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		if(metainfo[bufferIndex].free){
			bufferNum=bufferIndex;
			break;	
		}
	}
	metainfo[bufferNum].free = false;
	metainfo[bufferNum].dirty = false;
	metainfo[bufferNum].blockNum = blockNum;
	

	return bufferNum;
}

int StaticBuffer::getBufferNum(int blockNum){
	if(blockNum<0 || blockNum>=DISK_BLOCKS)return E_OUTOFBOUND;

	for(int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++){
		if(metainfo[bufferIndex].blockNum==blockNum)return bufferIndex;
	}
	return E_BLOCKNOTINBUFFER;
}
