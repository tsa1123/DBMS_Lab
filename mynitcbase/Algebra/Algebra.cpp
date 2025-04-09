extern int comparisons;

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
	
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
	int src_nAttrs = relCatEntry.numAttrs;

	char attr_names[src_nAttrs][ATTR_SIZE];
	int attr_types[src_nAttrs];

	for(int i=0; i<src_nAttrs; i++){
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
		attr_types[i] = attrCatEntry.attrType;
		strcpy(attr_names[i], attrCatEntry.attrName);
	}

	ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
	if(ret!=SUCCESS)return ret;

	int targetRelId = OpenRelTable::openRel(targetRel);
	if(targetRelId<0 || targetRelId>=MAX_OPEN){
		Schema::deleteRel(targetRel);
		return targetRelId;
	}

	Attribute record[src_nAttrs];
        RelCacheTable::resetSearchIndex(srcRelId);
	AttrCacheTable::resetSearchIndex(srcRelId, attr);

	comparisons=0;

	while(BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS){
		ret = BlockAccess::insert(targetRelId, record);
		if(ret!=SUCCESS){
			Schema::closeRel(targetRel);
			Schema::deleteRel(targetRel);
			return ret;
		}
	}

	Schema::closeRel(targetRel);
	printf("%d comparisons made\n",comparisons);
	return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if(srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;

	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

	int src_nAttrs = relCatEntry.numAttrs;

	int attr_offset[tar_nAttrs];
	int attr_types[tar_nAttrs];

	for(int i=0; i<tar_nAttrs; i++){
		AttrCatEntry attrCatEntry;
		if(AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry) == E_ATTRNOTEXIST){
			return E_ATTRNOTEXIST;
		}
		attr_offset[i] = attrCatEntry.offset;
		attr_types[i] = attrCatEntry.attrType;
	}

	int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
	if(ret!=SUCCESS)return ret;

	int targetRelId = OpenRelTable::openRel(targetRel);
        if(targetRelId<0 || targetRelId>=MAX_OPEN){
                Schema::deleteRel(targetRel);
                return targetRelId;
        }

        Attribute record[src_nAttrs];
	Attribute proj_record[tar_nAttrs];
        RelCacheTable::resetSearchIndex(srcRelId);
	
        while(BlockAccess::project(srcRelId, record) == SUCCESS){
                for(int attr_iter=0; attr_iter<tar_nAttrs; attr_iter++){
			proj_record[attr_iter] = record[attr_offset[attr_iter]];
		}

		ret = BlockAccess::insert(targetRelId, proj_record);
                if(ret!=SUCCESS){
                        Schema::closeRel(targetRel);
                        Schema::deleteRel(targetRel);
                        return ret;
                }
        }

        Schema::closeRel(targetRel);
	return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]){
	int srcRelId = OpenRelTable::getRelId(srcRel);
	if(srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;
	
	RelCatEntry relCatEntry;
	RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
	int numAttrs = relCatEntry.numAttrs;
	
	char attrNames[numAttrs][ATTR_SIZE];
	int attrTypes[numAttrs];
	
	for(int i=0; i<numAttrs; i++){
		AttrCatEntry attrCatEntry;
		AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
		strcpy(attrNames[i], attrCatEntry.attrName);
		attrTypes[i] = attrCatEntry.attrType;
	}	

	int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
	if(ret!=SUCCESS){
		return ret;
	}
	
	int targetRelId = OpenRelTable::openRel(targetRel);
        if(targetRelId<0 || targetRelId>=MAX_OPEN){
                Schema::deleteRel(targetRel);
                return targetRelId;
        }

        Attribute record[numAttrs];
        RelCacheTable::resetSearchIndex(srcRelId);

        while(BlockAccess::project(srcRelId, record) == SUCCESS){
                ret = BlockAccess::insert(targetRelId, record);
                if(ret!=SUCCESS){
                        Schema::closeRel(targetRel);
                        Schema::deleteRel(targetRel);
                        return ret;
                }
        }

        Schema::closeRel(targetRel);

	return SUCCESS;
}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]){
	int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
	int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

	if(srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN){
		return E_RELNOTOPEN;
	}

	AttrCatEntry attrCatEntry1, attrCatEntry2;
	if(AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1)!=SUCCESS || AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2)!=SUCCESS){
		return E_ATTRNOTEXIST;
	}

	if(attrCatEntry1.attrType != attrCatEntry2.attrType){
		return E_ATTRTYPEMISMATCH;
	}

	RelCatEntry relCatEntry1, relCatEntry2;
	RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
	RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);

	int numOfAttributes1 = relCatEntry1.numAttrs;
	int numOfAttributes2 = relCatEntry2.numAttrs;

	for(int i=0; i<numOfAttributes1; i++){
		AttrCatEntry attrCatEntryA1;
                AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntryA1);
		for(int j=0; j<numOfAttributes2; j++){
			if(i==attrCatEntry1.offset && j==attrCatEntry2.offset)continue;
			AttrCatEntry attrCatEntryA2;
			AttrCacheTable::getAttrCatEntry(srcRelId2, j, &attrCatEntryA2);

			if(strcmp(attrCatEntryA1.attrName, attrCatEntryA2.attrName)==0){
				return E_DUPLICATEATTR;
			}
		}
	}

	if(attrCatEntry2.rootBlock == -1){
		int retVal = BPlusTree::bPlusCreate(srcRelId2, attribute2);
		if(retVal!=SUCCESS){
			return retVal;
		}
	}
	int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;

	char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
	int targetRelAttrTypes[numOfAttributesInTarget];

	AttrCatEntry attrCatEntry;
	for(int i=0; i<numOfAttributes1; i++){
		AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntry);
		strcpy(targetRelAttrNames[i], attrCatEntry.attrName);
		targetRelAttrTypes[i] = attrCatEntry.attrType;
	}
	for(int i=0, j=numOfAttributes1; i<numOfAttributes2; i++){
		if(i==attrCatEntry2.offset)continue;

                AttrCacheTable::getAttrCatEntry(srcRelId2, i, &attrCatEntry);
                strcpy(targetRelAttrNames[j], attrCatEntry.attrName);
                targetRelAttrTypes[j] = attrCatEntry.attrType;
		
		j++;
        }

	int retVal = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
	if(retVal != SUCCESS){
		return retVal;
	}

	int targetRelId = OpenRelTable::openRel(targetRelation);
	if(targetRelId<0 || targetRelId>=MAX_OPEN){
		Schema::deleteRel(targetRelation);
		return targetRelId;
	}

	Attribute record1[numOfAttributes1];
	Attribute record2[numOfAttributes2];
	Attribute targetRecord[numOfAttributesInTarget];
	
	RelCacheTable::resetSearchIndex(srcRelId1);
	while(BlockAccess::project(srcRelId1, record1) == SUCCESS){
		RelCacheTable::resetSearchIndex(srcRelId2);
		AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

		while(BlockAccess::search(srcRelId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS){
			memcpy(targetRecord, record1, numOfAttributes1*ATTR_SIZE);
			for(int i=0, j=numOfAttributes1; i<numOfAttributes2; i++){
				if(i == attrCatEntry2.offset)continue;
				memcpy(&targetRecord[j], &record2[i], ATTR_SIZE);
				j++;
			}

			retVal = BlockAccess::insert(targetRelId, targetRecord);
			if(retVal != SUCCESS){
				OpenRelTable::closeRel(targetRelId);
				Schema::deleteRel(targetRelation);

				return E_DISKFULL;
			}
		}
	}

	OpenRelTable::closeRel(targetRelId);
	return SUCCESS;
}
