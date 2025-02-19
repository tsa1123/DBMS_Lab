#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable(){
	// Initializing values in relCache and attrCache to nullptr
	// and in tableMetaInfo to be free
	for(int i=0;i<MAX_OPEN;i++){
		RelCacheTable::relCache[i] = nullptr;
		AttrCacheTable::attrCache[i] = nullptr;
		tableMetaInfo[i].free = true;
	}

	// Loading RELCAT and ATTRCAT into relation cache
	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);
	
	struct RelCacheEntry relCacheEntry;
	RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
	relCacheEntry.recId.block = RELCAT_BLOCK;
	relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

	RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	*(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

	relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);
	RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
	relCacheEntry.recId.block = RELCAT_BLOCK;
	relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

	RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	*(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;


	// Loading RELCAT and ATTRCAT into attribute cache
	RecBuffer attrCatBlock(ATTRCAT_BLOCK);
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	AttrCacheEntry* prev = nullptr, *head = nullptr;
	for(int i=0;i<6;i++){
		attrCatBlock.getRecord(attrCatRecord, i);
		AttrCacheEntry* curr = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
		curr->recId.block = ATTRCAT_BLOCK;
		curr->recId.slot = i;
		if(i==0)head = curr;
		else if(i>0)prev->next = curr;
		prev = curr;
	}
	prev->next = nullptr;
	AttrCacheTable::attrCache[RELCAT_RELID] = head;

	prev=head=nullptr;
	for(int i=6;i<12;i++){
		attrCatBlock.getRecord(attrCatRecord, i);
		AttrCacheEntry* curr=(struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
		curr->recId.block = ATTRCAT_BLOCK;
		curr->recId.slot = i;
		if(i==6)head = curr;
		else if(i>6)prev->next = curr;
		prev = curr;
	}
	prev->next = nullptr;
	AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

	// Setting tableMetaInfo entries
	tableMetaInfo[RELCAT_RELID].free = false;
	tableMetaInfo[ATTRCAT_RELID].free = false;
	strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);
	strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable(){
	for(int i=2; i<MAX_OPEN; i++){
		if(!tableMetaInfo[i].free){
			OpenRelTable::closeRel(i);
		}
	}


	free(RelCacheTable::relCache[RELCAT_RELID]);
	free(RelCacheTable::relCache[ATTRCAT_RELID]);
	
	AttrCacheEntry* curr = AttrCacheTable::attrCache[RELCAT_RELID];
	AttrCacheEntry* next = nullptr;
	while(curr!=nullptr){
		next = curr->next;
		free(curr);
		curr = next;
	}
	curr = AttrCacheTable::attrCache[ATTRCAT_RELID];
	while(curr!=nullptr){
		next = curr->next;
		free(curr);
		curr = next;
	}
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]){
	for(int i=0; i<MAX_OPEN; i++){
		if(!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName, relName)==0)return i;
	}
	return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]){
	int ret = getRelId(relName);
	if(ret != E_RELNOTOPEN)return ret;

	int relId = getFreeOpenRelTableEntry();
	if(relId == E_CACHEFULL)return E_CACHEFULL;

	RelCacheTable::resetSearchIndex(RELCAT_RELID);
	Attribute attrVal;
	strcpy(attrVal.sVal, relName);
	char name[] = RELCAT_ATTR_RELNAME;
	RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, name, attrVal, EQ);

	if(relcatRecId.block == -1 && relcatRecId.slot == -1)return E_RELNOTEXIST;

	RecBuffer relCatBuffer(relcatRecId.block);
	Attribute record[RELCAT_NO_ATTRS];
	relCatBuffer.getRecord(record, relcatRecId.slot);
	
	RelCacheEntry relCacheEntry;
	RelCacheTable::recordToRelCatEntry(record, &relCacheEntry.relCatEntry);
	relCacheEntry.recId = relcatRecId;

	RelCacheTable::relCache[relId] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	*(RelCacheTable::relCache[relId]) = relCacheEntry;

	AttrCacheEntry *listHead, *prev=nullptr;
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	while(true){
		char attrName[] = ATTRCAT_ATTR_RELNAME;
		RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, attrName, attrVal, EQ);
	       	if(attrCatRecId.block == -1 && attrCatRecId.slot == -1){
	       		break;
	       	}	
		RecBuffer attrCatBuffer(attrCatRecId.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

		AttrCacheEntry *currEntry = (struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &currEntry->attrCatEntry);
		currEntry->next = nullptr;
		if(prev == nullptr){
			listHead = currEntry;
		}
		else prev->next = currEntry;
		
		prev = currEntry;
	}
	AttrCacheTable::attrCache[relId] = listHead;

	tableMetaInfo[relId].free = false;
	strcpy(tableMetaInfo[relId].relName, relName);
	
	return relId;
}

int OpenRelTable::closeRel(int relId){
	if(relId == RELCAT_RELID || relId == ATTRCAT_RELID){
		return E_NOTPERMITTED;
	}
	if(relId<0 || relId>=MAX_OPEN){
		return E_OUTOFBOUND;
	}
	if(tableMetaInfo[relId].free){
		return E_RELNOTOPEN;
	}
	
	if(RelCacheTable::relCache[relId]->dirty){
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), relCatRecord);
		
		RecId recId = RelCacheTable::relCache[relId]->recId;
		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord, recId.slot);
	}

	free(RelCacheTable::relCache[relId]);

	AttrCacheEntry *next, *curr = AttrCacheTable::attrCache[relId];
        while(curr!=nullptr){
                next = curr->next;
                free(curr);
                curr = next;
        }

	tableMetaInfo[relId].free = true;
	RelCacheTable::relCache[relId] = nullptr;
	AttrCacheTable::attrCache[relId] = nullptr;

	return SUCCESS;
}

int OpenRelTable::getFreeOpenRelTableEntry(){
	for(int i=0; i<MAX_OPEN; i++){
		if(tableMetaInfo[i].free)return i;
	}
	return E_CACHEFULL;
}
