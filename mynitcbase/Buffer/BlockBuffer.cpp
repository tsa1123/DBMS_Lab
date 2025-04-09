#include "BlockBuffer.h"
#include<iostream>
#include <cstdlib>
#include <cstring>
int comparisons;
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

IndBuffer::IndBuffer(char blockType) : BlockBuffer::BlockBuffer(blockType){}

IndBuffer::IndBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum){}

IndInternal::IndInternal() : IndBuffer::IndBuffer('I'){}

IndInternal::IndInternal(int blockNum) : IndBuffer::IndBuffer(blockNum){}

IndLeaf::IndLeaf() : IndBuffer::IndBuffer('L'){}

IndLeaf::IndLeaf(int blockNum) : IndBuffer::IndBuffer(blockNum){}

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
	memcpy(&head->pblock, bufferPtr+4, 4);

	return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head){
	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret != SUCCESS)return ret;

	struct HeadInfo *bufferHeader = (struct HeadInfo*)bufferPtr;

	//bufferHeader->blockType = head->blockType;
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
	this->getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	if(slotNum >= slotCount)return E_OUTOFBOUND;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);
	memcpy(slotPointer, rec, recordSize);

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	
	if(ret!=SUCCESS){
		std::cout<<"[!] Error in code\n";
		exit(1);
	}

	return SUCCESS;
}

void BlockBuffer::releaseBlock(){
	if(this->blockNum==-1 || StaticBuffer::blockAllocMap[this->blockNum]==UNUSED_BLK)return;
	else{
		int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
		if(bufferNum>0 && bufferNum<BUFFER_CAPACITY){
			StaticBuffer::metainfo[bufferNum].free = true;
		}
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
	this->getHeader(&head);

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
        this->getHeader(&head);

        int slotCount = head.numSlots;

        unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;
        memcpy(slotMapInBuffer, slotMap, slotCount);

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if(ret!=SUCCESS)return ret;

        return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType){
	double diff;
	comparisons++;
	if(attrType == STRING){
		diff = strcmp(attr1.sVal, attr2.sVal);
	}
	else diff = attr1.nVal - attr2.nVal;

	if(diff<0)return -1;
	else if(diff>0)return 1;
	else return 0;
}

int IndInternal::getEntry(void *ptr, int indexNum){
	if(indexNum<0 || indexNum>=MAX_KEYS_INTERNAL){
		return E_OUTOFBOUND;
	}

	unsigned char *bufferPtr;

	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
        if(ret != SUCCESS)return ret;
	
	struct InternalEntry *internalEntry = (struct InternalEntry*)ptr;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

	memcpy(&(internalEntry->lChild), entryPtr, 4);
	memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
	memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

	return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum){
        if(indexNum<0 || indexNum>=MAX_KEYS_INTERNAL){
                return E_OUTOFBOUND;
        }

        unsigned char *bufferPtr;

        int ret = loadBlockAndGetBufferPtr(&bufferPtr);
        if(ret != SUCCESS)return ret;

        struct InternalEntry *internalEntry = (struct InternalEntry*)ptr;

        unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

        memcpy(entryPtr, &(internalEntry->lChild), 4);
        memcpy(entryPtr + 4, &(internalEntry->attrVal), ATTR_SIZE);
        memcpy(entryPtr + 20, &(internalEntry->rChild), 4);
	
	return StaticBuffer::setDirtyBit(this->blockNum);
}

int IndLeaf::getEntry(void *ptr, int indexNum){
	if(indexNum<0 || indexNum>=MAX_KEYS_LEAF){
		return E_OUTOFBOUND;
	}

	unsigned char *bufferPtr;

        int ret = loadBlockAndGetBufferPtr(&bufferPtr);
        if(ret != SUCCESS)return ret;

	struct Index *index = (struct Index*)ptr;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);

	memcpy(&(index->attrVal), entryPtr, ATTR_SIZE);
	memcpy(&(index->block), entryPtr + 16, 4);
	memcpy(&(index->slot), entryPtr + 20, 4);
	
	return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum){
	if(indexNum<0 || indexNum>=MAX_KEYS_LEAF){
		return E_OUTOFBOUND;
	}

	unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if(ret !=SUCCESS)return ret;

	struct Index *index = (struct Index*)ptr;

        unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);

        memcpy(entryPtr, &(index->attrVal), ATTR_SIZE);
        memcpy(entryPtr + 16, &(index->block), 4);
        memcpy(entryPtr + 20, &(index->slot), 4);

	return StaticBuffer::setDirtyBit(this->blockNum);
}
