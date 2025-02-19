#include "Algebra.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool isNumber(char *str){
	int len;
	float ignore;

	int ret = sscanf(str, "%f %n", &ignore, &len);
	return ret==1 && len==strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
	if(strcmp(relName, RELCAT_RELNAME)==0 || strcmp(relName, ATTRCAT_RELNAME)==0)return E_NOTPERMITTED;

	int relId = OpenRelTable::getRelId(relName);
	if(relId == E_RELNOTOPEN)return E_RELNOTOPEN;

	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	if(relCatEntry.numAttrs != nAttrs){
		return E_NATTRMISMATCH;
	}

	Attribute recordValues[nAttrs];
	AttrCatEntry attrCatEntry;
	for(int i=0; i<nAttrs; i++){
		AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
		int type = attrCatEntry.attrType;
		
		if(type == NUMBER){
			if(isNumber(record[i])){
				recordValues[i].nVal = atof(record[i]);
			}
			else{
				return E_ATTRTYPEMISMATCH;
			}
		}
		else{
			strcpy(recordValues[i].sVal, record[i]);
		}
	}

	int retVal = BlockAccess::insert(relId, recordValues);

	return retVal;
}

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if( srcRelId == E_RELNOTOPEN )return E_RELNOTOPEN;

	AttrCatEntry attrCatEntry;

	int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
	if(ret == E_ATTRNOTEXIST)return E_ATTRNOTEXIST;

	int type = attrCatEntry.attrType;
	Attribute attrVal;
	if(type == NUMBER){
		if(isNumber(strVal)) attrVal.nVal = atof(strVal);
		else return E_ATTRTYPEMISMATCH;
	}
	else if(type == STRING){
		strcpy(attrVal.sVal, strVal);
	}
	
	RelCacheTable::resetSearchIndex(srcRelId);
	RelCatEntry relCatEntry;

	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
	
	int typelist[relCatEntry.numAttrs];
	printf("|");
	for(int i=0; i<relCatEntry.numAttrs; i++){
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
		typelist[i] = attrCatEntry.attrType;
		printf(" %-16s|",attrCatEntry.attrName);
	}
	printf("\n");

	Attribute record[relCatEntry.numAttrs];
	while(true){
		RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);
		if(searchRes.block != -1 && searchRes.slot != -1){
			RecBuffer recBuffer(searchRes.block);
			recBuffer.getRecord(record, searchRes.slot);
			
			printf("|");
			for(int i=0; i<relCatEntry.numAttrs; i++){
				if(typelist[i] == STRING){
					printf(" %-16s|",record[i].sVal);
				}
				else printf(" %-16f|",record[i].nVal);
			}
			printf("\n");
		}
		else break;		
	}
	return SUCCESS;
}

