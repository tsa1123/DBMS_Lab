#include "BPlusTree.h"
#include <cstring>

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]){
        if(relId==RELCAT_RELID || relId==ATTRCAT_RELID){
		return E_NOTPERMITTED;
        }

        AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	if(ret!=SUCCESS)return ret;

	if(attrCatEntry.rootBlock!=-1)return SUCCESS;

	IndLeaf rootBlockBuf;
	int rootBlock = rootBlockBuf.getBlockNum();

	if(rootBlock==E_DISKFULL){
		return E_DISKFULL;
	}
	attrCatEntry.rootBlock = rootBlock;
	AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

	int block = relCatEntry.firstBlk;

	unsigned char slotMap[relCatEntry.numSlotsPerBlk];
	Attribute record[relCatEntry.numAttrs];
	while(block != -1){
		RecBuffer recBlockBuf(block);
		recBlockBuf.getSlotMap(slotMap);
		
		for(int i=0; i<relCatEntry.numSlotsPerBlk; i++){
			if(slotMap[i]==SLOT_OCCUPIED){
				recBlockBuf.getRecord(record, i);
				RecId recId = RecId{block, i};

				ret = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);

				if(ret == E_DISKFULL){
					return E_DISKFULL;
				}
			}
		}
		
		HeadInfo head;
		recBlockBuf.getHeader(&head);
		block = head.rblock;
	}

	return SUCCESS;
}

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op){
	IndexId searchIndex;
	AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

	int block, index;

	if(searchIndex.block==-1 && searchIndex.index==-1){
		block = attrCatEntry.rootBlock;
		index = 0;

		if(block==-1){
			return RecId{-1, -1};
		}
	}
	else{
		block = searchIndex.block;
		index = searchIndex.index + 1;

		IndLeaf leaf(block);
		HeadInfo leafHead;
		leaf.getHeader(&leafHead);

		if(index>=leafHead.numEntries){
			block = leafHead.rblock;
			index = 0;

			if(block==-1){
				return RecId{-1, -1};
			}
		}
	}

	while(StaticBuffer::getStaticBlockType(block)==IND_INTERNAL){
		IndInternal internalBlk(block);

		HeadInfo intHead;
		internalBlk.getHeader(&intHead);

		InternalEntry intEntry;
		if(op==NE || op==LT || op==LE){
			InternalEntry intEntry;
			internalBlk.getEntry(&intEntry, 0);

			block = intEntry.lChild;
		}
		else{
			int i=0;
			InternalEntry intEntry;
			if(op==GT){
				for(; i<intHead.numEntries; i++){
					internalBlk.getEntry(&intEntry, i);
					if(compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType)>0)break;
				}
			}
			else{
				for(; i<intHead.numEntries; i++){
					internalBlk.getEntry(&intEntry, i);
					if(compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType)>=0)break;
				}
			}

			if(i<intHead.numEntries){
				block = intEntry.lChild;
			}
			else{
				internalBlk.getEntry(&intEntry, intHead.numEntries-1);
				block = intEntry.rChild;
			}
		}
	}

	while(block!=-1){
		IndLeaf leafBlk(block);
		HeadInfo leafHead;

		leafBlk.getHeader(&leafHead);

		Index leafEntry;

		while(index < leafHead.numEntries){
			leafBlk.getEntry(&leafEntry, index);

			int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

			 if (
                		(op == EQ && cmpVal == 0) ||
                		(op == LE && cmpVal <= 0) ||
                		(op == LT && cmpVal < 0) ||
                		(op == GT && cmpVal > 0) ||
                		(op == GE && cmpVal >= 0) ||
                		(op == NE && cmpVal != 0)
            		) {
				searchIndex = {block, index};
				AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
				return RecId{leafEntry.block, leafEntry.slot};
			}
			else if((op == EQ || op == LE || op == LT) && cmpVal > 0){
				return RecId{-1, -1};
			}
			index++;
		}
		if(op!=NE){
			break;
		}

		block = leafHead.rblock;
		index = 0;
	}

	return RecId{-1, -1};
}

int BPlusTree::bPlusDestroy(int rootBlockNum){
	if(rootBlockNum<0 || rootBlockNum>=DISK_BLOCKS){
		return E_OUTOFBOUND;
	}

	int type = StaticBuffer::getStaticBlockType(rootBlockNum);

	if(type == IND_LEAF){
		IndLeaf treeBlockBuf(rootBlockNum);

		treeBlockBuf.releaseBlock();

		return SUCCESS;
	}
	else if(type == IND_INTERNAL){
		IndInternal treeBlockBuf(rootBlockNum);

		HeadInfo head;
		treeBlockBuf.getHeader(&head);

		InternalEntry entry;

		for(int i=0; i<head.numEntries; i++){
			treeBlockBuf.getEntry(&entry, i);
			bPlusDestroy(entry.lChild);
		}
		bPlusDestroy(entry.rChild);

		treeBlockBuf.releaseBlock();

		return SUCCESS;
	}
	else{
		return E_INVALIDBLOCK;
	}
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId){
	AttrCatEntry attrCatEntry;
	int retVal = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
	if(retVal != SUCCESS)return retVal;

	int blockNum = attrCatEntry.rootBlock;

	if(blockNum == -1){
		return E_NOINDEX;
	}

	int leafBlkNum = findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType);
	Index entry;
	memcpy(&(entry.attrVal), &(attrVal), ATTR_SIZE);
	entry.block = recId.block;
	entry.slot = recId.slot;
	retVal = insertIntoLeaf(relId, attrName, leafBlkNum, entry);

	if(retVal == E_DISKFULL){
		bPlusDestroy(blockNum);
		attrCatEntry.rootBlock = -1;
		AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

		return E_DISKFULL;
	}
	
	return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType){
	int blockNum = rootBlock;
	
	HeadInfo head;
	while(StaticBuffer::getStaticBlockType(blockNum)!=IND_LEAF){
		IndInternal indBuffer(blockNum);
		indBuffer.getHeader(&head);
		
		InternalEntry entry;
		int i=0;
		for(; i<head.numEntries; i++){
			indBuffer.getEntry(&entry, i);
			if(compareAttrs(attrVal, entry.attrVal, attrType)<=0){
				break;	
			}
		}
		
		if(i==head.numEntries){
			indBuffer.getEntry(&entry, head.numEntries -1);
			blockNum = entry.rChild;
		}
		else{
			indBuffer.getEntry(&entry, i);
			blockNum = entry.lChild;
		}
	}

	return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry){
	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

	IndLeaf leafBuffer(blockNum);

	HeadInfo blockHeader;
	leafBuffer.getHeader(&blockHeader);

	Index indices[blockHeader.numEntries + 1];
	int j=0;
	bool inserted = false;
	for(; j<blockHeader.numEntries; j++){
		leafBuffer.getEntry(&indices[j], j);
		if(compareAttrs(indexEntry.attrVal, indices[j].attrVal, attrCatEntry.attrType)<0){
			memcpy(&(indices[j].attrVal),&(indexEntry.attrVal), ATTR_SIZE);
			indices[j].block = indexEntry.block;
			indices[j].slot = indexEntry.slot;
			
			inserted = true;

			for(;j<blockHeader.numEntries; j++){
				leafBuffer.getEntry(&indices[j+1], j);
			}

			break;
		}
	}
	if(!inserted){
		memcpy(&(indices[j].attrVal),&(indexEntry.attrVal), ATTR_SIZE);
                indices[j].block = indexEntry.block;
                indices[j].slot = indexEntry.slot;
	}

	if(blockHeader.numEntries < MAX_KEYS_LEAF){
		blockHeader.numEntries++;
		leafBuffer.setHeader(&blockHeader);

		for(int i=0; i<blockHeader.numEntries; i++){
			leafBuffer.setEntry(&indices[i], i);
		}

		return SUCCESS;
	}

	int newRightBlk = splitLeaf(blockNum, indices);

	if(newRightBlk == E_DISKFULL){
		return E_DISKFULL;
	}
	
	int retVal;
	if(blockHeader.pblock != -1){
		InternalEntry internalEntry;
		internalEntry.lChild = blockNum;
		memcpy(&(internalEntry.attrVal), &(indices[MIDDLE_INDEX_LEAF].attrVal), ATTR_SIZE);
		internalEntry.rChild = newRightBlk;
		retVal = insertIntoInternal(relId, attrName, blockHeader.pblock, internalEntry);	
	}
	else{
		retVal = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
	}

	if(retVal != SUCCESS)return retVal;
	return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]){
	IndLeaf rightBlk;
	IndLeaf leftBlk(leafBlockNum);

	int rightBlkNum = rightBlk.getBlockNum();
	int leftBlkNum = leftBlk.getBlockNum();
	
	if(rightBlkNum == E_DISKFULL){
		return E_DISKFULL;
	}	
	HeadInfo leftBlkHeader, rightBlkHeader;
	leftBlk.getHeader(&leftBlkHeader);
	rightBlk.getHeader(&rightBlkHeader);

	rightBlkHeader.blockType = leftBlkHeader.blockType;
	rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1)/2;
	rightBlkHeader.pblock = leftBlkHeader.pblock;
	rightBlkHeader.lblock = leftBlkNum;
	rightBlkHeader.rblock = leftBlkHeader.rblock;
	rightBlk.setHeader(&rightBlkHeader);

	leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1)/2;
	leftBlkHeader.rblock = rightBlkNum;
	leftBlk.setHeader(&leftBlkHeader);

	for(int i=0; i<=MIDDLE_INDEX_LEAF; i++){
		leftBlk.setEntry(&indices[i], i);
		rightBlk.setEntry(&indices[MIDDLE_INDEX_LEAF + 1 + i], i);
	}

	return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry){
	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

	IndInternal intBlk(intBlockNum);
	HeadInfo blockHeader;
	intBlk.getHeader(&blockHeader);
	
	InternalEntry internalEntries[blockHeader.numEntries + 1];
	int i=0;
	for(; i<blockHeader.numEntries; i++){
		intBlk.getEntry(&internalEntries[i], i);
		if(compareAttrs(intEntry.attrVal, internalEntries[i].attrVal, attrCatEntry.attrType)<0){
			internalEntries[i].lChild = intEntry.lChild;
			internalEntries[i].rChild = intEntry.rChild;
			memcpy(&(internalEntries[i].attrVal), &(intEntry.attrVal), ATTR_SIZE);

		        for(int j=i; j<blockHeader.numEntries; j++){
                		intBlk.getEntry(&internalEntries[j+1], j);
		        }

			break;
		}
	}
	if(i==blockHeader.numEntries){
	        internalEntries[i].lChild = intEntry.lChild;
                internalEntries[i].rChild = intEntry.rChild;
	        memcpy(&(internalEntries[i].attrVal), &(intEntry.attrVal), ATTR_SIZE);	
	}
	else{
		internalEntries[i+1].lChild = intEntry.rChild;
	}

	if(blockHeader.numEntries < MAX_KEYS_INTERNAL){
		blockHeader.numEntries++;
		intBlk.setHeader(&blockHeader);

		for(i=0; i<blockHeader.numEntries; i++){
			intBlk.setEntry(&internalEntries[i], i);
		}

		return SUCCESS;
	}

	int newRightBlk = splitInternal(intBlockNum, internalEntries);
	if(newRightBlk == E_DISKFULL){
		bPlusDestroy(intEntry.rChild);

		return E_DISKFULL;
	}

	int retVal;
	if(blockHeader.pblock != -1){
		InternalEntry middleEntry;
		middleEntry.lChild = intBlockNum;
		memcpy(&(middleEntry.attrVal), &(internalEntries[MIDDLE_INDEX_INTERNAL].attrVal), ATTR_SIZE);
		middleEntry.rChild = newRightBlk;
		retVal = insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
	}
	else{
		retVal = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
	}

	if(retVal != SUCCESS)return retVal;
	return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]){
	IndInternal rightBlk;
	IndInternal leftBlk(intBlockNum);

	int rightBlkNum = rightBlk.getBlockNum();
	int leftBlkNum = leftBlk.getBlockNum();

	if(rightBlkNum == E_DISKFULL){
		return E_DISKFULL;
	}

	HeadInfo leftBlkHeader, rightBlkHeader;
	leftBlk.getHeader(&leftBlkHeader);
	rightBlk.getHeader(&rightBlkHeader);

	rightBlkHeader.numEntries = (MAX_KEYS_INTERNAL)/2;
	rightBlkHeader.pblock = leftBlkHeader.pblock;
	rightBlk.setHeader(&rightBlkHeader);

	leftBlkHeader.numEntries = (MAX_KEYS_INTERNAL)/2;
	leftBlk.setHeader(&leftBlkHeader);

	for(int i=0; i<MIDDLE_INDEX_INTERNAL; i++){
		leftBlk.setEntry(&internalEntries[i], i);
		rightBlk.setEntry(&internalEntries[MIDDLE_INDEX_INTERNAL + 1 + i], i);
	}

	HeadInfo head;

	for(int i=0; i<rightBlkHeader.numEntries; i++){
		BlockBuffer buffer(internalEntries[MIDDLE_INDEX_INTERNAL+1+i].rChild);
		buffer.getHeader(&head);
		head.pblock = rightBlkNum;
		buffer.setHeader(&head);
	}
	BlockBuffer buffer(internalEntries[MIDDLE_INDEX_INTERNAL+1].lChild);
	buffer.getHeader(&head);
	head.pblock = rightBlkNum;
	buffer.setHeader(&head);

	return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild){
	AttrCatEntry attrCatEntry;
	AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

	IndInternal newRootBlk;
	int newRootBlkNum = newRootBlk.getBlockNum();
	if(newRootBlkNum == E_DISKFULL){
		bPlusDestroy(rChild);
		return E_DISKFULL;
	}

	HeadInfo head;
	newRootBlk.getHeader(&head);
	head.numEntries = 1;
	newRootBlk.setHeader(&head);

	InternalEntry entry;
	memcpy(&(entry.attrVal), &(attrVal), ATTR_SIZE);
	entry.lChild = lChild;
	entry.rChild = rChild;
	newRootBlk.setEntry(&entry, 0);

	BlockBuffer leftBlk(lChild);
	BlockBuffer rightBlk(rChild);
	
	leftBlk.getHeader(&head);
	head.pblock = newRootBlkNum;
	leftBlk.setHeader(&head);

	rightBlk.getHeader(&head);
	head.pblock = newRootBlkNum;
	rightBlk.setHeader(&head);

	attrCatEntry.rootBlock = newRootBlkNum;
	AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

	return SUCCESS;
}
