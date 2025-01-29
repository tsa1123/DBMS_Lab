#include "BlockAccess.h"

#include <cstring>

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
