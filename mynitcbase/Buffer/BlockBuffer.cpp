#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(char blockType){
	int retVal;
       	switch(blockType){
		case 'R':retVal = getFreeBlock(REC);
		 	break;
		case 'I':retVal = getFreeBlock(IND_INTERNAL);
			 break;
		case 'L':retVal = getFreeBlock(IND_LEAF);
		 	break;
	}
	this->blockNum = retVal;
}

BlockBuffer::BlockBuffer(int blockNum){
	this->blockNum=blockNum;
}

RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R'){}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

int BlockBuffer::getBlockNum(){
	return this->blockNum;
}

int BlockBuffer::getHeader(struct HeadInfo *head){
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS)return ret;

	memcpy(&head->numSlots, bufferPtr+24, 4);
	memcpy(&head->numEntries, bufferPtr+16, 4);
	memcpy(&head->numAttrs, bufferPtr+20, 4);
	memcpy(&head->rblock, bufferPtr+12, 4);
	memcpy(&head->lblock, bufferPtr+8, 4);

	return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head){
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS)return ret;

	struct HeadInfo *bufferHeader = (struct HeadInfo*)bufferPtr;

	bufferHeader->blockType = head->blockType;
	bufferHeader->pblock = head->pblock;
	bufferHeader->lblock = head->lblock;
	bufferHeader->rblock = head->rblock;
	bufferHeader->numEntries = head->numEntries;
	bufferHeader->numAttrs = head->numAttrs;
	bufferHeader->numSlots = head->numSlots;

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS)return ret;

	return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){
	int blockIndex = -1;
	for(int index=0; index<DISK_BLOCKS; index++){
		if(StaticBuffer::blockAllocMap[index] == UNUSED_BLK){
			blockIndex = index;
			break;
		}
	}
	if(blockIndex == -1)return E_DISKFULL;
	
	this->blockNum = blockIndex;

	int bufferNum = StaticBuffer::getFreeBuffer(blockIndex);
	
	struct HeadInfo head = {pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0};
	setHeader(&head);

	setBlockType(blockType);

	return blockIndex;
}

int BlockBuffer::setBlockType(int blockType){
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS)return ret;
	
	*((int32_t*)bufferPtr) = blockType;
	StaticBuffer::blockAllocMap[this->blockNum] = blockType;
	
	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS)return ret;
	
	return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum){
	struct HeadInfo head;
	this->getHeader(&head);
	
	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS)return ret;
	
	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);
	memcpy(rec, slotPointer, recordSize);

	return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum){
	unsigned char* bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret!=SUCCESS)return ret;

	struct HeadInfo head;
	getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	if(slotNum >= slotCount)return E_OUTOFBOUND;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);
	memcpy(slotPointer, rec, recordSize);

	StaticBuffer::setDirtyBit(this->blockNum);

	return SUCCESS;
}

void BlockBuffer::releaseBlock(){
	if(this->blockNum==-1)return;
	else{
		int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
		if(bufferNum<0 || bufferNum>=BUFFER_CAPACITY) return;
		StaticBuffer::metainfo[bufferNum].free = true;

		StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
		this->blockNum = -1;
		
	}
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
	if(bufferNum!=E_BLOCKNOTINBUFFER){
		StaticBuffer::metainfo[bufferNum].timeStamp=0;
		for(int bufferIndex=0; bufferIndex<BUFFER_CAPACITY; bufferIndex++){
			if(bufferIndex!=bufferNum && !StaticBuffer::metainfo[bufferIndex].free){
				(StaticBuffer::metainfo[bufferIndex].timeStamp)++;
			}
		}
	}
	else{
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
		if(bufferNum == E_OUTOFBOUND)return E_OUTOFBOUND;

		Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
	}
	*buffPtr = StaticBuffer::blocks[bufferNum];

	return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char* slotMap){
	unsigned char *bufferPtr;

	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS)return ret;

	struct HeadInfo head;
	getHeader(&head);

	int slotCount = head.numSlots;

	unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;
	memcpy(slotMap, slotMapInBuffer, slotCount);
	return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char* slotMap){
	unsigned char *bufferPtr;

        int ret = loadBlockAndGetBufferPtr(&bufferPtr);
        if(ret != SUCCESS)return ret;

        struct HeadInfo head;
        getHeader(&head);

        int slotCount = head.numSlots;

        unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;
        memcpy(slotMapInBuffer, slotMap, slotCount);

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS)return ret;

        return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType){
	double diff;
	
	if(attrType == STRING){
		diff = strcmp(attr1.sVal, attr2.sVal);
	}
	else diff = attr1.nVal - attr2.nVal;

	if(diff<0)return -1;
	else if(diff>0)return 1;
	else return 0;
}
