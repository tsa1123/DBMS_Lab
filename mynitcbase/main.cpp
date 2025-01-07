#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include "Buffer/BlockBuffer.h"
using namespace std;
int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  Disk disk_run;
  // StaticBuffer buffer;
  // OpenRelTable cache;
  RecBuffer relCatBuffer(RELCAT_BLOCK);

  HeadInfo relCatHeader,attrCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  for(int i=0;i<relCatHeader.numEntries;i++){
  	Attribute relCatRecord[RELCAT_NO_ATTRS];
	relCatBuffer.getRecord(relCatRecord,i);

	printf("Relation: %s\n",relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
	
	double numAttrs = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	int32_t attrCatBlockNo=ATTRCAT_BLOCK;

	while(numAttrs>0){
		RecBuffer attrCatBuffer(attrCatBlockNo);
		attrCatBuffer.getHeader(&attrCatHeader);
		
		for(int k=0;k<attrCatHeader.numEntries;k++){
			attrCatBuffer.getRecord(attrCatRecord,k);
			if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal)==0){
				const char* attrType=(attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal==NUMBER)? "NUM" : "STR";
				printf(" %s: %s\n",attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,attrType);
				numAttrs--;
				if(numAttrs==0)break;
			}
		}
		attrCatBlockNo=attrCatHeader.rblock;
	}
	printf("\n");
  }

  printf("Exercise 2:\n");
  printf("============\n");
  
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  int32_t attrCatBlockNo=ATTRCAT_BLOCK;
  int flag=0;
  while(attrCatBlockNo!=-1 && flag==0){
	  RecBuffer attrCatBuffer(attrCatBlockNo);
          attrCatBuffer.getHeader(&attrCatHeader);

          for(int k=0;k<attrCatHeader.numEntries;k++){
		  attrCatBuffer.getRecord(attrCatRecord,k);
                  if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,"Students")==0){
		  	if(strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,"Class")==0){
                        	unsigned char buffer[BLOCK_SIZE];
				Disk::readBlock(buffer, attrCatBlockNo);

				unsigned char* location = buffer + HEADER_SIZE + attrCatHeader.numSlots + (attrCatHeader.numAttrs * ATTR_SIZE * k) + (ATTRCAT_ATTR_NAME_INDEX * ATTR_SIZE);
				memcpy(location, "Batch", 6);
				Disk::writeBlock(buffer, attrCatBlockNo);
				flag=1;
				printf("[+]Schema Modified\n");
				break;
			}
                  }
          }
          attrCatBlockNo=attrCatHeader.rblock;
  }
  if(flag==0)printf("[+]Attribute \'Class\' not Found!\n");

  attrCatBlockNo=ATTRCAT_BLOCK;
  while(attrCatBlockNo!=-1 ){
          RecBuffer attrCatBuffer(attrCatBlockNo);
          attrCatBuffer.getHeader(&attrCatHeader);
                
          for(int k=0;k<attrCatHeader.numEntries;k++){
                  attrCatBuffer.getRecord(attrCatRecord,k);
                  if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,"Students")==0){
			  const char* attrType=(attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal==NUMBER)? "NUM" : "STR";
                          printf(" %s: %s\n",attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,attrType);
                  }
          }
          attrCatBlockNo=attrCatHeader.rblock;
  }
  //return FrontendInterface::handleFrontend(argc, argv);
}
