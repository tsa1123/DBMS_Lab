#include "BlockAccess.h"

#include <cstring>

int BlockAccess::insert(int relId, union Attribute *record){
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	
	RecId recId = {-1, -1};
	int numAttrs = relCatEntry.numAttrs;
	int numSlots = relCatEntry.numSlotsPerBlk;
	int prevBlockNum = -1;
	int blockNum = relCatEntry.firstBlk;

	struct HeadInfo head;
	while(blockNum!=-1){
		RecBuffer recBuffer(blockNum);
		recBuffer.getHeader(&head);
		if(head.numEntries < numSlots){
			unsigned char slotMap[numSlots];
			recBuffer.getSlotMap(slotMap);
			for(int i=0; i<numSlots; i++){
				if(slotMap[i]==SLOT_UNOCCUPIED){
					recId.block = blockNum;
					recId.slot = i;
					break;
				}
			}
			break;
		}
		prevBlockNum = blockNum;
		blockNum = head.rblock;
	}

	if(recId.block == -1 && recId.slot == -1){
		if(relId == RELCAT_RELID){
			return E_MAXRELATIONS;
		}
		RecBuffer* buffer = new RecBuffer();
		
		blockNum = buffer->getBlockNum();

		if(blockNum == E_DISKFULL){
			return E_DISKFULL;
		}

		recId.block = blockNum;
		recId.slot = 0;

		head.blockType = REC;
		head.pblock = -1;
            	head.lblock = prevBlockNum;
            	head.rblock = -1;
		head.numEntries = 0;
            	head.numSlots = numSlots;
		head.numAttrs = numAttrs;
		buffer->setHeader(&head);
		
		unsigned char slotMap[numSlots];
		for(int i=0; i<numSlots; i++)slotMap[i] = SLOT_UNOCCUPIED;
		buffer->setSlotMap(slotMap);
		delete buffer;

		if(prevBlockNum != -1){
			RecBuffer prevBlkBuffer(prevBlockNum);
			prevBlkBuffer.getHeader(&head);
			head.rblock = recId.block;
			prevBlkBuffer.setHeader(&head);
		}
		else{
			relCatEntry.firstBlk = recId.block;
			RelCacheTable::setRelCatEntry(relId, &relCatEntry);
		}
		relCatEntry.lastBlk = recId.block;
		RelCacheTable::setRelCatEntry(relId, &relCatEntry);
	}
	RecBuffer recBuffer(recId.block);
	recBuffer.setRecord(record, recId.slot);

	unsigned char slotMap[numSlots];
	recBuffer.getSlotMap(slotMap);
	slotMap[recId.slot] = SLOT_OCCUPIED;
	recBuffer.setSlotMap(slotMap);

	relCatEntry.numRecs++;
	RelCacheTable::setRelCatEntry(relId, &relCatEntry);

	recBuffer.getHeader(&head);
	head.numEntries ++;
	recBuffer.setHeader(&head);
	
	return SUCCESS;
}

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op){
	RecId prevRecId;
	RelCacheTable::getSearchIndex(relId, &prevRecId);
	int block, slot;

	if(prevRecId.block == -1 && prevRecId.slot == -1){
		RelCatEntry relCatEntry;
		RelCacheTable::getRelCatEntry(relId, &relCatEntry);

		block = relCatEntry.firstBlk;
		slot = 0;
	}
	else{
		block = prevRecId.block;
		slot = prevRecId.slot + 1;
	}

	while(block!=-1){
		RecBuffer recBuffer(block);
		HeadInfo header;
		recBuffer.getHeader(&header);

		if(slot >= header.numSlots){
			block = header.rblock;
			slot = 0;
			continue;
		}

		unsigned char slotMap[header.numSlots];
		recBuffer.getSlotMap(slotMap);
		if(slotMap[slot] == SLOT_UNOCCUPIED){
			slot++;
			continue;
		}

		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
		union Attribute record[header.numAttrs];
		recBuffer.getRecord(record, slot);

		int cmpVal = compareAttrs(record[attrCatEntry.offset],attrVal,attrCatEntry.attrType);
		
		if((op==NE && cmpVal!=0) ||
		   (op==EQ && cmpVal==0) ||
		   (op==LT && cmpVal<0)  ||
		   (op==LE && cmpVal<=0) ||
		   (op==GT && cmpVal>0)  ||
		   (op==GE && cmpVal>=0)){
			RecId recId={block, slot};
			RelCacheTable::setSearchIndex(relId, &recId);
			return recId;
		}

		slot++;
	}

	return RecId{-1, -1};
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op){
	RecId recId;

	recId = linearSearch(relId, attrName, attrVal, op);
	if(recId.block == -1 && recId.slot == -1){
		return E_NOTFOUND;
	}

	RecBuffer buffer(recId.block);
	buffer.getRecord(record, recId.slot);

	return SUCCESS;
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute newRelationName;
	strcpy(newRelationName.sVal, newName);
	RecId recId = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, newRelationName, EQ);
	
	if(recId.block != -1 && recId.slot != -1){
		return E_RELEXIST;
	}
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute oldRelationName;
	strcpy(oldRelationName.sVal, oldName);
	recId = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelationName, EQ);

	if(recId.block == -1 && recId.slot == -1){
		return E_RELNOTEXIST;
	}

	RecBuffer relCatBuffer(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	relCatBuffer.getRecord(relCatRecord, recId.slot);

	strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);
	relCatBuffer.setRecord(relCatRecord, recId.slot);

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	int numAttrs = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	for(int i=0; i<numAttrs; i++){
		recId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

		RecBuffer attrCatBuffer(recId.block);
		attrCatBuffer.getRecord(attrCatRecord, recId.slot);
		strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
		attrCatBuffer.setRecord(attrCatRecord, recId.slot);
	}

	return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr;
	strcpy(relNameAttr.sVal, relName);
	RecId recId = linearSearch(RELCAT_RELID, (char*) RELCAT_ATTR_RELNAME, relNameAttr, EQ);

	if(recId.block == -1 && recId.slot == -1){
		return E_RELNOTEXIST;
	}

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	
	RecId attrToRenameRecId{-1, -1};
	Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
	while(true){
		recId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
		if(recId.block == -1 && recId.slot == -1){
			break;
		}

		RecBuffer attrCatBuffer(recId.block);
		attrCatBuffer.getRecord(attrCatEntryRecord, recId.slot);
		if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName)==0){
			attrToRenameRecId = recId;
		}

		if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName)==0){
			return E_ATTREXIST;
		}
	}

	if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1){
		return E_ATTRNOTEXIST;
	}

	RecBuffer attrCatBuffer(attrToRenameRecId.block);
        attrCatBuffer.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
	strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
	attrCatBuffer.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);

	return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]){
	if(strcmp(relName, RELCAT_RELNAME)==0 || strcmp(relName, ATTRCAT_RELNAME)==0){
		return E_NOTPERMITTED;
	}
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr;
	strcpy(relNameAttr.sVal, relName);

	RecId recId = BlockAccess::linearSearch(RELCAT_RELID, (char *)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
	if(recId.block==-1 && recId.slot==-1){
		return E_RELNOTEXIST;
	}

	RecBuffer relCatBuffer(RELCAT_BLOCK);
	Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
	relCatBuffer.getRecord(relCatEntryRecord, recId.slot);

	int block = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
	int nAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	
	struct HeadInfo head;
	while(block!=-1){
		RecBuffer recBuffer(block);
		recBuffer.getHeader(&head);
		block = head.rblock;
		recBuffer.releaseBlock();
	}

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	int numberOfAttributesDeleted = 0;

	while(true){
		RecId attrCatRecId;
		attrCatRecId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
		
		if(attrCatRecId.block==-1 && attrCatRecId.slot==-1){
			break;
		}

		numberOfAttributesDeleted++;

		RecBuffer attrCatBuffer(attrCatRecId.block);
		
		Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
		attrCatBuffer.getRecord(attrCatEntryRecord, attrCatRecId.slot);
		
		int rootBlock = attrCatEntryRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
		
		unsigned char slotMap[DISK_BLOCKS];
		attrCatBuffer.getSlotMap(slotMap);
		slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
		attrCatBuffer.setSlotMap(slotMap);

		attrCatBuffer.getHeader(&head);
		head.numEntries--;
		attrCatBuffer.setHeader(&head);

		if(head.numEntries == 0){
			RecBuffer prevBlockBuffer(head.lblock);
			struct HeadInfo prevHeader;
			prevBlockBuffer.getHeader(&prevHeader);
			prevHeader.rblock = head.rblock;
			prevBlockBuffer.setHeader(&prevHeader);

			if(head.rblock!=-1){
				RecBuffer nextBlockBuffer(head.rblock);
				struct HeadInfo nextHeader;
				nextBlockBuffer.getHeader(&nextHeader);
				nextHeader.lblock = head.lblock;
				nextBlockBuffer.setHeader(&nextHeader);
			}
			else{
				RelCatEntry relCatEntry;
				RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
				relCatEntry.lastBlk = head.lblock;
				RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
			}
			attrCatBuffer.releaseBlock();
		}

	}
	
	relCatBuffer.getHeader(&head);
	head.numEntries--;
	relCatBuffer.setHeader(&head);

	unsigned char slotMap[DISK_BLOCKS];
	relCatBuffer.getSlotMap(slotMap);
	slotMap[recId.slot] = SLOT_UNOCCUPIED;
	relCatBuffer.setSlotMap(slotMap);

	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
	relCatEntry.numRecs--;
	RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

	RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
	relCatEntry.numRecs -= numberOfAttributesDeleted;
	RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);

	return SUCCESS;
}

int BlockAccess::project(int relId, Attribute *record){
	RecId prevRecId;
	RelCacheTable::getSearchIndex(relId, &prevRecId);

	int block, slot;
	if(prevRecId.block==-1 && prevRecId.slot==-1){
		RelCatEntry relCatEntry;
		RelCacheTable::getRelCatEntry(relId, &relCatEntry);
		block = relCatEntry.firstBlk;
		slot = 0;
	}
	else{
		block = prevRecId.block;
		slot = prevRecId.slot + 1;
	}

	while(block!=-1){
		RecBuffer buffer(block);
		struct HeadInfo head;
		buffer.getHeader(&head);
		if(slot>=head.numSlots){
			block = head.rblock;
			slot = 0;
			continue;
		}

		unsigned char slotMap[DISK_BLOCKS];
		buffer.getSlotMap(slotMap);
		if(slotMap[slot] == SLOT_UNOCCUPIED){
			slot = slot+1;
		}
		else{
			break;
		}
	}

	if(block==-1){
		return E_NOTFOUND;
	}

	RecId nextRecId{block, slot};

	RelCacheTable::setSearchIndex(relId, &nextRecId);

	RecBuffer buffer(block);
	buffer.getRecord(record, slot);

	return SUCCESS;
}
