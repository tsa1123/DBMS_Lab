#include "BlockAccess.h"

#include <cstring>
<<<<<<< HEAD
=======

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
>>>>>>> 77970a1... Stage4
