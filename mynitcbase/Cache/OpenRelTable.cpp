#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTable::OpenRelTable(){
	for(int i=0;i<MAX_OPEN;i++){
		RelCacheTable::relCache[i] = nullptr;
		AttrCacheTable::attrCache[i] = nullptr;
	}

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

	int i=2,l=RelCacheTable::relCache[RELCAT_RELID]->relCatEntry.numSlotsPerBlk;
	while(i<l){
		relCatBlock.getRecord(relCatRecord, i);
		if(strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,"Students")==0){
			break;
		}
		i++;
	}
	if(i<l){
		RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
		relCacheEntry.recId.block = RELCAT_BLOCK;
		relCacheEntry.recId.slot = i;
		RelCacheTable::relCache[2] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	       *(RelCacheTable::relCache[2]) = relCacheEntry;	
	}

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

	prev = head = nullptr;
	for(int i=12;i<l;i++){
		attrCatBlock.getRecord(attrCatRecord, i);
                if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,"Students")!=0)continue;
		AttrCacheEntry* curr=(struct AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
                AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
                curr->recId.block = ATTRCAT_BLOCK;
                curr->recId.slot = i;
                if(head==nullptr)head = curr;
                else prev->next = curr;
                prev = curr;
        }
        prev->next = nullptr;
	AttrCacheTable::attrCache[2] = head;
}

OpenRelTable::~OpenRelTable(){
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
	if(strcmp(relName, RELCAT_RELNAME)==0)return RELCAT_RELID;
	if(strcmp(relName, ATTRCAT_RELNAME)==0)return ATTRCAT_RELID;
	if(strcmp(relName, "Students")==0)return 2;
	return E_RELNOTOPEN;
}
